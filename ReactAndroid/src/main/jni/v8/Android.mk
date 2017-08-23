LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := v8
LOCAL_SRC_FILES := jni/$(TARGET_ARCH_ABI)/libv8.cr.so
include $(PREBUILT_SHARED_LIBRARY)

