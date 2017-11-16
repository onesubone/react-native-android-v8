LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE:= v8platform
LOCAL_SRC_FILES := ../v8/jni/$(TARGET_ARCH_ABI)/libv8_libplatform.cr.so
include $(PREBUILT_SHARED_LIBRARY)
