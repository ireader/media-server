LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_CFLAGS += -DOS_LINUX
LOCAL_LDLIBS +=

LOCAL_C_INCLUDES := $(LOCAL_PATH)
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include

LOCAL_SRC_FILES += $(wildcard source/*.c)
LOCAL_SRC_FILES += $(wildcard source/*.cpp)
LOCAL_SRC_FILES += $(wildcard payload/*.c)
LOCAL_SRC_FILES += $(wildcard payload/*.cpp)

LOCAL_MODULE := rtp
include $(BUILD_STATIC_LIBRARY)
