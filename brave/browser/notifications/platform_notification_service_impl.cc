// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE-CHROMIUM file.

#include "brave/browser/notifications/platform_notification_service_impl.h"

#include <memory>
#include <set>
#include <string>
#include <utility>

#include "brave/browser/brave_content_browser_client.h"
#include "brave/browser/brave_permission_manager.h"
#include "browser/notification.h"
#include "browser/notification_delegate.h"
#include "browser/notification_presenter.h"
#include "content/public/browser/permission_type.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/platform_notification_data.h"
#include "content/public/common/notification_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"


namespace brave {

namespace {

void OnPermissionResponse(const base::Callback<void(bool)>& callback,
                          blink::mojom::PermissionStatus status) {
  if (status == blink::mojom::PermissionStatus::GRANTED)
    callback.Run(true);
  else
    callback.Run(false);
}

void OnWebNotificationAllowed(
    brightray::BrowserClient* browser_client,
    const SkBitmap& icon,
    const content::PlatformNotificationData& data,
    brightray::NotificationDelegate* delegate,
    bool allowed) {
  if (!allowed)
    return;
  auto presenter = browser_client->GetNotificationPresenter();
  if (!presenter)
    return;
  auto notification = presenter->CreateNotification(delegate);
  if (notification) {
    notification->Show(data.title, data.body, data.tag,
        data.icon, icon, data.silent);
  }
}

}  // namespace

// static
PlatformNotificationServiceImpl*
PlatformNotificationServiceImpl::GetInstance() {
  return base::Singleton<PlatformNotificationServiceImpl>::get();
}

PlatformNotificationServiceImpl::PlatformNotificationServiceImpl() {}

PlatformNotificationServiceImpl::~PlatformNotificationServiceImpl() {}

void PlatformNotificationServiceImpl::DisplayNotification(
    content::BrowserContext* browser_context,
    const std::string& notification_id,
    const GURL& origin,
    const content::PlatformNotificationData& notification_data,
    const content::NotificationResources& notification_resources) {
  brightray::NotificationDelegate* delegate =
      new brightray::NotificationDelegate(notification_id);
  auto callback = base::Bind(&OnWebNotificationAllowed,
             BraveContentBrowserClient::Get(),
             notification_resources.notification_icon,
             notification_data,
             base::Passed(&delegate));

  auto permission_manager = browser_context->GetPermissionControllerDelegate();
  // TODO(bridiver) user gesture
  permission_manager->RequestPermission(
      content::PermissionType::NOTIFICATIONS, NULL, origin, false,
        base::Bind(&OnPermissionResponse, callback));
}

void PlatformNotificationServiceImpl::DisplayPersistentNotification(
    content::BrowserContext* browser_context,
    const std::string& notification_id,
    const GURL& service_worker_origin,
    const GURL& origin,
    const content::PlatformNotificationData& notification_data,
    const content::NotificationResources& notification_resources) {
  DisplayNotification(browser_context, notification_id, origin,
                      notification_data, notification_resources);
}

void PlatformNotificationServiceImpl::CloseNotification(
    content::BrowserContext* browser_context,
    const std::string& notification_id) {
  auto presenter =
    BraveContentBrowserClient::Get()->GetNotificationPresenter();
  if (!presenter)
    return;
  auto notification = presenter->lookupNotification(notification_id);
  if (!notification)
    return;
  notification->Dismiss();
}

void PlatformNotificationServiceImpl::ClosePersistentNotification(
    content::BrowserContext* browser_context,
    const std::string& notification_id) {
  CloseNotification(browser_context, notification_id);
}

void PlatformNotificationServiceImpl::GetDisplayedNotifications(
    content::BrowserContext* browser_context,
    const DisplayedNotificationsCallback& callback) {
}

int64_t PlatformNotificationServiceImpl::ReadNextPersistentNotificationId(
    content::BrowserContext* browser_context) {
  return -1;
}

}  // namespace brave
