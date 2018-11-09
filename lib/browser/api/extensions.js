const extensions = process.atomBinding('extension')
const {app, BrowserWindow, ipcMain, webContents} = require('electron')
const path = require('path')
const browserActions = require('./browser-actions')

// List of currently active background pages by extensionId
var backgroundPages = {}

// List of events registered for background pages by extensionId
var backgroundPageEvents = {}

// List of current active tabs
var tabs = {}

var getResourceURL = function (extensionId, path) {
  path = String(path)
  if (!path.length || path[0] != '/')
    path = '/' + path
  return 'chrome-extension://' + extensionId + path
}

process.on('EXTENSION_READY_INTERNAL', (installInfo) => {
  process.emit('extension-ready', installInfo)
  let browserAction = installInfo.manifest.browser_action
  if (browserAction) {
    let details = {
      title: browserAction.default_title,
      path: browserAction.default_icon,
      popup: browserAction.default_popup
    }
    browserActions.setDefaultBrowserAction(installInfo.id, details)
    addBackgroundPageEvent(installInfo.id, 'chrome-browser-action-clicked')
    process.emit('chrome-browser-action-registered', installInfo.id, details)
  }
})

// TODO(bridiver) move these into modules
// background pages
var addBackgroundPage = function (extensionId, backgroundPage) {
  backgroundPages[extensionId] = backgroundPages[extensionId] || []
  backgroundPages[extensionId].push(backgroundPage)
  process.emit('background-page-loaded', extensionId, backgroundPage)
}

var addBackgroundPageEvent = function (extensionId, event) {
  backgroundPageEvents[event] = backgroundPageEvents[event] || []
  if (backgroundPageEvents[event].indexOf(extensionId) === -1) {
    backgroundPageEvents[event].push(extensionId)
  }
}

var sendToBackgroundPages = function (extensionId, session, event) {
  if (!backgroundPageEvents[event] || !session)
    return

  var pages = []
  if (extensionId === 'all') {
    pages = backgroundPageEvents[event].reduce(
      (curr, id) => curr = curr.concat(backgroundPages[id] || []), [])
  } else {
    pages = backgroundPages[extensionId] || []
  }

  var args = [].slice.call(arguments, 2)
  pages.forEach(function (backgroundPage) {
    try {
      // only send to background pages in the same browser context
      if (backgroundPage.session.equal(session)) {
        backgroundPage.send.apply(backgroundPage, args)
      }
    } catch (e) {
      console.error('Could not send to background page: ' + e)
    }
  })
}

var createBackgroundPage = function (webContents) {
  var extensionId = webContents.getURL().match(/chrome-extension:\/\/([^\/]*)/)[1]
  var id = webContents.id
  addBackgroundPage(extensionId, webContents)
  webContents.on('destroyed', function () {
    process.emit('background-page-destroyed', extensionId, id)
    var index = backgroundPages[extensionId].indexOf(webContents)
    if (index > -1) {
      backgroundPages[extensionId].splice(index, 1)
    }
  })
}

// chrome.tabs
var getSessionForTab = function (tabId) {
  let tab = tabs[tabId]
  if (!tab) {
    return
  }

  return tab.session
}

var getWebContentsForTab = function (tabId) {
  let tab = tabs[tabId]
  if (!tab) {
    return
  }

  return tab.webContents
}

var getTabValue = function (tabId) {
  var webContents = getWebContentsForTab(tabId)
  if (webContents) {
    return extensions.tabValue(webContents)
  }
}

var getWindowIdForTab = function (tabId) {
  // cache the windowId so we can still access
  // it when the webContents is destroyed
  if (tabs[tabId] && tabs[tabId].tabInfo) {
    return tabs[tabId].tabInfo.windowId
  } else {
    return -1
  }
}

var getTabsForWindow = function (windowId) {
  return tabsQuery({windowId})
}

