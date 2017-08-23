#include <algorithm>
#include <condition_variable>
#include <mutex>
#include <sstream>
#include <string>
#include <folly/json.h>
#include <folly/Exception.h>
#include <folly/Memory.h>
#include <folly/String.h>
#include <folly/Conv.h>
#include <fcntl.h>
#include <sys/time.h>

#include "Platform.h"
#include "SystraceSection.h"
#include "JSModulesUnbundle.h"
#include "ModuleRegistry.h"
#include "V8Executor.h"
#include "V8NativeModules.h"
#include "V8Utils.h"
#include "include/v8.h"
#include "include/libplatform/libplatform.h"


namespace v8 {

#define THROW_RUNTIME_ERROR(INFO) std::throw_with_nested(std::runtime_error(INFO))
#define _ISOLATE_CONTEXT_ENTER Isolate *isolate = GetIsolate(); \
    Isolate::Scope isolate_scope(isolate); \
    HandleScope handle_scope(isolate); \
    Local<Context> context = Local<Context>::New(isolate, m_context); \
    Context::Scope context_scope(context); \

Isolate *V8Executor::m_isolate = nullptr;


#if DEBUG
static void nativeInjectHMRUpdate() {
}
#endif

void nativePerformanceNow(const FunctionCallbackInfo<Value> &args) {
    static const int64_t NANOSECONDS_IN_SECOND = 1000000000LL;
    static const int64_t NANOSECONDS_IN_MILLISECOND = 1000000LL;

    // This is equivalent to android.os.SystemClock.elapsedRealtime() in native
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC_RAW, &now);
    int64_t nano = now.tv_sec * NANOSECONDS_IN_SECOND + now.tv_nsec;
    args.GetReturnValue().Set((nano / (double) NANOSECONDS_IN_MILLISECOND));
}

// Native JS Function Hooks
template<void (V8Executor::*method)(const FunctionCallbackInfo<Value> &args)>
void V8Executor::installNativeFunctionHook(Local<ObjectTemplate> global, const char *name) {
    LOGI("V8Executor.installNativeFunctionHook name: %s", name);
    struct funcWrapper {
        static void call(const v8::FunctionCallbackInfo<v8::Value> &args) {
            Isolate *isolate = args.GetIsolate();
            HandleScope handle_scope(isolate);
            Local<Context> context = isolate->GetCurrentContext();
            auto ptr = context->GetAlignedPointerFromEmbedderData(1);
            V8Executor *executor = static_cast<V8Executor *>(ptr);
            if (!executor) {
                THROW_RUNTIME_ERROR("Get Empty Context in installNativeHook!");
            }
            (executor->*method)(std::move(args));
        }
    };
    global->Set(toLocalString(GetIsolate(), name), FunctionTemplate::New(GetIsolate(), funcWrapper::call));
}

// Native Static JS Function Hooks
template<void (*method)(const v8::FunctionCallbackInfo<Value> &args)>
void installGlobalFunction(Isolate *isolate, Local<ObjectTemplate> global, const char *name) {
  LOGI("V8Executor.installGlobalFunction name: %s", name);
  global->Set(toLocalString(isolate, name), FunctionTemplate::New(V8Executor::GetIsolate(), method));
}

