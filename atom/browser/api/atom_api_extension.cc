// Copyright (c) 2015 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "atom/browser/api/atom_api_extension.h"

#include <string>
#include <vector>
#include "atom/browser/api/atom_api_web_contents.h"
#include "atom/browser/extensions/atom_extension_system.h"
#include "atom/browser/extensions/tab_helper.h"
#include "atom/common/native_mate_converters/file_path_converter.h"
#include "atom/common/native_mate_converters/string16_converter.h"
#include "atom/common/native_mate_converters/value_converter.h"
#include "atom/common/node_includes.h"
#include "base/files/file_path.h"
#include "base/strings/string_util.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/notification_types.h"
#include "extensions/common/extension.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/one_shot_event.h"
#include "native_mate/converter.h"
#include "native_mate/dictionary.h"

namespace mate {

template<>
struct Converter<extensions::Manifest::Location> {
  static bool FromV8(v8::Isolate* isolate, v8::Local<v8::Value> val,
                     extensions::Manifest::Location* out) {
    std::string type;
    if (!ConvertFromV8(isolate, val, &type))
      return false;

    if (type == "internal")
      *out = extensions::Manifest::Location::INTERNAL;
    else if (type == "external_pref")
      *out = extensions::Manifest::Location::EXTERNAL_PREF;
    else if (type == "external_registry")
      *out = extensions::Manifest::Location::EXTERNAL_REGISTRY;
    else if (type == "unpacked")
      *out = extensions::Manifest::Location::UNPACKED;
    else if (type == "component")
      *out = extensions::Manifest::Location::COMPONENT;
    else if (type == "external_pref_download")
      *out = extensions::Manifest::Location::EXTERNAL_PREF_DOWNLOAD;
    else if (type == "external_policy_download")
      *out = extensions::Manifest::Location::EXTERNAL_POLICY_DOWNLOAD;
    else if (type == "command_line")
      *out = extensions::Manifest::Location::COMMAND_LINE;
    else if (type == "external_policy")
      *out = extensions::Manifest::Location::EXTERNAL_POLICY;
    else if (type == "external_component")
      *out = extensions::Manifest::Location::EXTERNAL_COMPONENT;
    else
      *out = extensions::Manifest::Location::INVALID_LOCATION;
    return true;
  }
};

}  // namespace mate

namespace {

scoped_refptr<extensions::Extension> LoadExtension(const base::FilePath& path,
    const base::DictionaryValue& manifest,
    const extensions::Manifest::Location& manifest_location,
    int flags,
    std::string* error) {
  scoped_refptr<extensions::Extension> extension(extensions::Extension::Create(
      path, manifest_location, manifest, flags, error));
  if (!extension.get())
    return NULL;

  std::vector<extensions::InstallWarning> warnings;
  if (!extensions::file_util::ValidateExtension(extension.get(),
                                                error,
                                                &warnings))
    return NULL;
  extension->AddInstallWarnings(warnings);

  return extension;
}

}  // namespace