var updateWindow = function (windowId, updateInfo) {
  let win = BrowserWindow.fromId(windowId)

  if (win) {
    if (updateInfo.focused) {
      win.focus()
    }

    if (updateInfo.left || updateInfo.top ||
      updateInfo.width || updateInfo.height) {
      let bounds = win.getBounds()
      bounds.x = updateInfo.left || bounds.x
      bounds.y = updateInfo.top || bounds.y
      bounds.width = updateInfo.width || bounds.width
      bounds.height = updateInfo.height || bounds.height
      win.setBounds(bounds)
    }

    switch (updateInfo.state) {
      case 'minimized':
        win.minimize()
        break
      case 'maximized':
        win.maximize()
        break
      case 'fullscreen':
        win.setFullScreen(true)
        break
    }

    return windowInfo(win, false)
  } else {
    console.warn('chrome.windows.update could not find windowId ' + windowId)
    return {}
  }
}

var createGuest = function (opener, url) {
  var payload = {}
  process.emit('ELECTRON_GUEST_VIEW_MANAGER_NEXT_INSTANCE_ID', payload)
  var guestInstanceId = payload.returnValue

  let embedder = opener.hostWebContents || opener
  var options = {
    guestInstanceId,
    embedder,
    session: opener.session,
    isGuest: true,
    delayedLoadUrl: url
  }
  var webPreferences = Object.assign({}, opener.getWebPreferences(), options)
  var guest = webContents.create(webPreferences)

  process.emit('ELECTRON_GUEST_VIEW_MANAGER_REGISTER_GUEST', { sender: opener }, guest, guestInstanceId)
  return guest
}

var pending_tabs = {}

var createTab = function (createProperties, sender, responseId) {
  var opener = null
  var openerTabId = createProperties.openerTabId
  if (openerTabId) {
    opener = getWebContentsForTab(openerTabId)
  }

  var windowId = createProperties.windowId
  if (opener) {
    var win = null
    win = BrowserWindow.fromWebContents(opener)
    if (win && win.id !== windowId) {
      console.warn('The openerTabId is not in the selected window ', createProperties)
      return
    }
    if (!win) {
      console.warn('The openerTabId is not attached to a window ', createProperties)
      return
    }
  } else {
    var tab = tabsQuery({windowId: windowId || -2, active: true})[0]
    if (tab)
      opener = getWebContentsForTab(tab.id)
  }

  if (!opener) {
    console.warn('Could not find an opener for new tab ', createProperties)
    return
  }

  if (opener.isGuest()) {
    var guest = createGuest(opener, createProperties.url)

    pending_tabs[guest.getId()] = {
      sender,
      createProperties,
      responseId
    }

    var active = createProperties.active !== false
    if (!active)
      active = createProperties.selected !== false

    var disposition = active ? 'foreground-tab' : 'background-tab'

    process.emit('ELECTRON_GUEST_VIEW_MANAGER_TAB_OPEN',
      { sender: opener }, // event
      'about:blank',
      '',
      disposition,
      { webPreferences: guest.getWebPreferences() })
  } else {
    // TODO(bridiver) - open a new window
    // ELECTRON_GUEST_WINDOW_MANAGER_WINDOW_OPEN
    console.warn('chrome.tabs.create from a non-guest opener is not supported yet')
  }
}

var removeTabs = function (tabIds) {
  Array(tabIds).forEach((tabId) => {
    var webContents = getWebContentsForTab(tabId)
    if (webContents) {
      if (webContents.isGuest()) {
        webContents.destroy()
      } else {
        var win = BrowserWindow.fromWebContents(webContents)
        win && win.close()
      }
    }
  })
}

var updateTab = function (tabId, updateProperties) {
  var webContents = getWebContentsForTab(tabId)
  if (!webContents)
    return

  if (updateProperties.url)
    webContents.loadURL(updateProperties.url)
  if (updateProperties.active || updateProperties.selected || updateProperties.highlighted)
    webContents.setActive(true)
}

