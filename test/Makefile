ROOT:=../../sdk
NOVERSION = 1

#--------------------------------Output------------------------------
# OUTTYPE: 0-exe, 1-dll, 2-static
#--------------------------------------------------------------------
OUTTYPE = 0
OUTFILE = media-server

#-------------------------------Include------------------------------
#
# INCLUDES = $(addprefix -I,$(INCLUDES)) # add -I prefix
#--------------------------------------------------------------------
INCLUDES = . $(ROOT)/include $(ROOT)/libaio/include $(ROOT)/libhttp/include \
			../libdash/include \
			../libflv/include \
			../libhls/include \
			../libmov/include \
			../libmpeg/include \
			../librtmp/include \
			../librtmp/aio \
			../librtp/include \
			../librtsp/include

#-------------------------------Source-------------------------------
#
#--------------------------------------------------------------------
SOURCE_PATHS = . $(ROOT)/source/digest $(ROOT)/libhttp/test \
				../libmpeg/test ../libhls/demo \
				../libdash/test ../libmov/test \
				../libflv/test ../librtmp/aio ../librtmp/test \
				../librtp/test ../librtsp/source/server/aio ../librtsp/test ../librtsp/test/media
SOURCE_FILES = $(foreach dir,$(SOURCE_PATHS),$(wildcard $(dir)/*.cpp))
SOURCE_FILES += $(foreach dir,$(SOURCE_PATHS),$(wildcard $(dir)/*.c))
SOURCE_FILES += $(ROOT)/source/uri-parse.c
SOURCE_FILES += $(ROOT)/source/urlcodec.c
SOURCE_FILES += $(ROOT)/source/ntp-time.c
SOURCE_FILES += $(ROOT)/source/time64.c
SOURCE_FILES += $(ROOT)/source/base64.c
SOURCE_FILES += $(ROOT)/source/unicode.c
SOURCE_FILES += $(ROOT)/source/thread-pool.c

_SOURCE_FILES = $(ROOT)/libhttp/test/main.c
SOURCE_FILES := $(filter-out $(_SOURCE_FILES),$(SOURCE_FILES))

#-----------------------------Library--------------------------------
#
# LIBPATHS = $(addprefix -L,$(LIBPATHS)) # add -L prefix
#--------------------------------------------------------------------
LIBPATHS = $(ROOT)/libaio/$(BUILD).$(PLATFORM)
ifdef RELEASE
# relase library path
LIBPATHS +=
else
LIBPATHS +=
endif

LIBS = rt pthread dl aio

STATIC_LIBS = ../libdash/$(BUILD).$(PLATFORM)/libdash.a \
				../libflv/$(BUILD).$(PLATFORM)/libflv.a \
				../libhls/$(BUILD).$(PLATFORM)/libhls.a \
				../libmov/$(BUILD).$(PLATFORM)/libmov.a \
				../libmpeg/$(BUILD).$(PLATFORM)/libmpeg.a \
				../librtmp/$(BUILD).$(PLATFORM)/librtmp.a \
				../librtsp/$(BUILD).$(PLATFORM)/librtsp.a \
				../librtp/$(BUILD).$(PLATFORM)/librtp.a \
				$(ROOT)/libhttp/$(BUILD).$(PLATFORM)/libhttp.a

#-----------------------------DEFINES--------------------------------
#
# DEFINES := $(addprefix -D,$(DEFINES)) # add -L prefix
#--------------------------------------------------------------------
DEFINES = OS_INT64_TYPE


ifdef HTTP_TEST
SOURCE_FILES += http-test.c
SOURCE_FILES += $(ROOT)/libhttp/test/http-client-test.cpp
SOURCE_FILES += $(ROOT)/libhttp/test/http-client-test2.cpp
DEFINES += HTTP_TEST
INCLUDES += $(ROOT)/libhttp/include
endif

ifdef RTSP_TEST
SOURCE_FILES += sdp-test.c
SOURCE_FILES += rtsp-test.c
SOURCE_FILES += $(ROOT)/../media-server/librtsp/source/rtsp-header-range.c 
SOURCE_FILES += $(ROOT)/../media-server/librtsp/source/rtsp-header-rtp-info.c
SOURCE_FILES += $(ROOT)/../media-server/librtsp/source/rtsp-header-session.c
SOURCE_FILES += $(ROOT)/../media-server/librtsp/source/rtsp-header-transport.c
SOURCE_FILES += $(ROOT)/../media-server/librtsp/source/sdp.c
SOURCE_FILES += $(ROOT)/../media-server/librtsp/source/sdp-a-fmtp.c
SOURCE_FILES += $(ROOT)/../media-server/librtsp/source/sdp-a-rtpmap.c
DEFINES += SDP_TEST
DEFINES += RTSP_TEST
INCLUDES += $(ROOT)/../media-server/librtsp/include
endif

include $(ROOT)/gcc.mk

CXXFLAGS += -std=c++0x
