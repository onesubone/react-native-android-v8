#ifndef V8_DEMO_V8EXECUTOR_H
#define V8_DEMO_V8EXECUTOR_H

#pragma once

#ifndef RN_V8JS_EXECUTOR_EXPORT
#define RN_V8JS_EXECUTOR_EXPORT __attribute__((visibility("default")))
#endif

#include <string>
#include <cxxreact/Executor.h>
#include <cxxreact/V8NativeModules.h>
#include <jschelpers/Value.h>
#include "MessageQueueThread.h"
#include "v8.h"
#include <folly/Optional.h>
#include <jschelpers/JSCHelpers.h>
#include <folly/json.h>

namespace v8 {
class RN_V8JS_EXECUTOR_EXPORT V8Executor : public JSExecutor {
public:
  /**
   * Must be invoked from thread this Executor will run on.
   */
  explicit V8Executor(std::shared_ptr<ExecutorDelegate> delegate,
                       std::shared_ptr<MessageQueueThread> messageQueueThread,
                       const folly::dynamic& jscConfig) throw(JSException);

  virtual ~V8Executor() override;

  virtual void loadApplicationScript(
    std::unique_ptr<const JSBigString> script,
    std::string sourceURL) override;

  virtual void setJSModulesUnbundle(
    std::unique_ptr<JSModulesUnbundle> unbundle) override;

  virtual void callFunction(
      const std::string& moduleId,
      const std::string& methodId,
      const folly::dynamic& arguments) override;

  virtual void invokeCallback(
      const double callbackId,
      const folly::dynamic& arguments) override;

/*  template <typename T>
  Value callFunctionSync(const std::string& module, const std::string& method, T&& args) {
          return callFunctionSyncWithValue(
            module, method, ValueEncoder<typename std::decay<T>::type>::toValue(
              m_context, std::forward<T>(args)));
  }*/

  virtual void setGlobalVariable(
      std::string propName,
      std::unique_ptr<const JSBigString> jsonValue) override;

  virtual void* getJavaScriptContext() override;

  virtual bool supportsProfiling() override;
  virtual void startProfiler(const std::string &titleString) override;
  virtual void stopProfiler(const std::string &titleString, const std::string &filename) override;

  virtual void handleMemoryPressureUiHidden() override;
  virtual void handleMemoryPressureModerate() override;
  virtual void handleMemoryPressureCritical() override;

  virtual void destroy() override;
  void setContextName(const std::string& name);

  /**
   * global data
   */
  static Isolate *GetIsolate();
private:
  std::shared_ptr<ExecutorDelegate> m_delegate;
  std::shared_ptr<bool> m_isDestroyed = std::shared_ptr<bool>(new bool(false));
  std::string m_deviceCacheDir;
  Global<Context> m_context;
  std::shared_ptr<MessageQueueThread> m_messageQueueThread;
  std::unique_ptr<JSModulesUnbundle> m_unbundle;
  V8NativeModules m_nativeModules;
  folly::dynamic m_jscConfig;
  std::once_flag m_bindFlag;


  Global<Function> m_invokeCallbackAndReturnFlushedQueueJS;
  Global<Function> m_callFunctionReturnFlushedQueueJS;
  Global<Function> m_flushedQueueJS;
  Global<Function> m_callFunctionReturnResultAndFlushedQueueJS;


  void initOnJSVMThread() throw(JSException);
  void executeScript(Local<Context> context, const Local<String> &script) throw(JSException) ;
  // This method is experimental, and may be modified or removed.
  // Value callFunctionSyncWithValue(const std::string& module, const std::string& method, Value value);
  Global<Value> getNativeModule(Local<String> property, const PropertyCallbackInfo<Value> &info);
  void terminateOnJSVMThread();
  void bindBridge() throw(JSException);
  void callNativeModules(Local<Context> context, Local<Value> value);
  void flush();
  void flushQueueImmediate(Value&&);
  void loadModule(uint32_t moduleId);

  template<void (V8Executor::*method)(const v8::FunctionCallbackInfo<v8::Value> &args)>
  void installNativeFunctionHook(Local<ObjectTemplate> global, const char *name);

  template<Global<Value> (V8Executor::*method)(Local<String> property, const PropertyCallbackInfo<Value> &info)>
  void installNativePropertyHook(Local<ObjectTemplate> global, const char *name);
  template<Global<Value> (V8Executor::*method)(Local<String> property, const PropertyCallbackInfo<Value> &info)>
  void installNativePropertyHook(Local<Object> global, const char *name);


  void nativeRequire(const FunctionCallbackInfo<Value> &args);
  void nativeFlushQueueImmediate(const FunctionCallbackInfo<Value> &args);
  void nativeCallSyncHook(const FunctionCallbackInfo<Value> &args);

  static Isolate *m_isolate; // shared on global
};
}
#endif //V8_DEMO_V8EXECUTOR_H
