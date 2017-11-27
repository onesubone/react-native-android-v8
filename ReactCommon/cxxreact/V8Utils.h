//
// Created by hanyanan on 2017/1/22.
//

#ifndef V8_UTILS_H
#define V8_UTILS_H

#include "v8.h"
#include <string>

#include <cxxreact/JSBigString.h>
#include <folly/json.h>
#include <android/log.h>
#include <folly/Exception.h>
#include <folly/Memory.h>
#include <folly/String.h>
#include <folly/Conv.h>
#include <fcntl.h>
#include "jschelpers/Value.h"
#include <android/log.h>
#define _RN_V8_DEBUG_ 1
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO  , "V8Application", __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG  , "V8Application", __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN  , "V8Application", __VA_ARGS__)

using namespace facebook::react;

namespace v8 {

  Local<String> toLocalString(Isolate *isolate, const char *string);

  Local<String> toLocalString(Isolate *isolate, const std::string &string);

  Local<String> toLocalString(Isolate *isolate, const facebook::react::JSBigString &bigString);

  std::string toStdString(const Local<String> &string);

  std::string toJsonStdString(Local<Context> context, const Local<Object> &object);

  Local<String> toJsonString(Local<Context> context, const Local<Object> &object);

  Local<Value> fromJsonString(Local<Context> context, const std::string &jsonStr);

  Local<Value> fromJsonString(Isolate *isolate, Local<Context> context, const char *jsonStr);

  Local<Value> fromJsonString(Isolate *isolate, Local<Context> context, const char *jsonStr, int length);

  Local<Value> fromJsonString(Isolate *isolate, Local<Context> context, const Local<String> &jsonStr);

  Local<Value> fromDynamic(Isolate *isolate, Local<v8::Context> context, const folly::dynamic &value);

  Local<Value> safeToLocal(const MaybeLocal<Value> &maybeLocal);

  std::pair<Local<Uint32>, Local<Uint32>> parseNativeRequireParameters(const v8::FunctionCallbackInfo<v8::Value> &args);

  void nativeLog(const FunctionCallbackInfo<Value> &args);
  void printType(Local<Value> value, const char *desc);
}

#endif //V8_UTILS_H