template<Global<Value> (V8Executor::*method)(Local<String> property, const PropertyCallbackInfo<Value> &info_t)>
void V8Executor::installNativePropertyHook(Local<ObjectTemplate> global, const char *name) {
    LOGI("V8Executor.installNativePropertyHook name: %s", name);
    struct funcWrapper {
        static void call(Local<Name> localProperty, const PropertyCallbackInfo<Value> &info) {
            printType(localProperty, "installNativePropertyHook.localProperty");
            Isolate *isolate = info.GetIsolate();
            HandleScope handle_scope(isolate);
            Local<Context> context = isolate->GetCurrentContext();
            auto ptr = context->GetAlignedPointerFromEmbedderData(1);
            V8Executor *executor = static_cast<V8Executor *>(ptr);
            if (!executor) {
                THROW_RUNTIME_ERROR("Get Empty Context in installNativePropertyHook!");
            }

            Global<Value> res = (executor->*method)(Local<String>::Cast(localProperty), std::move(info));
            info.GetReturnValue().Set(std::move(res));
        }
    };
    Local<ObjectTemplate> nativeModuleProxyTemplate = ObjectTemplate::New(GetIsolate());
    NamedPropertyHandlerConfiguration configuration(funcWrapper::call);
    nativeModuleProxyTemplate->SetHandler(configuration);
    global->Set(toLocalString(GetIsolate(), name), nativeModuleProxyTemplate);
    LOGI("V8Executor.installNativePropertyHook Finished name: %s", name);
}

    template<Global<Value> (V8Executor::*method)(Local<String> property, const PropertyCallbackInfo<Value> &info)>
    void V8Executor::installNativePropertyHook(Local<Object> global, const char *name) {
        LOGI("V8Executor.installNativePropertyHook name: %s", name);
        struct funcWrapper {
            static void getter(Local<String> localProperty, const PropertyCallbackInfo<Value> &info) {
                Isolate *isolate = info.GetIsolate();
                HandleScope handle_scope(isolate);
                Local<Context> context = isolate->GetCurrentContext();
                auto ptr = context->GetAlignedPointerFromEmbedderData(1);
                V8Executor *executor = static_cast<V8Executor *>(ptr);
                if (!executor) {
                    THROW_RUNTIME_ERROR("Get Empty Context in installNativePropertyHook!");
                }
                Global<Value> res = (executor->*method)(localProperty, info);
                info.GetReturnValue().Set(std::move(res));
            }

            static void setter(Local<String> property, Local<Value> value, const PropertyCallbackInfo<void>& info) {
                LOGI("V8Executor.installNativePropertyHook setter");
            }
        };
        global->SetAccessor(toLocalString(GetIsolate(), name), &funcWrapper::getter, nullptr);
        LOGI("V8Executor.installNativePropertyHook Finished name: %s", name);
    }

Isolate *V8Executor::GetIsolate() {
    if (m_isolate) {
        return m_isolate;
    }
    Platform *platform = platform::CreateDefaultPlatform();
    V8::InitializePlatform(platform);
    V8::Initialize();

    // Create a new Isolate and make it the current one.
    Isolate::CreateParams create_params;
    create_params.array_buffer_allocator = v8::ArrayBuffer::Allocator::NewDefaultAllocator();
    Isolate *isolate = Isolate::New(create_params);
    m_isolate = isolate;
    return isolate;
}

V8Executor::V8Executor(std::shared_ptr<ExecutorDelegate> delegate,
                         std::shared_ptr<MessageQueueThread> messageQueueThread,
                         const folly::dynamic& jscConfig) throw(facebook::react::JSException) :
    m_delegate(delegate),
    m_messageQueueThread(messageQueueThread),
    m_nativeModules(delegate ? delegate->getModuleRegistry() : nullptr),
    m_jscConfig(jscConfig) {
  initOnJSVMThread();
}

V8Executor::~V8Executor() {
  CHECK(*m_isDestroyed) << "JSCExecutor::destroy() must be called before its destructor!";
}

void V8Executor::destroy() {
    *m_isDestroyed = true;
    if (m_messageQueueThread.get()) {
        m_messageQueueThread->runOnQueueSync([this]() {
            terminateOnJSVMThread();
        });
    } else {
        terminateOnJSVMThread();
    }
}

void V8Executor::setContextName(const std::string& name) {
  LOGI("V8Executor.setContextName name: %s", name.c_str());
}


void V8Executor::initOnJSVMThread() throw(JSException) {
  SystraceSection s("V8Executor.initOnJSVMThread");
  Isolate *isolate = GetIsolate();
  Isolate::Scope isolate_scope(isolate);
  HandleScope handle_scope(isolate);
  Local<ObjectTemplate> global = ObjectTemplate::New(isolate);
  // Bind the global 'print' function to the C++ Print callback.
  installNativeFunctionHook<&V8Executor::nativeFlushQueueImmediate>(global, "nativeFlushQueueImmediate");
  installNativeFunctionHook<&V8Executor::nativeCallSyncHook>(global, "nativeCallSyncHook");

  installGlobalFunction<&nativeLog>(isolate, global, "nativeLoggingHook");
  installGlobalFunction<&nativePerformanceNow>(isolate, global, "nativePerformanceNow");

  #if DEBUG
  installGlobalFunction(isolate, global, "nativeInjectHMRUpdate", nativeInjectHMRUpdate);
  #endif

  #if defined(WITH_JSC_EXTRA_TRACING) || (DEBUG && defined(WITH_FBSYSTRACE))
  // TODO
  #endif

  #ifdef WITH_JSC_EXTRA_TRACING
  // TODO
  #endif

  // native require
  installNativeFunctionHook<&V8Executor::nativeRequire>(global, "nativeRequire");
  installNativePropertyHook<&V8Executor::getNativeModule>(global, "nativeModuleProxy");
  Local<Context> context = Context::New(isolate, NULL, global);
  Context::Scope context_scope(context);
  context->SetAlignedPointerInEmbedderData(1, this);
  //Local<Object> globalObj = context->Global();
  //installNativePropertyHook<&V8Executor::getNativeModule>(globalObj, "nativeModuleProxy");

  m_context.Reset(isolate, context);
  LOGI("V8Executor.initOnJSVMThread Finished!!!!");
}

