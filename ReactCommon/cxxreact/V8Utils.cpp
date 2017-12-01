//
// Created by hanyanan on 2017/1/22.
//

#include "V8Utils.h"

namespace v8 {

Local<String> toLocalString(Isolate *isolate, const char *string) {
    return String::NewFromUtf8(isolate, std::move(string));
}

Local<String> toLocalString(Isolate *isolate, const std::string &string) {
    return String::NewFromUtf8(isolate, string.c_str(), NewStringType::kNormal,
                        static_cast<int>(string.length())).ToLocalChecked();
}

Local<String> toLocalString(Isolate *isolate, const facebook::react::JSBigString &bigString) {
    return String::NewFromUtf8(isolate, std::move(bigString.c_str()));
}

std::string toStdString(const Local<String> &string) {
    int length = string->Length();
    char *buff = new char[length + 1];
    buff[length] = '\0';
    string->WriteUtf8(buff, length);
    return std::string(buff);
}

std::string toJsonStdString(Local<Context> context, const Local<Object> &object) {
    Local<String> jsonStr = JSON::Stringify(context, std::move(object)).ToLocalChecked();
    return toStdString(std::move(jsonStr));
}

Local<String> toJsonString(Local<Context> context, const Local<Object> &object) {
    return JSON::Stringify(context, std::move(object)).ToLocalChecked();
}

Local<Value> fromJsonString(Isolate *isolate, Local<Context> context, const std::string &jsonStr) {
    Local<String> res = String::NewFromUtf8(isolate, std::move(jsonStr.c_str()));
    return JSON::Parse(context, std::move(res)).ToLocalChecked();
}

Local<Value> fromJsonString(Isolate *isolate, Local<Context> context, const char *jsonStr) {
    Local<String> res = String::NewFromUtf8(isolate, std::move(jsonStr));
    return JSON::Parse(context, std::move(res)).ToLocalChecked();
}

Local<Value> fromJsonString(Isolate *isolate, Local<Context> context, const char *jsonStr, int length) {
    Local<String> res = String::NewFromUtf8(isolate, std::move(jsonStr), v8::NewStringType::kNormal, length).ToLocalChecked();
    return JSON::Parse(context, std::move(res)).ToLocalChecked();
}

Local<Value> fromJsonString(Local<Context> context, const Local<String> &jsonStr) {
    return JSON::Parse(context, std::move(jsonStr)).ToLocalChecked();
}

Local<Value> fromDynamic(Isolate *isolate, Local<v8::Context> context, const folly::dynamic &value) {
    const std::string &json = folly::toJson(std::move(value));
    Local<Value> result;
    if (v8::JSON::Parse(context, toLocalString(isolate, std::move(json))).ToLocal(&result)) {
        return result;
    }
    return Local<Value>();
}

Local<Value> safeToLocal(const MaybeLocal<Value> &maybeLocal) {
    Local<Value> res;
    if(maybeLocal.ToLocal(&res)) {
        return res;
    }
    return Local<Value>();
}

void nativeLog(const FunctionCallbackInfo<Value> &args) {
    android_LogPriority logLevel = ANDROID_LOG_DEBUG;
    if (args.Length() > 1) {
        int32_t level = Int32::Cast(*(args[1]))->Value();
        // The lowest log level we get from JS is 0. We shift and cap it to be
        // in the range the Android logging method expects.
        logLevel = std::min(static_cast<android_LogPriority>(level + ANDROID_LOG_DEBUG), ANDROID_LOG_FATAL);
    }
    if (args.Length() > 0) {
        Local<String> log = Local<String>::Cast(args[0]);
        const std::string &str = toStdString(log);
        __android_log_print(logLevel, "ReactNativeJS", "%s", str.c_str());
    }
}


std::pair<Local<Uint32>, Local<Uint32>> parseNativeRequireParameters(const v8::FunctionCallbackInfo<v8::Value> &args) {
    Local<Uint32> moduleId, bundleId;
    Isolate* isolate = args.GetIsolate();
    Local<Context> context = isolate->GetCurrentContext();
    if (args.Length() == 1) {
        moduleId = Local<Uint32>::Cast(args[0]);
    } else if (args.Length() == 2) {
        moduleId = Local<Uint32>::Cast(args[0]);
        bundleId = Local<Uint32>::Cast(args[1]);
    } else {
        throw std::invalid_argument("Got wrong number of args");
    }

    if (moduleId->Value() < 0) {
        throw std::invalid_argument(folly::to<std::string>("Received invalid module ID: ", toJsonStdString(context,Local<Object>::Cast(args[0]))));
    }

    if (bundleId->Value() < 0) {
        throw std::invalid_argument(folly::to<std::string>("Received invalid bundle ID: ", toJsonStdString(context,Local<Object>::Cast(args[1]))));
    }

    return std::make_pair(static_cast<Local<Uint32>>(bundleId), static_cast<Local<Uint32>>(moduleId));
}




void printType(Local<Value> value, const char *desc) {
    if (!*value) {
        LOGI("V8Executor  %s is null", desc);
        return;
    }
    if (value->IsFunction()) {
        LOGI("V8Executor  %s is a IsFunction", desc);
    }
    if (value->IsObject()) {
        LOGI("V8Executor  %s is a object", desc);
    }
    if (value->IsArgumentsObject()) {
        LOGI("V8Executor  %s is a IsArgumentsObject", desc);
    }
    if (value->IsArray()) {
        LOGI("V8Executor  %s is a IsArray", desc);
    }
    if (value->IsArrayBuffer()) {
        LOGI("V8Executor  %s is a IsArrayBuffer", desc);
    }
    if (value->IsAsyncFunction()) {
        LOGI("V8Executor  %s is a IsAsyncFunction", desc);
    }
    if (value->IsBoolean()) {
        LOGI("V8Executor  %s is a IsBoolean", desc);
    }
    if (value->IsBooleanObject()) {
        LOGI("V8Executor  %s is a IsBooleanObject", desc);
    }
    if (value->IsDataView()) {
        LOGI("V8Executor  %s is a IsDataView", desc);
    }
    if (value->IsDate()) {
        LOGI("V8Executor  %s is a IsDate", desc);
    }
    if (value->IsExternal()) {
        LOGI("V8Executor  %s is a IsExternal", desc);
    }
    if (value->IsFloat32Array()) {
        LOGI("V8Executor  %s is a IsFloat32Array", desc);
    }
    if (value->IsGeneratorFunction()) {
        LOGI("V8Executor  %s is a IsGeneratorFunction", desc);
    }
    if (value->IsGeneratorObject()) {
        LOGI("V8Executor  %s is a IsGeneratorObject", desc);
    }
    if (value->IsInt8Array()) {
        LOGI("V8Executor  %s is a IsInt8Array", desc);
    }
    if (value->IsInt16Array()) {
        LOGI("V8Executor  %s is a IsInt16Array", desc);
    }
    if (value->IsFloat64Array()) {
        LOGI("V8Executor  %s is a IsFloat64Array", desc);
    }
    if (value->IsInt32()) {
        LOGI("V8Executor  %s is a IsInt32", desc);
    }
    if (value->IsInt32Array()) {
        LOGI("V8Executor  %s is a IsInt32Array", desc);
    }
    if (value->IsMap()) {
        LOGI("V8Executor  %s is a IsMap", desc);
    }
    if (value->IsMapIterator()) {
        LOGI("V8Executor  %s is a IsMapIterator", desc);
    }
    if (value->IsNumber()) {
        LOGI("V8Executor  %s is a IsNumber", desc);
    }
    if (value->IsNativeError()) {
        LOGI("V8Executor  %s is a IsNativeError", desc);
    }
    if (value->IsNumberObject()) {
        LOGI("V8Executor  %s is a IsNumberObject", desc);
    }
    if (value->IsName()) {
        LOGI("V8Executor  %s is a IsName", desc);
    }
    if (value->IsNull()) {
        LOGI("V8Executor  %s is a IsNull", desc);
    }
    if (value->IsPromise()) {
        LOGI("V8Executor  %s is a IsPromise", desc);
    }
    if (value->IsProxy()) {
        LOGI("V8Executor  %s is a IsProxy", desc);
    }
    if (value->IsSet()) {
        LOGI("V8Executor  %s is a IsSet", desc);
    }
    if (value->IsSetIterator()) {
        LOGI("V8Executor  %s is a IsSetIterator", desc);
    }
    if (value->IsString()) {
        LOGI("V8Executor  %s is a IsString", desc);
    }
    if (value->IsStringObject()) {
        LOGI("V8Executor  %s is a IsStringObject", desc);
    }
    if (value->IsSymbol()) {
        LOGI("V8Executor  %s is a IsSymbol", desc);
    }
    if (value->IsSymbolObject()) {
        LOGI("V8Executor  %s is a IsSymbolObject", desc);
    }
    if (value->IsTypedArray()) {
        LOGI("V8Executor  %s is a IsTypedArray", desc);
    }
    if (value->IsUint8Array()) {
        LOGI("V8Executor  %s is a IsUint8Array", desc);
    }
    if (value->IsUint8ClampedArray()) {
        LOGI("V8Executor  %s is a IsUint8ClampedArray", desc);
    }
    if (value->IsUint16Array()) {
        LOGI("V8Executor  %s is a IsUint16Array", desc);
    }
    if (value->IsUint32()) {
        LOGI("V8Executor  %s is a IsUint32", desc);
    }
    if (value->IsUint32Array()) {
        LOGI("V8Executor  %s is a IsUint32Array", desc);
    }
    if (value->IsUndefined()) {
        LOGI("V8Executor  %s is a IsUndefined", desc);
    }
    if (value->IsWeakMap()) {
        LOGI("V8Executor  %s is a IsWeakMap", desc);
    }
    if (value->IsWeakSet()) {
        LOGI("V8Executor  %s is a IsWeakSet", desc);
    }
    if (value->IsWebAssemblyCompiledModule()) {
        LOGI("V8Executor  %s is a IsWebAssemblyCompiledModule", desc);
    }
}

}