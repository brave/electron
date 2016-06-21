// Copyright (c) 2013 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "atom/browser/atom_browser_context.h"

#include "atom/browser/atom_browser_main_parts.h"
#include "atom/browser/atom_download_manager_delegate.h"
#include "atom/browser/browser.h"
#include "atom/browser/net/atom_cert_verifier.h"
#include "atom/browser/net/atom_network_delegate.h"
#include "atom/browser/net/atom_ssl_config_service.h"
#include "atom/browser/net/atom_url_request_job_factory.h"
#include "atom/browser/net/asar/asar_protocol_handler.h"
#include "atom/browser/net/http_protocol_handler.h"
#include "atom/browser/atom_permission_manager.h"
#include "atom/browser/web_view_manager.h"
#include "atom/common/atom_version.h"
#include "atom/common/chrome_version.h"
#include "atom/common/options_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "components/prefs/pref_registry_simple.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/worker_pool.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/user_agent.h"
#include "net/ftp/ftp_network_layer.h"
#include "net/url_request/data_protocol_handler.h"
#include "net/url_request/ftp_protocol_handler.h"
#include "net/url_request/url_request_intercepting_job_factory.h"
#include "net/url_request/url_request_context.h"
#include "url/url_constants.h"

#if defined(ENABLE_EXTENSIONS)
#include "atom/browser/extensions/atom_browser_client_extensions_part.h"
#include "atom/browser/extensions/atom_extension_system_factory.h"
#include "atom/browser/extensions/atom_extensions_network_delegate.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_filter.h"
#include "chrome/browser/chrome_notification_types.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/syncable_prefs/pref_service_syncable.h"
#include "components/syncable_prefs/pref_service_syncable_factory.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "extensions/browser/extension_pref_store.h"
#include "extensions/browser/extension_pref_value_map_factory.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_protocols.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extensions_browser_client.h"
#endif

using content::BrowserThread;