void V8Executor::terminateOnJSVMThread() {
    SystraceSection s("V8Executor::terminateOnJSVMThread");
  m_nativeModules.reset();
  m_invokeCallbackAndReturnFlushedQueueJS.Reset();
  m_callFunctionReturnFlushedQueueJS.Reset();
  m_flushedQueueJS.Reset();
  m_callFunctionReturnResultAndFlushedQueueJS.Reset();
  m_context.Reset();
}

void V8Executor::loadApplicationScript(std::unique_ptr<const JSBigString> script, std::string sourceURL) {
  SystraceSection s("V8Executor::loadApplicationScript", "sourceURL", sourceURL);
  // TODO
  _ISOLATE_CONTEXT_ENTER;
  executeScript(context, std::move(toLocalString(isolate, std::move(script->c_str()))));

  flush();
}

void V8Executor::executeScript(Local<Context> context, const Local<String> &script) throw(JSException) {
    SystraceSection s("V8Executor::executeScript");
    Isolate *isolate = GetIsolate();
    TryCatch try_catch(isolate);
    Local<Script> compiled_script;
    if (!Script::Compile(context, std::move(script)).ToLocal(&compiled_script)) {
        String::Utf8Value error(try_catch.Exception());
        // The script failed to compile; bail out.
        THROW_RUNTIME_ERROR("Error ExecuteScript while compile script!");
    }

    // Run the script!
    Local<Value> result;
    if (!compiled_script->Run(context).ToLocal(&result)) {
        // The TryCatch above is still in effect and will have caught the error.
        String::Utf8Value error(try_catch.Exception());
        LOGW("compiled_script->Run error: %s", *error);
        THROW_RUNTIME_ERROR("Error ExecuteScript while running script!");
    }
}

void V8Executor::bindBridge() throw(JSException) {
  SystraceSection s("V8Executor::bindBridge");
  std::call_once(m_bindFlag, [this] {
     _ISOLATE_CONTEXT_ENTER;
     Local<Object> globalObj = context->Global();
     Local<Value> batchedBridgeValue; // batchedBridgeValue;
     if (!globalObj->Get(context, toLocalString(isolate, "__fbBatchedBridge")).ToLocal(&batchedBridgeValue)) {
        Local<Value> requireBatchedBridge; // batchedBridgeValue;
        if (globalObj->Get(context, toLocalString(isolate, "__fbRequireBatchedBridge")).ToLocal(&requireBatchedBridge)) {
            Local<Function> requireBatchedBridgeFunc = Local<Function>::Cast(requireBatchedBridge);
            if(!requireBatchedBridgeFunc->Call(context, context->Global(), 0, {}).ToLocal(&batchedBridgeValue)) {
                THROW_RUNTIME_ERROR("Could not get BatchedBridge, make sure your bundle is packaged correctly");
            }
        }
     }
     if(batchedBridgeValue.IsEmpty()){
        THROW_RUNTIME_ERROR("Could not get BatchedBridge, make sure your bundle is packaged correctly");
     }
     Local<Object> batchedBridge = Local<Object>::Cast(batchedBridgeValue);
     auto funcSet = [&](const char *name, Global<Function> &globalFunc) mutable {
         Local<Function> localFunc = Local<Function>::Cast(batchedBridge->Get(toLocalString(isolate, name)));
         globalFunc.Reset(isolate, localFunc);
     };
     funcSet("callFunctionReturnFlushedQueue", m_callFunctionReturnFlushedQueueJS);
     funcSet("invokeCallbackAndReturnFlushedQueue", m_invokeCallbackAndReturnFlushedQueueJS);
     funcSet("flushedQueue", m_flushedQueueJS);
     funcSet("callFunctionReturnResultAndFlushedQueue", m_callFunctionReturnResultAndFlushedQueueJS);
    });
}

void V8Executor::callNativeModules(Local<Context> context, Local<Value> value) {
  SystraceSection s("V8Executor::callNativeModules");
  CHECK(m_delegate) << "Attempting to use native modules without a delegate";
  try {
     if (value->IsObject()) {
        Local<Object> obj = Local<Object>::Cast(value);
        const std::string &arg = toJsonStdString(context, std::move(obj));
        m_delegate->callNativeModules(*this, folly::parseJson(std::move(arg)), true);
     } else {
//            m_delegate->callNativeModules(*this, folly::parseJson(std::string("")), true);
     }
  } catch (...) {
     std::string message = "Error in callNativeModules()";
//        try {
//            message += ":" + value.toString().str();
//        } catch (...) {
//            // ignored
//        }
        std::throw_with_nested(std::runtime_error(message));
    }
}

