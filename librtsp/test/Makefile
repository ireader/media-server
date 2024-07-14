ROOT:=../../../sdk
AVCODEC:=../../../avcodec
NOVERSION = 1

#--------------------------------Output------------------------------
# OUTTYPE: 0-exe, 1-dll, 2-static
#--------------------------------------------------------------------
OUTTYPE = 0
OUTFILE = test-rtsp

#-------------------------------Include------------------------------
#
# INCLUDES = $(addprefix -I,$(INCLUDES)) # add -I prefix
#--------------------------------------------------------------------
INCLUDES = . $(ROOT)/include $(ROOT)/libhttp/include $(ROOT)/libaio/include \
			../../libflv/include \
			../../libmov/include \
			../../libmkv/include \
			../../libmpeg/include \
			../../librtp/include \
			../../librtsp/include \
			$(AVCODEC)/avcodec/include \
			$(AVCODEC)/avbsf/include

#-------------------------------Source-------------------------------
#
#--------------------------------------------------------------------
SOURCE_PATHS = ../librtsp/source/sdp ./media
SOURCE_FILES = $(foreach dir,$(SOURCE_PATHS),$(wildcard $(dir)/*.cpp))
SOURCE_FILES += $(foreach dir,$(SOURCE_PATHS),$(wildcard $(dir)/*.c))
SOURCE_FILES += rtsp-server-test.cpp
SOURCE_FILES += rtp-udp-transport.cpp
SOURCE_FILES += ../../libmov/test/mov-file-buffer.c

_SOURCE_FILES = $(ROOT)/libhttp/test/main.c
SOURCE_FILES := $(filter-out $(_SOURCE_FILES),$(SOURCE_FILES))

#-----------------------------Library--------------------------------
#
# LIBPATHS = $(addprefix -L,$(LIBPATHS)) # add -L prefix
#--------------------------------------------------------------------
LIBPATHS = 
ifdef RELEASE
# relase library path
LIBPATHS +=
else
LIBPATHS +=
endif

LIBS = rt pthread dl

STATIC_LIBS = ../../librtsp/$(BUILD).$(PLATFORM)/librtsp.a \
				$(AVCODEC)/avbsf/$(BUILD).$(PLATFORM)/libavbsf.a \
				$(AVCODEC)/avcodec/$(BUILD).$(PLATFORM)/libavcodec.a \
				../../libmpeg/$(BUILD).$(PLATFORM)/libmpeg.a \
				../../libmov/$(BUILD).$(PLATFORM)/libmov.a \
				../../librtp/$(BUILD).$(PLATFORM)/librtp.a \
				../../libflv/$(BUILD).$(PLATFORM)/libflv.a \
				$(ROOT)/libhttp/$(BUILD).$(PLATFORM)/libhttp.a \
				$(ROOT)/libsdk/$(BUILD).$(PLATFORM)/libsdk.a
				

#-----------------------------DEFINES--------------------------------
#
# DEFINES := $(addprefix -D,$(DEFINES)) # add -L prefix
#--------------------------------------------------------------------
DEFINES = RTSP_TEST_MAIN=1 RTSP_SERVER_SOCKET_TEST __ERROR__=00*10000000+__LINE__*1000

include $(ROOT)/gcc.mk

CXXFLAGS += -std=c++0x