namespace atom {

namespace {

class NoCacheBackend : public net::HttpCache::BackendFactory {
  int CreateBackend(net::NetLog* net_log,
                    std::unique_ptr<disk_cache::Backend>* backend,
                    const net::CompletionCallback& callback) override {
    return net::ERR_FAILED;
  }
};

std::string RemoveWhitespace(const std::string& str) {
  std::string trimmed;
  if (base::RemoveChars(str, " ", &trimmed))
    return trimmed;
  else
    return str;
}

}  // namespace

AtomBrowserContext::AtomBrowserContext(const std::string& partition,
                                       bool in_memory)
    : brightray::BrowserContext(partition, in_memory),
#if defined(ENABLE_EXTENSIONS)
      pref_registry_(new user_prefs::PrefRegistrySyncable),
#endif
      cert_verifier_(new AtomCertVerifier),
      job_factory_(new AtomURLRequestJobFactory),
#if defined(ENABLE_EXTENSIONS)
      network_delegate_(new extensions::AtomExtensionsNetworkDelegate(this)) {
#else
      network_delegate_(new AtomNetworkDelegate) {
#endif
  if (in_memory) {
    original_context_ = AtomBrowserContext::From(partition, false);
  }
}

AtomBrowserContext::~AtomBrowserContext() {
#if defined(ENABLE_EXTENSIONS)
  bool prefs_loaded = user_prefs_->GetInitializationStatus() !=
      PrefService::INITIALIZATION_STATUS_WAITING;

  if (prefs_loaded) {
    user_prefs_->CommitPendingWrite();
  }

  NotifyWillBeDestroyed(this);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PROFILE_DESTROYED,
      content::Source<AtomBrowserContext>(this),
      content::NotificationService::NoDetails());

  if (user_prefs_registrar_.get())
    user_prefs_registrar_->RemoveAll();

  BrowserContextDependencyManager::GetInstance()->
      DestroyBrowserContextServices(this);

#endif

}

net::NetworkDelegate* AtomBrowserContext::CreateNetworkDelegate() {
  return network_delegate_;
}

std::string AtomBrowserContext::GetUserAgent() {
  Browser* browser = Browser::Get();
  std::string name = RemoveWhitespace(browser->GetName());
  std::string user_agent;
  if (name == ATOM_PRODUCT_NAME) {
    user_agent = "Chrome/" CHROME_VERSION_STRING " "
                 ATOM_PRODUCT_NAME "/" ATOM_VERSION_STRING;
  } else {
    user_agent = base::StringPrintf(
        "%s/%s Chrome/%s " ATOM_PRODUCT_NAME "/" ATOM_VERSION_STRING,
        name.c_str(),
        browser->GetVersion().c_str(),
        CHROME_VERSION_STRING);
  }
  return content::BuildUserAgentFromProduct(user_agent);
}

std::unique_ptr<net::URLRequestJobFactory>
AtomBrowserContext::CreateURLRequestJobFactory(
    content::ProtocolHandlerMap* handlers,
    content::URLRequestInterceptorScopedVector* interceptors) {
  std::unique_ptr<AtomURLRequestJobFactory> job_factory(job_factory_);

  for (auto& it : *handlers) {
    job_factory->SetProtocolHandler(it.first,
                                    make_scoped_ptr(it.second.release()));
  }
  handlers->clear();

  job_factory->SetProtocolHandler(
      url::kDataScheme, make_scoped_ptr(new net::DataProtocolHandler));
  job_factory->SetProtocolHandler(
      url::kFileScheme, make_scoped_ptr(new asar::AsarProtocolHandler(
          BrowserThread::GetBlockingPool()->GetTaskRunnerWithShutdownBehavior(
              base::SequencedWorkerPool::SKIP_ON_SHUTDOWN))));
  job_factory->SetProtocolHandler(
      url::kHttpScheme,
      make_scoped_ptr(new HttpProtocolHandler(url::kHttpScheme)));
  job_factory->SetProtocolHandler(
      url::kHttpsScheme,
      make_scoped_ptr(new HttpProtocolHandler(url::kHttpsScheme)));
  job_factory->SetProtocolHandler(
      url::kWsScheme,
      make_scoped_ptr(new HttpProtocolHandler(url::kWsScheme)));
  job_factory->SetProtocolHandler(
      url::kWssScheme,
      make_scoped_ptr(new HttpProtocolHandler(url::kWssScheme)));
#if defined(ENABLE_EXTENSIONS)
  extensions::InfoMap* extension_info_map =
      extensions::AtomExtensionSystemFactory::GetInstance()->
        GetForBrowserContext(this)->info_map();
  job_factory->SetProtocolHandler(
      extensions::kExtensionScheme,
      extensions::CreateExtensionProtocolHandler(IsOffTheRecord(),
                                                 extension_info_map));
#endif

  auto host_resolver =
      url_request_context_getter()->GetURLRequestContext()->host_resolver();
  job_factory->SetProtocolHandler(
      url::kFtpScheme,
      make_scoped_ptr(new net::FtpProtocolHandler(
          new net::FtpNetworkLayer(host_resolver))));

  // Set up interceptors in the reverse order.
  std::unique_ptr<net::URLRequestJobFactory> top_job_factory =
      std::move(job_factory);
  content::URLRequestInterceptorScopedVector::reverse_iterator it;
  for (it = interceptors->rbegin(); it != interceptors->rend(); ++it)
    top_job_factory.reset(new net::URLRequestInterceptingJobFactory(
        std::move(top_job_factory), make_scoped_ptr(*it)));
  interceptors->weak_clear();

  return top_job_factory;
}

net::HttpCache::BackendFactory*
AtomBrowserContext::CreateHttpCacheBackendFactory(
    const base::FilePath& base_path) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDisableHttpCache))
    return new NoCacheBackend;
  else
    return brightray::BrowserContext::CreateHttpCacheBackendFactory(base_path);
}

content::DownloadManagerDelegate*
AtomBrowserContext::GetDownloadManagerDelegate() {
  if (!download_manager_delegate_.get()) {
    auto download_manager = content::BrowserContext::GetDownloadManager(this);
    download_manager_delegate_.reset(
        new AtomDownloadManagerDelegate(download_manager));
  }
  return download_manager_delegate_.get();
}

content::BrowserPluginGuestManager* AtomBrowserContext::GetGuestManager() {
  if (!guest_manager_)
    guest_manager_.reset(new WebViewManager);
  return guest_manager_.get();
}

