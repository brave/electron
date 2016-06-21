// Copyright (c) 2013 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_EXTENSIONS_CONTENT_RENDERER_CLIENT_H_
#define EXTENSIONS_RENDERER_EXTENSIONS_CONTENT_RENDERER_CLIENT_H_

#include "atom/renderer/atom_renderer_client.h"

namespace atom {
class ContentSettingsManager;
}

namespace extensions {

class ExtensionsContentRendererClient : public atom::AtomRendererClient {
 public:
  ExtensionsContentRendererClient();

  // content::ContentRendererClient:
  void RenderThreadStarted() override;
  void RenderFrameCreated(content::RenderFrame*) override;
  void RenderViewCreated(content::RenderView*) override;
  void RunScriptsAtDocumentStart(content::RenderFrame* render_frame) override;
  void RunScriptsAtDocumentEnd(content::RenderFrame* render_frame) override;
  bool AllowPopup() override;
  bool ShouldFork(blink::WebLocalFrame* frame,
                  const GURL& url,
                  const std::string& http_method,
                  bool is_initial_navigation,
                  bool is_server_redirect,
                  bool* send_referrer) override;
  void DidInitializeServiceWorkerContextOnWorkerThread(
      v8::Local<v8::Context> context,
      const GURL& url) override;
  void WillDestroyServiceWorkerContextOnWorkerThread(
      v8::Local<v8::Context> context,
      const GURL& url) override;

  bool WillSendRequest(
    blink::WebFrame* frame,
    ui::PageTransition transition_type,
    const GURL& url,
    const GURL& first_party_for_cookies,
    GURL* new_url) override;

 private:
  std::unique_ptr<atom::ContentSettingsManager> content_settings_manager_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionsContentRendererClient);
};

}  // namespace atom

#endif  // EXTENSIONS_RENDERER_EXTENSIONS_CONTENT_RENDERER_CLIENT_H_