void V8Executor::flush() {
  SystraceSection s("V8Executor::flush");
  if (!m_flushedQueueJS.IsEmpty()) {
    _ISOLATE_CONTEXT_ENTER;
    Local<Function> flushedQueueJS = Local<Function>::New(isolate, m_flushedQueueJS);
    callNativeModules(context, flushedQueueJS->Call(context, context->Global(), 0, {}).ToLocalChecked());
    return;
  }


  // When a native module is called from JS, BatchedBridge.enqueueNativeCall()
  // is invoked.  For that to work, require('BatchedBridge') has to be called,
  // and when that happens, __fbBatchedBridge is set as a side effect.
  _ISOLATE_CONTEXT_ENTER;
  Local<Object> globalObj = context->Global();
  Local<Value> batchedBridgeValue; // batchedBridgeValue;
  if (globalObj->Get(context, toLocalString(isolate, "__fbBatchedBridge")).ToLocal(&batchedBridgeValue)) {
     LOGI("V8Executor::flush obj __fbBatchedBridge is not empty");
     bindBridge();
     Local<Function> flushedQueueJS = Local<Function>::New(isolate, m_flushedQueueJS);
     callNativeModules(context, flushedQueueJS->Call(context, context->Global(), 0, {}).ToLocalChecked());
  }else if (m_delegate) {
     // If we have a delegate, we need to call it; we pass a null list to
     // callNativeModules, since we know there are no native calls, without
     // calling into JS again.  If no calls were made and there's no delegate,
     // nothing happens, which is correct.
     callNativeModules(context, Local<Value>());
  }
}


void V8Executor::setJSModulesUnbundle(std::unique_ptr<JSModulesUnbundle> unbundle) {
  SystraceSection s("V8Executor::setJSModulesUnbundle");
  m_unbundle = std::move(unbundle);
}

void V8Executor::callFunction(const std::string &moduleId, const std::string &methodId,
                                const folly::dynamic &arguments) {
  SystraceSection s("V8Executor::callFunction");
  LOGI("V8Executor::callFunction moduleId: %s, methodId:%s", moduleId.c_str(), methodId.c_str());
  if (m_callFunctionReturnResultAndFlushedQueueJS.IsEmpty()) {
    bindBridge();
  }
  _ISOLATE_CONTEXT_ENTER;
  Local<Function> localFunc = Local<Function>::New(isolate, m_callFunctionReturnFlushedQueueJS);
  Local<String> localModuleId = toLocalString(isolate, moduleId);
  Local<String> localMethodId = toLocalString(isolate, methodId);
  const std::string &json = folly::toJson(arguments);
  Local<String> json_str = toLocalString(isolate, std::move(json));
  Local<Value> localArguments = JSON::Parse(context, json_str).ToLocalChecked();
  Local<Value> argv[3] = {localModuleId, localMethodId, localArguments};
  Local<Value> result = localFunc->Call(context, context->Global(), 3, argv).ToLocalChecked(); // TODO, catch exception
  callNativeModules(context, result);
}

void V8Executor::invokeCallback(const double callbackId, const folly::dynamic &arguments) {
  SystraceSection s("V8Executor::invokeCallback");
  if (m_invokeCallbackAndReturnFlushedQueueJS.IsEmpty()) {
    bindBridge();
  }
  _ISOLATE_CONTEXT_ENTER;
  Local<Function> invokeFunc = Local<Function>::New(isolate, m_invokeCallbackAndReturnFlushedQueueJS);
  Local<Number> localCallbackId = Number::New(isolate, callbackId);
  Local<Value> localArguments = fromDynamic(isolate, context, arguments);
  Local<Value> argv[2] = {localCallbackId, localArguments};
  Local<Value> result = invokeFunc->Call(context, context->Global(), 2, argv).ToLocalChecked();
  callNativeModules(context, result);
}

void V8Executor::setGlobalVariable(std::string propName, std::unique_ptr<const JSBigString> jsonValue) {
  try {
    SystraceSection s("V8Executor.setGlobalVariable", "propName", propName);
    _ISOLATE_CONTEXT_ENTER;
    Local<String> propNameString = toLocalString(isolate, propName);
    Local<Value> attribute = fromJsonString(isolate, context, jsonValue->c_str(), jsonValue->size());
    context->Global()->Set(propNameString, attribute);
  } catch (...) {
    std::throw_with_nested(std::runtime_error("Error setting global variable: " + propName));
  }
}


