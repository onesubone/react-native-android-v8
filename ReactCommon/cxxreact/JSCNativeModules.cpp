// Copyright 2004-present Facebook. All Rights Reserved.

#include "JSCNativeModules.h"

#include <string>

#include <android/log.h>
#define _RN_V8_DEBUG_ 1
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO  , "V8Application", __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG  , "V8Application", __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN  , "V8Application", __VA_ARGS__)

namespace facebook {
namespace react {

JSCNativeModules::JSCNativeModules(std::shared_ptr<ModuleRegistry> moduleRegistry) :
  m_moduleRegistry(std::move(moduleRegistry)) {}

JSValueRef JSCNativeModules::getModule(JSContextRef context, JSStringRef jsName) {
  if (!m_moduleRegistry) {
    return nullptr;
  }

  std::string moduleName = String::ref(context, jsName).str();

  const auto it = m_objects.find(moduleName);
  if (it != m_objects.end()) {
    return static_cast<JSObjectRef>(it->second);
  }

  auto module = createModule(moduleName, context);
  if (!module.hasValue()) {
    // Allow lookup to continue in the objects own properties, which allows for overrides of NativeModules
    return nullptr;
  }

  // Protect since we'll be holding on to this value, even though JS may not
  module->makeProtected();

  auto result = m_objects.emplace(std::move(moduleName), std::move(*module)).first;
  return static_cast<JSObjectRef>(result->second);
}

void JSCNativeModules::reset() {
  m_genNativeModuleJS = nullptr;
  m_objects.clear();
}

folly::Optional<Object> JSCNativeModules::createModule(const std::string& name, JSContextRef context) {
  LOGI("JSCNativeModules::createModule moduleName %s", name.c_str());
  if (!m_genNativeModuleJS) {
    auto global = Object::getGlobalObject(context);
    m_genNativeModuleJS = global.getProperty("__fbGenNativeModule").asObject();
    m_genNativeModuleJS->makeProtected();
  }

  auto result = m_moduleRegistry->getConfig(name);
  if (!result.hasValue()) {
    LOGI("JSCNativeModules::createModule moduleName %s Return NUll!", name.c_str());
    return nullptr;
  }

  Value moduleInfo = m_genNativeModuleJS->callAsFunction({
    Value::fromDynamic(context, result->config),
    Value::makeNumber(context, result->index)
  });
  CHECK(!moduleInfo.isNull()) << "Module returned from genNativeModule is null";

  return moduleInfo.asObject().getProperty("module").asObject();
}

} }