content::PermissionManager* AtomBrowserContext::GetPermissionManager() {
  if (!permission_manager_.get())
    permission_manager_.reset(new AtomPermissionManager);
  return permission_manager_.get();
}

std::unique_ptr<net::CertVerifier> AtomBrowserContext::CreateCertVerifier() {
  return make_scoped_ptr(cert_verifier_);
}

net::SSLConfigService* AtomBrowserContext::CreateSSLConfigService() {
  return new AtomSSLConfigService;
}

void AtomBrowserContext::RegisterPrefs(PrefRegistrySimple* pref_registry) {
  pref_registry->RegisterFilePathPref(prefs::kSelectFileLastDirectory,
                                      base::FilePath());
  base::FilePath download_dir;
  PathService::Get(chrome::DIR_DEFAULT_DOWNLOADS, &download_dir);
  pref_registry->RegisterFilePathPref(prefs::kDownloadDefaultDirectory,
                                      download_dir);
  pref_registry->RegisterDictionaryPref(prefs::kDevToolsFileSystemPaths);
#if defined(ENABLE_EXTENSIONS)
  RegisterUserPrefs();
#endif
}

#if defined(ENABLE_EXTENSIONS)
void AtomBrowserContext::RegisterUserPrefs() {
  extensions::ExtensionPrefs::RegisterProfilePrefs(pref_registry_.get());

  BrowserContextDependencyManager::GetInstance()->
      RegisterProfilePrefsForServices(this, pref_registry_.get());

  extensions::AtomBrowserClientExtensionsPart::RegisteryProfilePrefs(
      pref_registry_.get());

  base::FilePath filepath = GetPath().Append(
      FILE_PATH_LITERAL("UserPrefs"));

  syncable_prefs::PrefServiceSyncableFactory factory;
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      JsonPrefStore::GetTaskRunnerForFile(
          filepath, BrowserThread::GetBlockingPool());
  scoped_refptr<JsonPrefStore> pref_store =
      new JsonPrefStore(filepath, task_runner, std::unique_ptr<PrefFilter>());

  bool async = false;
  factory.set_async(async);
  factory.set_user_prefs(pref_store);

  if (extensions::ExtensionsBrowserClient::Get()) {
    scoped_refptr<PrefStore> extension_prefs = new ExtensionPrefStore(
        ExtensionPrefValueMapFactory::GetForBrowserContext(this),
        false);
    factory.set_extension_prefs(extension_prefs);
  }
  user_prefs_ = factory.CreateSyncable(pref_registry_.get());

  user_prefs_registrar_.reset(new PrefChangeRegistrar());
  if (IsOffTheRecord()) {
    PrefStore* otr_extension_prefs = new ExtensionPrefStore(
        ExtensionPrefValueMapFactory::GetForBrowserContext(this),
        true);
    auto otr_user_prefs = user_prefs_->CreateIncognitoPrefService(
                              otr_extension_prefs, overlay_pref_names_);
    user_prefs_registrar_->Init(otr_user_prefs);
    user_prefs::UserPrefs::Set(this, otr_user_prefs);
  } else {
    user_prefs_registrar_->Init(user_prefs_.get());
    user_prefs::UserPrefs::Set(this, user_prefs_.get());
  }

  if (async) {
    user_prefs_->AddPrefInitObserver(base::Bind(
        &AtomBrowserContext::OnPrefsLoaded, base::Unretained(this)));
  } else {
    OnPrefsLoaded(true);
  }
}

void AtomBrowserContext::OnPrefsLoaded(bool success) {
  if (!success)
    return;

  BrowserContextDependencyManager::GetInstance()->
      CreateBrowserContextServices(this);

  if (extensions::ExtensionsBrowserClient::Get()) {
    extensions::ExtensionSystem::Get(this)->InitForRegularProfile(true);
  }

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PROFILE_CREATED,
      content::Source<AtomBrowserContext>(this),
      content::NotificationService::NoDetails());
}
#endif

}  // namespace atom

namespace brightray {

// static
scoped_refptr<BrowserContext> BrowserContext::Create(
    const std::string& partition, bool in_memory) {
  return make_scoped_refptr(new atom::AtomBrowserContext(partition, in_memory));
}

}  // namespace brightray
