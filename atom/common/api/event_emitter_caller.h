// Copyright (c) 2015 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef ATOM_COMMON_API_EVENT_EMITTER_CALLER_H_
#define ATOM_COMMON_API_EVENT_EMITTER_CALLER_H_

#include <vector>

#include "gin/converter.h"
#include "gin/dictionary.h"
#include "native_mate/converter.h"

namespace mate {

template<>
struct Converter<gin::Dictionary> {
  static v8::Local<v8::Value> ToV8(v8::Isolate* isolate, gin::Dictionary val) {
    return gin::Converter<gin::Dictionary>::ToV8(isolate, val);
  }

  static bool FromV8(v8::Isolate* isolate,
                                   v8::Local<v8::Value> val,
                                   gin::Dictionary* out) {
    return gin::Converter<gin::Dictionary>::FromV8(isolate, val, out);;
  }
};

namespace internal {

using ValueVector = std::vector<v8::Local<v8::Value>>;

v8::Local<v8::Value> CallMethodWithArgs(v8::Isolate* isolate,
                                      v8::Local<v8::Object> obj,
                                      const char* method,
                                      ValueVector* args);

}  // namespace internal

// obj.emit.apply(obj, name, args...);
// The caller is responsible of allocating a HandleScope.
template<typename StringType, typename... Args>
v8::Local<v8::Value> EmitEvent(v8::Isolate* isolate,
                               v8::Local<v8::Object> obj,
                               const StringType& name,
                               const internal::ValueVector& args) {
  internal::ValueVector concatenated_args = { gin::StringToV8(isolate, name) };
  concatenated_args.reserve(1 + args.size());
  concatenated_args.insert(concatenated_args.end(), args.begin(), args.end());
  return internal::CallMethodWithArgs(isolate, obj, "emit", &concatenated_args);
}

// obj.emit(name, args...);
// The caller is responsible of allocating a HandleScope.
template<typename StringType, typename... Args>
v8::Local<v8::Value> EmitEvent(v8::Isolate* isolate,
                               v8::Local<v8::Object> obj,
                               const StringType& name,
                               const Args&... args) {
  internal::ValueVector converted_args = {
      gin::StringToV8(isolate, name),
      mate::ConvertToV8(isolate, args)...,
  };
  return internal::CallMethodWithArgs(isolate, obj, "emit", &converted_args);
}

}  // namespace mate

#endif  // ATOM_COMMON_API_EVENT_EMITTER_CALLER_H_
