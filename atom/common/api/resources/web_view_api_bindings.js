// Copyright 2014 The Brave Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module implements the public-facing API functions for the <webview> tag.

const GuestViewInternal = require('guest-view-internal').GuestViewInternal
const TabViewInternal = require('tabViewInternal').TabViewInternal
const WebViewInternal = require('webViewInternal').WebViewInternal
const WebViewImpl = require('webView').WebViewImpl
const GuestViewImpl = require('guestView').GuestViewImpl
const remote = require('remote')
const GuestViewContainer = require('guestViewContainer').GuestViewContainer
const GuestView = require('guestView').GuestView

const asyncMethods = [
  'loadURL',
  'stop',
  'reload',
  'undo',
  'redo',
  'cut',
  'copy',
  'pasteAndMatchStyle',
  'findInPage',
  'stopFindInPage',
  'downloadURL',
  'print',
  'showCertificate',
  'showDefinitionForSelection',
  'executeScriptInTab',
  'zoomIn',
  'zoomOut',
  'zoomReset',
  'enablePreferredSizeMode',
  'send',
  'getPreferredSize',
]

const syncMethods = [
  'getZoomPercent'
]

var WEB_VIEW_API_METHODS = [
  'attachGuest',
  'detachGuest',
  'getId',
  'getGuestId'
].concat(asyncMethods).concat(syncMethods)

asyncMethods.forEach((method) => {
  WebViewImpl.prototype[method] = function () {
    if (!this.tabID)
      return

    remote.callAsyncWebContentsFunction(this.tabID, method, arguments)
  }
})

syncMethods.forEach((method) => {
  WebViewImpl.prototype[method] = function () {
    if (!this.guest.getId())
      return

    if (!this.webContents_) {
      console.error('webContents is not available for: ' + method)
    } else {
      return this.webContents_[method].apply(this, arguments)
    }
  }
})

// -----------------------------------------------------------------------------
// Custom API method implementations.
const attachWindow = WebViewImpl.prototype.attachWindow$
WebViewImpl.prototype.attachWindow$ = async function (opt_guestInstanceId, webContents) {
  if (this.guest.getId() === opt_guestInstanceId &&
      this.guest.getState() === GuestViewImpl.GuestState.GUEST_STATE_ATTACHED) {
    return true
  }
  const guestInstanceId = opt_guestInstanceId || this.guest.getId()

  if (opt_guestInstanceId) {
    // Detach if attached so that attached contents is not destroyed
    // but never try to call detach on a destroyed contents guest,
    // as that request will never be fulfilled.
    // Also do not change this.guest if the guest has not finished detaching yet,
    // so wait for that to be completed.
    await this.detachGuest()
    this.guest = new GuestView('webview', guestInstanceId)
  }

  const attached = GuestViewContainer.prototype.attachWindow$.call(this)

  if (attached) {
    this.attachedGuestInstanceId = guestInstanceId
    this.attachedWebContentsOp = webContents
      ? Promise.resolve(webContents)
      : new Promise(resolve => {
          if (webContents) {
            resolve(webContents)
          } else {
            WebViewInternal.getWebContents(guestInstanceId, (remoteWebContents) => { 
              // are we still looking for this contents?
              // or are we detached / attached to something different
              if (this.attachedGuestInstanceId && this.attachedGuestInstanceId !== guestInstanceId) {
                if (this.attachedWebContentsOp) {
                  this.attachedWebContentsOp.then(resolve)
                  return
                }
                resolve(null)
              }
              resolve(remoteWebContents)
            })
          }
        })
    // backwards compat for sync methods
    this.attachedWebContentsOp.then(webContents => {
      this.webContents_ = webContents
    })
  }
  return attached
}

WebViewImpl.prototype.detachGuest = async function () {
  // only allow 1 guest detach / attach at a time
  if (this.detachOperation_) {
    return this.detachOperation_
  }
  // the instance properties can change whilst detaching via the attachGuest function
  const guestToDetach = this.guest
  const tabID = this.tabID
  const currentEventWrapper = this.currentEventWrapper
  let webContents
  if (this.attachedWebContentsOp) {
    webContents = await this.attachedWebContentsOp
  }
  // clear attach properties
  this.attachedWebContentsOp = null
  this.attachedGuestInstanceId = null
  // Perform detach or noop.
  // Do not attempt to call detach on a
  // destroyed or detached or detaching web contents.
  if (
    !webContents ||
    webContents.isDestroyed() ||
    !webContents.attached ||
    !this.guest ||
    this.guest.getState() !== GuestViewImpl.GuestState.GUEST_STATE_ATTACHED
  ) {
    // already detached, noop
    return Promise.resolve()
  }
  // perform detach
  // resolve when detached or destroyed
  this.detachOperation_ = new Promise(resolve => {
    // guest may destroy or detach instead of running callback
    if (webContents && !webContents.isDestroyed()) {
      webContents.addListener('will-destroy', resolve)
    }
    // try to detach
    guestToDetach.detach(() => {
      // detach successful so don't need to resolve on destroy
      if (webContents && !webContents.isDestroyed()) {
        webContents.removeListener('will-destroy', resolve)
      }
      resolve()
    })
  })
  // don't forward previously-attached tab events to this webview anymore
  .then(() => {
    this.detachOperation_ = null
    if (tabID && currentEventWrapper) {
      GuestViewInternal.removeListener(tabID, currentEventWrapper)
    }
  })
  return this.detachOperation_
}

WebViewImpl.prototype.attachGuest = function (guestInstanceId, webContents) {
    return this.attachWindow$(guestInstanceId, webContents)
}

WebViewImpl.prototype.eventWrapper = function (tabID, event) {
  if (event.type == 'destroyed') {
    GuestViewInternal.removeListener(tabID, this.currentEventWrapper)
  }
  this.dispatchEvent(event)
}

WebViewImpl.prototype.setTabId = function (tabID) {
  if (this.tabID) {
    GuestViewInternal.removeListener(this.tabID, this.currentEventWrapper)
  }
  this.tabID = tabID
  this.currentEventWrapper = this.eventWrapper.bind(this, tabID)
  GuestViewInternal.addListener(tabID, this.currentEventWrapper)
}

WebViewImpl.prototype.getId = function () {
  return this.tabID
}

WebViewImpl.prototype.getGuestId = function () {
  return this.guest.getId()
}

WebViewImpl.prototype.getURL = function () {
  return this.attributes[WebViewConstants.ATTRIBUTE_SRC]
}
// -----------------------------------------------------------------------------

WebViewImpl.getApiMethods = function () {
  return WEB_VIEW_API_METHODS
}
