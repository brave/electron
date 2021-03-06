'use strict'

const {app, BrowserWindow} = require('electron')
const binding = process.atomBinding('dialog')
const v8Util = process.atomBinding('v8_util')

var includes = [].includes

const fileDialogProperties = {
  openFile: 1 << 0,
  openDirectory: 1 << 1,
  multiSelections: 1 << 2,
  createDirectory: 1 << 3,
  showHiddenFiles: 1 << 4,
  promptToCreate: 1 << 5
}

var messageBoxTypes = ['none', 'info', 'warning', 'error', 'question']

var messageBoxOptions = {
  noLink: 1 << 0
}

var parseArgs = function (window, options, callback, ...args) {
  if (window !== null && window.constructor !== BrowserWindow) {
    // Shift.
    [callback, options, window] = [options, window, null]
  }

  if ((callback == null) && typeof options === 'function') {
    // Shift.
    [callback, options] = [options, null]
  }

  // Fallback to using very last argument as the callback function
  var lastArgument = args[args.length - 1]
  if ((callback == null) && typeof lastArgument === 'function') {
    callback = lastArgument
  }

  return [window, options, callback]
}

var checkAppInitialized = function () {
  if (!app.isReady()) {
    throw new Error('dialog module can only be used after app is ready')
  }
}

module.exports = {
  showDialog: function (...args) {
    var wrappedCallback
    checkAppInitialized()
    let [window, options, callback] = parseArgs.apply(null, args)
    if (window == null) {
      throw new TypeError('window can not be null')
    }
    if (options == null) {
      throw new TypeError('options can not be null')
    }
    if (callback == null) {
      throw new TypeError('callback can not be null')
    }
    if (options.type == null) {
      options.type = ''
    } else if (typeof options.type !== 'string') {
      throw new TypeError('type must be a string')
    }
    if (options.defaultPath == null) {
      options.defaultPath = ''
    } else if (typeof options.defaultPath !== 'string') {
      throw new TypeError('Default path must be a string')
    }
    wrappedCallback = typeof callback === 'function' ? function (success, result) {
      return callback(success ? result : void 0)
    } : null
    let settings = options
    settings.window = window
    return binding.showDialog(settings, wrappedCallback)
  },

  showMessageBox: function (...args) {
    var flags, i, j, len, messageBoxType, ref2, ref3, text
    checkAppInitialized()
    let [window, options, callback] = parseArgs.apply(null, args)
    if (options == null) {
      options = {
        type: 'none'
      }
    }
    if (options.type == null) {
      options.type = 'none'
    }
    messageBoxType = messageBoxTypes.indexOf(options.type)
    if (!(messageBoxType > -1)) {
      throw new TypeError('Invalid message box type')
    }
    if (!Array.isArray(options.buttons)) {
      throw new TypeError('Buttons must be an array')
    }
    if (options.title == null) {
      options.title = ''
    } else if (typeof options.title !== 'string') {
      throw new TypeError('Title must be a string')
    }
    if (options.message == null) {
      options.message = ''
    } else if (typeof options.message !== 'string') {
      throw new TypeError('Message must be a string')
    }
    if (options.detail == null) {
      options.detail = ''
    } else if (typeof options.detail !== 'string') {
      throw new TypeError('Detail must be a string')
    }
    if (options.icon == null) {
      options.icon = null
    }
    if (options.defaultId == null) {
      options.defaultId = -1
    }

    // Choose a default button to get selected when dialog is cancelled.
    if (options.cancelId == null) {
      options.cancelId = 0
      ref2 = options.buttons
      for (i = j = 0, len = ref2.length; j < len; i = ++j) {
        text = ref2[i]
        if ((ref3 = text.toLowerCase()) === 'cancel' || ref3 === 'no') {
          options.cancelId = i
          break
        }
      }
    }
    flags = options.noLink ? messageBoxOptions.noLink : 0
    return binding.showMessageBox(messageBoxType, options.buttons, options.defaultId, options.cancelId, flags, options.title, options.message, options.detail, options.icon, window, callback)
  },

  showErrorBox: function (...args) {
    return binding.showErrorBox.apply(binding, args)
  }
}

// Mark standard asynchronous functions.
var ref1 = ['showMessageBox', 'showDialog']
var j, len, api
for (j = 0, len = ref1.length; j < len; j++) {
  api = ref1[j]
  v8Util.setHiddenValue(module.exports[api], 'asynchronous', true)
}