namespace atom {

namespace api {

Extension::Extension(v8::Isolate* isolate,
                 content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
  Init(isolate);
  extensions::ExtensionRegistry::Get(browser_context_)->AddObserver(this);
}

Extension::~Extension() {
  if (extensions::ExtensionRegistry::Get(browser_context_)) {
    extensions::ExtensionRegistry::Get(browser_context_)->RemoveObserver(this);
  }
}

void Extension::Load(mate::Arguments* args) {
  base::FilePath path;
  args->GetNext(&path);

  base::DictionaryValue manifest;
  args->GetNext(&manifest);

  extensions::Manifest::Location manifest_location =
      extensions::Manifest::Location::UNPACKED;
  args->GetNext(&manifest_location);

  int flags = 0;
  args->GetNext(&flags);

  std::string error;
  std::unique_ptr<base::DictionaryValue> manifest_copy =
      manifest.CreateDeepCopy();
  if (manifest_copy->empty()) {
    manifest_copy = extensions::file_util::LoadManifest(path, &error);
  }

  if (!error.empty()) {
    node::Environment* env = node::Environment::GetCurrent(isolate());
    mate::EmitEvent(isolate(),
                  env->process_object(),
                  "extension-load-error",
                  error);
  } else {
    scoped_refptr<extensions::Extension> extension = LoadExtension(path,
                              *manifest_copy,
                              manifest_location,
                              flags,
                              &error);

    if (!error.empty()) {
      node::Environment* env = node::Environment::GetCurrent(isolate());
      mate::EmitEvent(isolate(),
                  env->process_object(),
                  "extension-load-error",
                  error);
    } else {
      extensions::ExtensionSystem::Get(browser_context_)->ready().Post(
            FROM_HERE,
            base::Bind(&Extension::AddExtension,
              // GetWeakPtr()
              base::Unretained(this), base::Passed(&extension)));
    }
  }
}

void Extension::AddExtension(scoped_refptr<extensions::Extension> extension) {
  auto extension_service =
      extensions::ExtensionSystem::Get(browser_context_)->extension_service();
  extension_service->AddExtension(extension.get());
}

void Extension::OnExtensionReady(content::BrowserContext* browser_context,
                                const extensions::Extension* extension) {
  mate::Dictionary install_info = mate::Dictionary::CreateEmpty(isolate());
  install_info.Set("name", extension->non_localized_name());
  install_info.Set("id", extension->id());
  install_info.Set("url", extension->url().spec());
  install_info.Set("base_path", extension->path().value());
  install_info.Set("version", extension->VersionString());
  install_info.Set("description", extension->description());
  auto manifest = extension->manifest()->value()->CreateDeepCopy();
  install_info.Set("manifest", mate::ConvertToV8(isolate(), *manifest));
  node::Environment* env = node::Environment::GetCurrent(isolate());
  mate::EmitEvent(isolate(),
                  env->process_object(),
                  "EXTENSION_READY_INTERNAL",
                  install_info);
}

void Extension::OnExtensionUnloaded(content::BrowserContext* browser_context,
                            const extensions::Extension* extension,
                            extensions::UnloadedExtensionInfo::Reason reason) {
  node::Environment* env = node::Environment::GetCurrent(isolate());
  mate::EmitEvent(isolate(),
                  env->process_object(),
                  "extension-unloaded",
                  extension->id());
}

void Extension::Disable(const std::string& extension_id) {
  auto extension_service =
      extensions::ExtensionSystem::Get(browser_context_)->extension_service();
  if (extension_service) {
    extension_service->DisableExtension(
        extension_id, extensions::Extension::DISABLE_USER_ACTION);
  }
}

void Extension::Enable(const std::string& extension_id) {
  auto extension_service =
      extensions::ExtensionSystem::Get(browser_context_)->extension_service();
  if (extension_service) {
    extension_service->EnableExtension(
        extension_id);
  }
}

// static
bool Extension::IsBackgroundPageUrl(GURL url,
                    content::BrowserContext* browser_context) {
  if (url.scheme() != "chrome-extension")
    return false;

  if (extensions::ExtensionSystem::Get(browser_context)
      ->ready().is_signaled()) {
    const extensions::Extension* extension =
        extensions::ExtensionRegistry::Get(browser_context)->
            enabled_extensions().GetExtensionOrAppByURL(url);
    if (extension &&
        url == extensions::BackgroundInfo::GetBackgroundURL(extension))
      return true;
  }

  return false;
}

// static
bool Extension::IsBackgroundPageWebContents(
    content::WebContents* web_contents) {
  auto browser_context = web_contents->GetBrowserContext();
  auto url = web_contents->GetURL();

  return IsBackgroundPageUrl(url, browser_context);
}

// static
bool Extension::IsBackgroundPage(const WebContents* web_contents) {
  return IsBackgroundPageWebContents(web_contents->web_contents());
}

// static
v8::Local<v8::Value> Extension::TabValue(v8::Isolate* isolate,
                    WebContents* web_contents) {
  std::unique_ptr<base::DictionaryValue> value(
      extensions::TabHelper::CreateTabValue(web_contents->web_contents()));
  return mate::ConvertToV8(isolate, *value);
}

// static
bool Extension::HandleURLOverride(GURL* url,
        content::BrowserContext* browser_context) {
  return false;
}

bool Extension::HandleURLOverrideReverse(GURL* url,
          content::BrowserContext* browser_context) {
  return false;
}

// static
mate::Handle<Extension> Extension::Create(
    v8::Isolate* isolate,
    content::BrowserContext* browser_context) {
  return mate::CreateHandle(isolate, new Extension(isolate, browser_context));
}

// static
void Extension::BuildPrototype(v8::Isolate* isolate,
                              v8::Local<v8::FunctionTemplate> prototype) {
  prototype->SetClassName(mate::StringToV8(isolate, "Extension"));
  mate::ObjectTemplateBuilder(isolate, prototype->PrototypeTemplate())
    .SetMethod("load", &Extension::Load)
    .SetMethod("enable", &Extension::Enable)
    .SetMethod("disable", &Extension::Disable);
}

}  // namespace api

}  // namespace atom

namespace {

void Initialize(v8::Local<v8::Object> exports, v8::Local<v8::Value> unused,
                v8::Local<v8::Context> context, void* priv) {
  v8::Isolate* isolate = context->GetIsolate();
  mate::Dictionary dict(isolate, exports);
  dict.SetMethod("tabValue", &atom::api::Extension::TabValue);
  dict.SetMethod("isBackgroundPage", &atom::api::Extension::IsBackgroundPage);
}

}  // namespace

NODE_MODULE_CONTEXT_AWARE_BUILTIN(atom_browser_extension, Initialize)
