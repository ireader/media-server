LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_CFLAGS += -DOS_LINUX -DOS_ANDROID
LOCAL_LDLIBS +=

LOCAL_C_INCLUDES := $(LOCAL_PATH)
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../libmov/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../libmpeg/include

LOCAL_SRC_FILES := $(wildcard source/*.c)
LOCAL_SRC_FILES += $(wildcard source/*.cpp)

LOCAL_MODULE := hls
include $(BUILD_STATIC_LIBRARY)
