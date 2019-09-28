LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE    := dash 
LOCAL_SRC_FILES := libdash/obj/local/$(TARGET_ARCH_ABI)/libdash.a
include $(PREBUILT_STATIC_LIBRARY)


include $(CLEAR_VARS)
LOCAL_MODULE    := flv 
LOCAL_SRC_FILES := libflv/obj/local/$(TARGET_ARCH_ABI)/libflv.a
include $(PREBUILT_STATIC_LIBRARY)


include $(CLEAR_VARS)
LOCAL_MODULE    := hls 
LOCAL_SRC_FILES := libhls/obj/local/$(TARGET_ARCH_ABI)/libhls.a
include $(PREBUILT_STATIC_LIBRARY)


include $(CLEAR_VARS)
LOCAL_MODULE    := mov 
LOCAL_SRC_FILES := libmov/obj/local/$(TARGET_ARCH_ABI)/libmov.a
include $(PREBUILT_STATIC_LIBRARY)


include $(CLEAR_VARS)
LOCAL_MODULE    := mpeg 
LOCAL_SRC_FILES := libmpeg/obj/local/$(TARGET_ARCH_ABI)/libmpeg.a
include $(PREBUILT_STATIC_LIBRARY)


include $(CLEAR_VARS)
LOCAL_MODULE    := rtmp 
LOCAL_SRC_FILES := librtmp/obj/local/$(TARGET_ARCH_ABI)/librtmp.a
include $(PREBUILT_STATIC_LIBRARY)


include $(CLEAR_VARS)
LOCAL_MODULE    := rtp 
LOCAL_SRC_FILES := librtp/obj/local/$(TARGET_ARCH_ABI)/librtp.a
include $(PREBUILT_STATIC_LIBRARY)


include $(CLEAR_VARS)
LOCAL_MODULE    := rtsp 
LOCAL_SRC_FILES := librtsp/obj/local/$(TARGET_ARCH_ABI)/librtsp.a
include $(PREBUILT_STATIC_LIBRARY)


include $(CLEAR_VARS)
LOCAL_MODULE    := sip 
LOCAL_SRC_FILES := libsip/obj/local/$(TARGET_ARCH_ABI)/libsip.a
include $(PREBUILT_STATIC_LIBRARY)


include $(CLEAR_VARS)
LOCAL_CFLAGS += -DOS_ANDROID -DOS_LINUX
LOCAL_LDLIBS += -llog -landroid

LOCAL_C_INCLUDES := .
LOCAL_C_INCLUDES += $(LOCAL_PATH)
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../sdk/include

LOCAL_SRC_FILES := test/test.cpp

LOCAL_SHARED_LIBRARIES := 
LOCAL_STATIC_LIBRARIES := 

LOCAL_MODULE := test
include $(BUILD_SHARED_LIBRARY)
