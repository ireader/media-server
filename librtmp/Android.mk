LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_CFLAGS += -DOS_LINUX
LOCAL_LDLIBS +=

LOCAL_C_INCLUDES := $(LOCAL_PATH)
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../libflv/include

LOCAL_SRC_FILES += $(wildcard source/*.c)
LOCAL_SRC_FILES += $(wildcard source/*.cpp)

LOCAL_MODULE := rtmp
include $(BUILD_STATIC_LIBRARY)