var chromeTabsUpdated = function (tabId) {
  let oldTabInfo = tabs[tabId] && tabs[tabId].tabInfo
  if (!oldTabInfo) {
    return
  }

  let tabInfo = tabs[tabId].tabInfo = getTabValue(tabId)
  let changeInfo = {}

  for (var key in tabInfo) {
    if (tabInfo[key] !== oldTabInfo[key]) {
      changeInfo[key] = tabInfo[key]
    }
  }

  if (Object.keys(changeInfo).length > 0) {
    sendToBackgroundPages('all', getSessionForTab(tabId), 'chrome-tabs-updated', tabId, changeInfo, tabInfo)
  }
}

var chromeTabsRemoved = function (tabId) {
  let windowId = getWindowIdForTab(tabId)
  let session = getSessionForTab(tabId)
  delete tabs[tabId]
  sendToBackgroundPages('all', session, 'chrome-tabs-removed', tabId, {
    windowId,
    isWindowClosing: windowId === -1 ? true : false
  })
}

Array.prototype.diff = function(a) {
    return this.filter(function(i) {return a.indexOf(i) < 0;});
};

var tabsQuery = function (queryInfo, useCurrentWindowId = false) {
  var tabIds = Object.keys(tabs)

  // convert current window identifier to the actual current window id
  if (queryInfo.windowId === -2 || queryInfo.currentWindow === true) {
    delete queryInfo.currentWindow
    var focusedWindow = BrowserWindow.getFocusedWindow()
    if (focusedWindow) {
      queryInfo.windowId = focusedWindow.id
    }
  }

  var queryKeys = Object.keys(queryInfo)
  // get the values for all tabs
  var tabValues = tabIds.reduce((tabs, tabId) => {
    tabs[tabId] = getTabValue(tabId)
    return tabs
  }, {})
  var result = []
  tabIds.forEach((tabId) => {
    // delete tab from the list if any key doesn't match
    if (!queryKeys.map((queryKey) => (tabValues[tabId][queryKey] === queryInfo[queryKey])).includes(false)) {
      result.push(tabValues[tabId])
    }
  })

  return result
}

var initializeTab = function (tabId, webContents) {
  let tabInfo = extensions.tabValue(webContents)
  if (!tabInfo || tabs[tabId] || tabId !== tabInfo.id) {
    return
  }

  let notifyCreate = false
  let pending = pending_tabs[tabId]

  if (!pending || !pending.createProperties.url || pending.createProperties.url === tabInfo.url) {
    notifyCreate = true
    tabs[tabId] = {}
    tabs[tabId].webContents = webContents
    tabs[tabId].session = webContents.session
    tabs[tabId].tabInfo = tabInfo
  }

  if (!notifyCreate) {
    return
  }

  if (pending) {
    try {
      // sender may be gone when we try to call this
      if (!pending.sender.isDestroyed()) {
        pending.sender.send('chrome-tabs-create-response-' + pending.responseId, tabInfo)
      }
    } catch (e) {
      console.warn('chrome.tabs.create sender went away for ' + tabId)
    }
    delete pending_tabs[tabId]
  }
  sendToBackgroundPages('all', getSessionForTab(tabId), 'chrome-tabs-created', tabInfo)
}

app.on('web-contents-created', function (event, webContents) {
  if (extensions.isBackgroundPage(webContents)) {
    createBackgroundPage(webContents)
    return
  }

  var tabId = webContents.getId()
  if (tabId === -1)
    return

  webContents.on('page-title-updated', function () {
    chromeTabsUpdated(tabId)
  })
  webContents.on('did-fail-load', function () {
    chromeTabsUpdated(tabId)
  })
  webContents.on('did-fail-provisional-load', function () {
    chromeTabsUpdated(tabId)
  })
  webContents.on('did-attach', function () {
    initializeTab(tabId, webContents)
    chromeTabsUpdated(tabId)
  })
  webContents.on('did-stop-loading', function () {
    chromeTabsUpdated(tabId)
  })
  webContents.on('navigation-entry-commited', function (evt, url) {
    initializeTab(tabId, webContents)
    chromeTabsUpdated(tabId)
  })
  webContents.on('did-navigate', function (evt, url) {
    initializeTab(tabId, webContents)
    chromeTabsUpdated(tabId)
  })
  webContents.on('load-start', function (evt, url, isMainFrame, isErrorPage) {
    if (isMainFrame) {
      initializeTab(tabId, webContents)
      chromeTabsUpdated(tabId)
    }
  })
  webContents.on('did-finish-load', function () {
    initializeTab(tabId, webContents)
    chromeTabsUpdated(tabId)
  })
  webContents.on('set-active', function (evt, active) {
    if (!tabs[tabId])
      return

    chromeTabsUpdated(tabId)
    var win = webContents.getOwnerBrowserWindow()
    if (win && active)
      sendToBackgroundPages('all', getSessionForTab(tabId), 'chrome-tabs-activated', tabId, {tabId: tabId, windowId: win.id})
  })
  webContents.on('destroyed', function () {
    chromeTabsRemoved(tabId)
    browserActions.onDestroyed(tabId)
  })
  webContents.on('crashed', function () {
    chromeTabsRemoved(tabId)
  })
  webContents.on('close', function () {
    chromeTabsRemoved(tabId)
  })
})

