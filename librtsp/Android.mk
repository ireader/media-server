LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_CFLAGS += -DOS_LINUX
LOCAL_LDLIBS +=

LOCAL_C_INCLUDES := $(LOCAL_PATH)
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../sdk/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../sdk/libhttp/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../librtp/include

LOCAL_SRC_FILES += $(wildcard source/*.c)
LOCAL_SRC_FILES += $(wildcard source/*.cpp)
LOCAL_SRC_FILES += $(wildcard source/client/*.c)
LOCAL_SRC_FILES += $(wildcard source/client/*.cpp)
LOCAL_SRC_FILES += $(wildcard source/server/*.c)
LOCAL_SRC_FILES += $(wildcard source/server/*.cpp)

LOCAL_MODULE := rtsp
include $(BUILD_STATIC_LIBRARY)
