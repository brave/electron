// Copyright (c) 2014 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "atom/renderer/extensions/atom_extensions_dispatcher_delegate.h"

#include <set>
#include <string>
#include "atom/common/javascript_bindings.h"
#include "atom/grit/atom_resources.h"  // NOLINT: This file is generated
#include "atom/grit/electron_api_resources.h"  // NOLINT: This file is generated
#include "base/win/windows_version.h"
#include "brave/grit/brave_resources.h"  // NOLINT: This file is generated
#include "brave/renderer/extensions/content_settings_bindings.h"
#include "brave/renderer/extensions/web_frame_bindings.h"
#include "chrome/grit/renderer_resources.h"  // NOLINT: This file is generated
#include "chrome/renderer/extensions/tabs_custom_bindings.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "extensions/common/extension.h"
#include "extensions/renderer/css_native_handler.h"
#include "extensions/renderer/i18n_custom_bindings.h"
#include "extensions/renderer/lazy_background_page_native_handler.h"
#include "extensions/renderer/resource_bundle_source_map.h"
#include "extensions/renderer/v8_helpers.h"
#include "gin/converter.h"
#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace extensions {

namespace {

#if defined(OS_MACOSX)
const char* kPlatform = "darwin";
#elif defined(OS_LINUX)
const char* kPlatform = "linux";
#elif defined(OS_WIN)
const char* kPlatform = "win32";
#else
const char* kPlatform = "unknown";
#endif

v8::Local<v8::Value> GetOrCreateProcess(ScriptContext* context) {
  v8::Local<v8::String> process_string(
      v8::String::NewFromUtf8(context->isolate(), "process"));
  v8::Local<v8::Object> global(context->v8_context()->Global());
  v8::Local<v8::Value> process(global->Get(process_string));
  if (process->IsUndefined()) {
    process = v8::Object::New(context->isolate());
    gin::SetProperty(context->isolate(), process.As<v8::Object>(),
        v8_helpers::ToV8StringUnsafe(context->isolate(), "type"),
        v8_helpers::ToV8StringUnsafe(context->isolate(), "renderer"));
    gin::SetProperty(context->isolate(), process.As<v8::Object>(),
        v8_helpers::ToV8StringUnsafe(context->isolate(), "platform"),
        v8_helpers::ToV8StringUnsafe(context->isolate(), kPlatform));
#if defined(OS_WIN)
    switch (base::win::GetVersion()) {
      case base::win::VERSION_WIN7:
        gin::SetProperty(context->isolate(), process.As<v8::Object>(),
          v8_helpers::ToV8StringUnsafe(context->isolate(), "platformVersion"),
          v8_helpers::ToV8StringUnsafe(context->isolate(), "win7"));
          break;
      case base::win::VERSION_WIN8:
        gin::SetProperty(context->isolate(), process.As<v8::Object>(),
          v8_helpers::ToV8StringUnsafe(context->isolate(), "platformVersion"),
          v8_helpers::ToV8StringUnsafe(context->isolate(), "win8"));
          break;
      case base::win::VERSION_WIN8_1:
        gin::SetProperty(context->isolate(), process.As<v8::Object>(),
          v8_helpers::ToV8StringUnsafe(context->isolate(), "platformVersion"),
          v8_helpers::ToV8StringUnsafe(context->isolate(), "win8_1"));
          break;
      case base::win::VERSION_WIN10:
        gin::SetProperty(context->isolate(), process.As<v8::Object>(),
          v8_helpers::ToV8StringUnsafe(context->isolate(), "platformVersion"),
          v8_helpers::ToV8StringUnsafe(context->isolate(), "win10"));
          break;
      case base::win::VERSION_WIN10_TH2:
        gin::SetProperty(context->isolate(), process.As<v8::Object>(),
          v8_helpers::ToV8StringUnsafe(context->isolate(), "platformVersion"),
          v8_helpers::ToV8StringUnsafe(context->isolate(), "win10_th2"));
          break;
      default:
        gin::SetProperty(context->isolate(), process.As<v8::Object>(),
          v8_helpers::ToV8StringUnsafe(context->isolate(), "platformVersion"),
          v8_helpers::ToV8StringUnsafe(context->isolate(), ""));
        break;
    }
#else
    gin::SetProperty(context->isolate(), process.As<v8::Object>(),
        v8_helpers::ToV8StringUnsafe(context->isolate(), "platformVersion"),
        v8_helpers::ToV8StringUnsafe(context->isolate(), ""));
#endif
    // TODO(bridiver) - add a function to return env vars
    // std::unique_ptr<base::Environment> env(base::Environment::Create());
    // gin::SetProperty(context->isolate(), process.As<v8::Object>(),
    //     v8_helpers::ToV8StringUnsafe(context->isolate(), "env"),
    //     v8_helpers::ToV8StringUnsafe(context->isolate(), "TODO"));
    global->Set(process_string, process);
  }

  return process;
}

// Returns |value| cast to an object if possible, else an empty handle.
v8::Local<v8::Object> AsObjectOrEmpty(v8::Local<v8::Value> value) {
  return value->IsObject() ? value.As<v8::Object>() : v8::Local<v8::Object>();
}

}

AtomExtensionsDispatcherDelegate::AtomExtensionsDispatcherDelegate() {
}

AtomExtensionsDispatcherDelegate::~AtomExtensionsDispatcherDelegate() {
}

void AtomExtensionsDispatcherDelegate::InitOriginPermissions(
    const extensions::Extension* extension,
    bool is_extension_active) {
}