ipcMain.on('chrome-tabs-create', function (evt, responseId, createProperties) {
  createTab(createProperties, evt.sender, responseId)
})

ipcMain.on('chrome-tabs-remove', function (evt, responseId, tabIds) {
  removeTabs(tabIds)
  evt.sender.send('chrome-tabs-remove-' + responseId)
})

ipcMain.on('chrome-tabs-get-current', function (evt, responseId) {
  var response = getTabValue(evt.sender.getId())
  evt.sender.send('chrome-tabs-get-current-response-' + responseId, response)
})

ipcMain.on('chrome-tabs-get', function (evt, responseId, tabId) {
  var response = getTabValue(tabId)
  evt.sender.send('chrome-tabs-get-response-' + responseId, response)
})

ipcMain.on('chrome-tabs-query', function (evt, responseId, queryInfo) {
  var response = tabsQuery(queryInfo)
  evt.sender.send('chrome-tabs-query-response-' + responseId, response)
})

ipcMain.on('chrome-tabs-update', function (evt, responseId, tabId, updateProperties) {
  var response = updateTab(tabId, updateProperties)
  evt.sender.send('chrome-tabs-update-response-' + responseId, response)
})

ipcMain.on('chrome-tabs-execute-script', function (evt, responseId, extensionId, tabId, details) {
  var tab = getWebContentsForTab(tabId)
  if (tab) {
    tab.executeScriptInTab(extensionId, details.code || '', details, (error, on_url, results) => {
      evt.sender.send('chrome-tabs-execute-script-response-' + responseId, error, on_url, results)
    })
  }
})

var tabEvents = ['updated', 'created', 'removed', 'activated']
tabEvents.forEach((event_name) => {
  ipcMain.on('register-chrome-tabs-' + event_name, function (evt, extensionId) {
    addBackgroundPageEvent(extensionId, 'chrome-tabs-' + event_name)
  })
})

// chrome.windows

var windowInfo = function (win, populateTabs) {
  var bounds = win.getBounds()
  return {
    focused: false,
    // create psuedo-windows to handle this
    incognito: false, // TODO(bridiver)
    id: win.id,
    focused: win.isFocused(),
    top: bounds.y,
    left: bounds.x,
    width: bounds.width,
    height: bounds.height,
    alwaysOnTop: win.isAlwaysOnTop(),
    tabs: populateTabs ? getTabsForWindow(win.id) : null
  }
}

ipcMain.on('chrome-windows-get-current', function (evt, responseId, getInfo) {
  var response = {
    focused: false,
    incognito: false // TODO(bridiver)
  }
  if(getInfo && getInfo.windowTypes) {
    console.warn('getWindow with windowTypes not supported yet')
  }
  var focusedWindow = BrowserWindow.getFocusedWindow()
  if (focusedWindow) {
    response = windowInfo(focusedWindow, getInfo.populate)
  }

  evt.sender.send('chrome-windows-get-current-response-' + responseId,
    response)
})

