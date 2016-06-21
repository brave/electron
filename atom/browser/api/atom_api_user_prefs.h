// Copyright (c) 2015 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef ATOM_BROWSER_API_ATOM_API_USER_PREFS_H_
#define ATOM_BROWSER_API_ATOM_API_USER_PREFS_H_

#include <string>

#include "atom/browser/api/trackable_object.h"
#include "base/callback.h"
#include "native_mate/handle.h"

namespace base {
class DictionaryValue;
class ListValue;
}

namespace atom {

class AtomBrowserContext;

namespace api {

class UserPrefs : public mate::TrackableObject<UserPrefs> {
 public:
  static mate::Handle<UserPrefs> Create(v8::Isolate* isolate,
                                      AtomBrowserContext* browser_context);

  // mate::TrackableObject:
  static void BuildPrototype(v8::Isolate* isolate,
                             v8::Local<v8::ObjectTemplate> prototype);

 protected:
  UserPrefs(v8::Isolate* isolate, AtomBrowserContext* browser_context);
  ~UserPrefs() override;

  void RegisterStringPref(const std::string& path,
      const std::string& default_value, bool overlay);
  void RegisterDictionaryPref(const std::string& path,
      const base::DictionaryValue& default_value, bool overlay);
  void RegisterListPref(const std::string& path,
      const base::ListValue& default_value, bool overlay);
  void RegisterBooleanPref(const std::string& path,
      bool default_value, bool overlay);
  void RegisterIntegerPref(const std::string& path,
      int default_value, bool overlay);

  std::string GetStringPref(const std::string& path);
  const base::DictionaryValue* GetDictionaryPref(const std::string& path);
  const base::ListValue* GetListPref(const std::string& path);
  bool GetBooleanPref(const std::string& path);
  int GetIntegerPref(const std::string& path);

  void SetStringPref(const std::string& path, const std::string& value);
  void SetDictionaryPref(const std::string& path,
      const base::DictionaryValue& value);
  void SetListPref(const std::string& path, const base::ListValue& value);
  void SetBooleanPref(const std::string& path, bool value);
  void SetIntegerPref(const std::string& path, int value);

  void SetDefaultStringPref(const std::string& path, const std::string& value);
  void SetDefaultDictionaryPref(const std::string& path,
      const base::DictionaryValue& value);
  void SetDefaultListPref(const std::string& path, const base::ListValue& value);
  void SetDefaultBooleanPref(const std::string& path, bool value);
  void SetDefaultIntegerPref(const std::string& path, int value);

  AtomBrowserContext* browser_context() { return browser_context_; };

 private:
  AtomBrowserContext* browser_context_;  // not owned

  DISALLOW_COPY_AND_ASSIGN(UserPrefs);
};

}  // namespace api

}  // namespace atom

#endif  // ATOM_BROWSER_API_ATOM_API_USER_PREFS_H_
