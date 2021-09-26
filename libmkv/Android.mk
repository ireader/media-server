LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_CFLAGS += -DOS_LINUX -DOS_ANDROID
LOCAL_LDLIBS +=

LOCAL_C_INCLUDES := $(LOCAL_PATH)
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include

LOCAL_SRC_FILES += $(wildcard src/*.c)
LOCAL_SRC_FILES += $(wildcard src/*.cpp)

LOCAL_MODULE := mkv
include $(BUILD_STATIC_LIBRARY)
