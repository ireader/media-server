LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_CFLAGS += -DOS_LINUX
LOCAL_LDLIBS +=

LOCAL_C_INCLUDES := $(LOCAL_PATH)
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../sdk/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../sdk/libhttp/include

LOCAL_SRC_FILES += $(wildcard src/*.c)
LOCAL_SRC_FILES += $(wildcard src/*.cpp)
LOCAL_SRC_FILES += $(wildcard src/header/*.c)
LOCAL_SRC_FILES += $(wildcard src/header/*.cpp)
LOCAL_SRC_FILES += $(wildcard src/uac/*.c)
LOCAL_SRC_FILES += $(wildcard src/uac/*.cpp)
LOCAL_SRC_FILES += $(wildcard src/uas/*.c)
LOCAL_SRC_FILES += $(wildcard src/uas/*.cpp)

LOCAL_MODULE := sip
include $(BUILD_STATIC_LIBRARY)