void *V8Executor::getJavaScriptContext() {
    return this;
}

bool V8Executor::supportsProfiling() {
  return false;
}

void V8Executor::startProfiler(const std::string &titleString) {
  LOGI("V8Executor.startProfiler titleString:%s", titleString.c_str());
  // Not Support Now
}

void V8Executor::stopProfiler(const std::string &titleString, const std::string& filename) {
  LOGI("V8Executor.stopProfiler titleString:%s, filename:%s", titleString.c_str(), filename.c_str());
  // Not Support Now
}

void V8Executor::handleMemoryPressureUiHidden() {
    LOGI("V8Executor.handleMemoryPressureUiHidden");
}

void V8Executor::handleMemoryPressureModerate() {
    LOGI("V8Executor.handleMemoryPressureModerate");
}

void V8Executor::handleMemoryPressureCritical() {
    LOGI("V8Executor.handleMemoryPressureCritical");
}

Global<Value> V8Executor::getNativeModule(Local<String> property, const PropertyCallbackInfo<Value> &info) {
  SystraceSection s("V8Executor.getNativeModule");
  _ISOLATE_CONTEXT_ENTER;
    LOGI("V8Executor.getNativeModule property length %d" , property->Length());
  LOGI("V8Executor.getNativeModule property %s" , toStdString(property).c_str());
  const std::string &pro = toStdString(property);
  if ("name" == pro) {
    return Global<Value>(isolate, toLocalString(isolate, "NativeModules"));
  }
  return m_nativeModules.getModule(isolate, context, pro);
}

void V8Executor::nativeRequire(const v8::FunctionCallbackInfo<v8::Value> &args) {
    SystraceSection s("V8Executor.nativeRequire");
    if (args.Length() != 1) {
        throw std::invalid_argument("Got wrong number of args");
    }
    _ISOLATE_CONTEXT_ENTER;
    Local<Uint32> id = Local<Uint32>::Cast(args[0]);
    uint32_t moduleId = id->Value();
    LOGI("V8Executor.nativeRequire %d" , moduleId);

    /*if (moduleId <= 0) {
        throw std::invalid_argument(folly::to<std::string>("Received invalid module ID: ",
            Value(m_context, arguments[0]).toString().str()));
    }*/
    ReactMarker::logMarker(ReactMarker::NATIVE_REQUIRE_START);
    auto module = m_unbundle->getModule(moduleId);
    const std::string &source = module.code;
    executeScript(context, std::move(toLocalString(isolate, std::move(source))));
    ReactMarker::logMarker(ReactMarker::NATIVE_REQUIRE_STOP);
}

void V8Executor::nativeFlushQueueImmediate(const v8::FunctionCallbackInfo<v8::Value> &args) {
  SystraceSection s("V8Executor.nativeFlushQueueImmediate");
  if (args.Length() != 1) {
    throw std::invalid_argument("Got wrong number of args");
  }
  _ISOLATE_CONTEXT_ENTER;
  const std::string &queueStr = toJsonStdString(context, Local<Object>::Cast(args[0]));
  LOGI("V8Executor.nativeFlushQueueImmediate %s" , queueStr.c_str());
  m_delegate->callNativeModules(*this, folly::parseJson(std::move(queueStr)), false);
}

void V8Executor::nativeCallSyncHook(const v8::FunctionCallbackInfo<v8::Value> &args) {
  SystraceSection s("V8Executor.nativeCallSyncHook");
  if (args.Length() != 3) {
    throw std::invalid_argument("Got wrong number of args");
  }
  Local<Context> context = Local<Context>::New(GetIsolate(), m_context);
  uint32_t moduleId = Uint32::Cast(*(args[0]))->Value();
  uint32_t methodId = Uint32::Cast(*(args[1]))->Value();
  LOGI("V8Executor.nativeCallSyncHook moduleId %d, methodId %d", moduleId, methodId);
  const std::string &argsJson = toJsonStdString(context, Local<Object>::Cast(args[2]));
  folly::dynamic dynamicArgs = folly::parseJson(std::move(argsJson));
  if (!dynamicArgs.isArray()) {
    throw std::invalid_argument(
        folly::to<std::string>("method parameters should be array, but are ", dynamicArgs.typeName()));
  }

  MethodCallResult result = m_delegate->callSerializableNativeHook(*this, moduleId, methodId, std::move(dynamicArgs));
  if (result.hasValue()) {
    return;
  }

  args.GetReturnValue().Set(fromDynamic(GetIsolate(), context, std::move(result.value())));
}

}
