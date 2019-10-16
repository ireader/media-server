ROOT:=../../sdk
NOVERSION = 1

#--------------------------------Output------------------------------
# OUTTYPE: 0-exe, 1-dll, 2-static
#--------------------------------------------------------------------
OUTTYPE = 0
OUTFILE = test

#-------------------------------Include------------------------------
#
# INCLUDES = $(addprefix -I,$(INCLUDES)) # add -I prefix
#--------------------------------------------------------------------
INCLUDES = . $(ROOT)/include $(ROOT)/libaio/include $(ROOT)/libhttp/include $(ROOT)/libice/include \
			../libdash/include \
			../libflv/include \
			../libhls/include \
			../libmov/include \
			../libmpeg/include \
			../librtmp/include \
			../librtmp/aio \
			../librtp/include \
			../librtsp/include \
			../libsip/include

#-------------------------------Source-------------------------------
#
#--------------------------------------------------------------------
SOURCE_PATHS = . $(ROOT)/source/digest $(ROOT)/libhttp/test \
				../libmpeg/test ../libhls/demo \
				../libdash/test ../libmov/test \
				../libflv/test ../librtmp/aio ../librtmp/test \
				../librtp/test ../librtsp/source/server/aio ../librtsp/source/sdp ../librtsp/test ../librtsp/test/media \
				../libsip/test
SOURCE_FILES = $(foreach dir,$(SOURCE_PATHS),$(wildcard $(dir)/*.cpp))
SOURCE_FILES += $(foreach dir,$(SOURCE_PATHS),$(wildcard $(dir)/*.c))
SOURCE_FILES += $(ROOT)/source/uri-parse.c
SOURCE_FILES += $(ROOT)/source/urlcodec.c
SOURCE_FILES += $(ROOT)/source/ntp-time.c
SOURCE_FILES += $(ROOT)/source/time64.c
SOURCE_FILES += $(ROOT)/source/base64.c
SOURCE_FILES += $(ROOT)/source/unicode.c
SOURCE_FILES += $(ROOT)/source/thread-pool.c
SOURCE_FILES += $(ROOT)/source/app-log.c
SOURCE_FILES += $(ROOT)/source/port/ip-route.c
SOURCE_FILES += $(ROOT)/source/sockpair.c
SOURCE_FILES += $(ROOT)/source/darray.c
SOURCE_FILES += $(ROOT)/source/channel.c
SOURCE_FILES += $(ROOT)/source/uuid.c
SOURCE_FILES += $(ROOT)/source/random.c
SOURCE_FILES += $(ROOT)/source/port/sysnetconfig.c
SOURCE_FILES += $(ROOT)/libice/test/ice-transport.c
SOURCE_FILES += $(ROOT)/deprecated/tools.c

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
				../libsip/$(BUILD).$(PLATFORM)/libsip.a \
				$(ROOT)/libhttp/$(BUILD).$(PLATFORM)/libhttp.a \
				$(ROOT)/libice/$(BUILD).$(PLATFORM)/libice.a

#-----------------------------DEFINES--------------------------------
#
# DEFINES := $(addprefix -D,$(DEFINES)) # add -L prefix
#--------------------------------------------------------------------
DEFINES = OS_INT64_TYPE

include $(ROOT)/gcc.mk

CXXFLAGS += -std=c++0x