void AtomExtensionsDispatcherDelegate::RegisterNativeHandlers(
    extensions::Dispatcher* dispatcher,
    extensions::ModuleSystem* module_system,
    extensions::ScriptContext* context) {
  module_system->RegisterNativeHandler(
      "atom",
      std::unique_ptr<NativeHandler>(
          new atom::JavascriptBindings(
              context->GetRenderFrame()->GetRenderView(), context)));
  module_system->RegisterNativeHandler(
      "contentSettings",
      std::unique_ptr<NativeHandler>(
          new brave::ContentSettingsBindings(context)));
  module_system->RegisterNativeHandler(
      "webFrame",
      std::unique_ptr<NativeHandler>(
          new brave::WebFrameBindings(context)));
  module_system->RegisterNativeHandler(
      "tabs",
      std::unique_ptr<NativeHandler>(
          new extensions::TabsCustomBindings(context)));

  // The following are native handlers that are defined in //extensions, but
  // are only used for APIs defined in Chrome.
  // See chrome/renderer/extensions/chrome_extensions_dispatcher_delegate.cc
  module_system->RegisterNativeHandler(
      "i18n", std::unique_ptr<NativeHandler>(
                  new extensions::I18NCustomBindings(context)));
  module_system->RegisterNativeHandler(
      "lazy_background_page",
      std::unique_ptr<NativeHandler>(
          new extensions::LazyBackgroundPageNativeHandler(context)));
  module_system->RegisterNativeHandler(
      "css_natives", std::unique_ptr<NativeHandler>(
                         new extensions::CssNativeHandler(context)));

}

void AtomExtensionsDispatcherDelegate::PopulateSourceMap(
    extensions::ResourceBundleSourceMap* source_map) {
  // TODO(bridiver) - add a permission for these
  // so only component extensions can use
  // blessed extension??
  source_map->RegisterSource("event_emitter", IDR_ATOM_EVENT_EMITTER_JS);
  source_map->RegisterSource("ipcRenderer", IDR_BRAVE_IPC_RENDERER_JS);
  source_map->RegisterSource("ipc_utils", IDR_ATOM_IPC_INTERNAL_JS);
  source_map->RegisterSource("webFrame",
                             IDR_ATOM_WEB_FRAME_BINDINGS_JS);
  source_map->RegisterSource("remote",
                              IDR_ELECTRON_REMOTE_BINDINGS_JS);
  source_map->RegisterSource("buffer",
                              IDR_ELECTRON_BUFFER_BINDINGS_JS);
  source_map->RegisterSource("is-promise",
                              IDR_ELECTRON_IS_PROMISE_BINDINGS_JS);
  source_map->RegisterSource("callbacks-registry",
                              IDR_ELECTRON_CALLBACKS_REGISTRY_BINDINGS_JS);
  source_map->RegisterSource("guest-view-internal",
                              IDR_ELECTRON_GUEST_VIEW_INTERNAL_BINDINGS_JS);
  source_map->RegisterSource("browserAction",
                             IDR_ATOM_BROWSER_ACTION_BINDINGS_JS);
  source_map->RegisterSource("permissions", IDR_ATOM_PERMISSIONS_BINDINGS_JS);
  source_map->RegisterSource("privacy", IDR_ATOM_PRIVACY_BINDINGS_JS);
  source_map->RegisterSource("tabs",
                             IDR_ATOM_TABS_BINDINGS_JS);
  source_map->RegisterSource("contextMenus",
                             IDR_ATOM_CONTEXT_MENUS_BINDINGS_JS);
  source_map->RegisterSource("contentSettings",
                             IDR_ATOM_CONTENT_SETTINGS_BINDINGS_JS);
  source_map->RegisterSource("windows",
                             IDR_ATOM_WINDOWS_BINDINGS_JS);
  source_map->RegisterSource("ChromeSetting", IDR_CHROME_SETTING_JS);
  source_map->RegisterSource("ContentSetting", IDR_CONTENT_SETTING_JS);
  source_map->RegisterSource("ChromeDirectSetting",
                             IDR_CHROME_DIRECT_SETTING_JS);
  source_map->RegisterSource("webViewInternal",
      IDR_ATOM_WEB_VIEW_INTERNAL_BINDINGS_JS);
  source_map->RegisterSource("tabViewInternal",
      IDR_ATOM_TAB_VIEW_INTERNAL_BINDINGS_JS);
  source_map->RegisterSource("webViewApiMethods",
      IDR_ATOM_WEB_VIEW_API_BINDINGS_JS);
}

void AtomExtensionsDispatcherDelegate::RequireAdditionalModules(
    extensions::ScriptContext* context,
    bool is_within_platform_app) {
  extensions::ModuleSystem* module_system = context->module_system();
  extensions::Feature::Context context_type = context->context_type();

  if (context_type == extensions::Feature::WEBUI_CONTEXT) {
    module_system->Require("webView");
    module_system->Require("tabViewInternal");
    module_system->Require("webViewInternal");
    module_system->Require("webViewApiMethods");
    module_system->Require("webViewAttributes");
  }

  if (context_type == extensions::Feature::WEBUI_CONTEXT ||
      (context->extension() && context->extension()->is_extension() &&
        Manifest::IsComponentLocation(context->extension()->location()))) {
    v8::Local<v8::Object> process =
        AsObjectOrEmpty(GetOrCreateProcess(context));
  }
}

void AtomExtensionsDispatcherDelegate::OnActiveExtensionsUpdated(
    const std::set<std::string>& extension_ids) {
}

}  // namespace extensions
