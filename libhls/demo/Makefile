ROOT:=../../../sdk

#--------------------------------Output------------------------------
# OUTTYPE: 0-exe, 1-dll, 2-static
#--------------------------------------------------------------------
OUTTYPE = 0
OUTFILE = hls-server

#-------------------------------Include------------------------------
#
# INCLUDES = $(addprefix -I,$(INCLUDES)) # add -I prefix
#--------------------------------------------------------------------
INCLUDES = . \
					../include \
					../../libmpeg/include \
					../../libflv/include \
					$(ROOT)/include \
					$(ROOT)/libhttp/include \
					$(ROOT)/libaio/include

#-------------------------------Source-------------------------------
#
#--------------------------------------------------------------------
#SOURCE_PATHS = source
SOURCE_FILES = hls-server.cpp
SOURCE_FILES += $(ROOT)/source/unicode.c
SOURCE_FILES += $(ROOT)/source/urlcodec.c
SOURCE_FILES += $(ROOT)/source/uri-parse.c
SOURCE_FILES += $(ROOT)/source/digest/crc32.c
SOURCE_FILES += $(ROOT)/libhttp/test/http-list-dir.cpp

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

LIBS = rt dl pthread aio

STATIC_LIBS = ../../libhls/$(BUILD).$(PLATFORM)/libhls.a \
				../../libmpeg/$(BUILD).$(PLATFORM)/libmpeg.a \
				../../libflv/$(BUILD).$(PLATFORM)/libflv.a \
                $(ROOT)/libhttp/$(BUILD).$(PLATFORM)/libhttp.a

#-----------------------------DEFINES--------------------------------
#
# DEFINES := $(addprefix -D,$(DEFINES)) # add -L prefix
#--------------------------------------------------------------------
DEFINES = _HLS_SERVER_TEST_

include ../../gcc.mk

GCC_VER_GTE44 := $(shell echo `gcc -dumpversion | cut -f1-2 -d.` \< 4.4 | bc )
ifeq ($(GCC_VER_GTE44),1)
CFLAGS += -march=i586
endif