ipcMain.on('chrome-windows-get-all', function (evt, responseId, getInfo) {
  if (getInfo && getInfo.windowTypes) {
    console.warn('getWindow with windowTypes not supported yet')
  }
  var response = BrowserWindow.getAllWindows().map((win) => {
    return windowInfo(win, getInfo.populate)
  })

  evt.sender.send('chrome-windows-get-all-response-' + responseId,
    response)
})

ipcMain.on('chrome-windows-update', function (evt, responseId, windowId, updateInfo) {
  var response = updateWindow(windowId, updateInfo)
  evt.sender.send('chrome-windows-update-response-' + responseId, response)
})

// chrome.browserAction

ipcMain.on('chrome-browser-action-set-badge-background-color', function (evt, extensionId, details) {
  process.emit('chrome-browser-action-set-badge-background-color', extensionId, details)
  browserActions.setBadgeBackgroundColor(extensionId, details)
})

ipcMain.on('chrome-browser-action-get-background-color-', function (evt, responseId, details) {
  let result = browserActions.getBackgroundColor(extensionId)
  evt.sender.send('chrome-browser-action-get-background-color-response-' + responseId, result)
})

ipcMain.on('chrome-browser-action-set-icon', function (evt, responseId, extensionId, details) {
  process.emit('chrome-browser-action-set-icon', extensionId, details)
  evt.sender.send('chrome-browser-action-set-icon-response-' + responseId)
})

ipcMain.on('chrome-browser-action-get-icon-', function (evt, responseId, details) {
  let result = browserActions.getIcon(extensionId)
  evt.sender.send('chrome-browser-action-get-icon-response-' + responseId, result)
})

ipcMain.on('chrome-browser-action-set-badge-text', function (evt, extensionId, details) {
  process.emit('chrome-browser-action-set-badge-text', extensionId, details, details.tabId)
  browserActions.setBadgeText(extensionId, details)
})

ipcMain.on('chrome-browser-action-get-badge-text-', function (evt, responseId, details) {
  let result = browserActions.getBadgeText(extensionId)
  evt.sender.send('chrome-browser-action-get-badge-text-response-' + responseId, result)
})

ipcMain.on('chrome-browser-action-set-title', function (evt, extensionId, details) {
  process.emit('chrome-browser-action-set-title', extensionId, details)
  browserActions.setTitle(extensionId, details)
})

ipcMain.on('chrome-browser-action-get-title', function (evt, responseId, details) {
  let result = browserActions.getTitle(extensionId)
  evt.sender.send('chrome-browser-action-get-title-response-' + responseId, result)
})

ipcMain.on('chrome-browser-action-set-popup', function (evt, extensionId, details) {
  process.emit('chrome-browser-action-set-popup', extensionId, details)
  browserActions.setPopup(extensionId, details)
})

ipcMain.on('chrome-browser-action-get-popup', function (evt, responseId, details) {
  let result = browserActions.getPopup(extensionId, details)
  evt.sender.send('chrome-browser-action-get-popup-response-' + responseId, result)
})

ipcMain.on('chrome-browser-action-clicked', function (evt, extensionId, tabId, name, props) {
  let popup = browserActions.getPopup(extensionId, {tabId})
  if (popup) {
    process.emit('chrome-browser-action-popup', extensionId, tabId, name, getResourceURL(extensionId, popup), props)
  } else {
    let response = getTabValue(tabId)
    if (response) {
      sendToBackgroundPages(extensionId, getSessionForTab(tabId), 'chrome-browser-action-clicked', response)
    }
  }
})

// TODO(bridiver) - refactor this in browser-laptop and remove
// https://github.com/brave/browser-laptop/pull/3241/files#r75715202
ipcMain.on('autofill-selection-clicked', function (evt, tabId, value, frontend_id, index) {
  let webContents = getWebContentsForTab(tabId)
  if (webContents)
    webContents.autofillSelect(value, frontend_id, index)
})

ipcMain.on('autofill-popup-hidden', function (evt, tabId) {
  let webContents = getWebContentsForTab(tabId)
  if (webContents)
    webContents.autofillPopupHidden()
})
