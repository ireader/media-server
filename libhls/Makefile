ROOT:=../../sdk

#--------------------------------Output------------------------------
# OUTTYPE: 0-exe, 1-dll, 2-static
#--------------------------------------------------------------------
OUTTYPE = 2
OUTFILE = libhls.a

#-------------------------------Include------------------------------
#
# INCLUDES = $(addprefix -I,$(INCLUDES)) # add -I prefix
#--------------------------------------------------------------------
INCLUDES = . \
					./include \
					../libhttp/include \
					../libmpeg/include \
					$(ROOT)/include \
					$(ROOT)/video

#-------------------------------Source-------------------------------
#
#--------------------------------------------------------------------
SOURCE_PATHS = source
SOURCE_FILES = $(foreach dir,$(SOURCE_PATHS),$(wildcard $(dir)/*.cpp))
SOURCE_FILES += $(foreach dir,$(SOURCE_PATHS),$(wildcard $(dir)/*.c))

#-----------------------------Library--------------------------------
#
# LIBPATHS = $(addprefix -L,$(LIBPATHS)) # add -L prefix
#--------------------------------------------------------------------
LIBPATHS = ../libhttp/$(BUILD).$(PLATFORM) ../libmpeg/$(BUILD).$(PLATFORM)
ifdef RELEASE
# relase library path
LIBPATHS += 
else
LIBPATHS +=
endif

LIBS = http mpeg

STATIC_LIBS =

#-----------------------------DEFINES--------------------------------
#
# DEFINES := $(addprefix -D,$(DEFINES)) # add -L prefix
#--------------------------------------------------------------------
DEFINES = LIBHLS_EXPORTS OS_MAC

include $(ROOT)/gcc.mk

GCC_VER_GTE44 := $(shell echo `gcc -dumpversion | cut -f1-2 -d.` \< 4.4 | bc )
ifeq ($(GCC_VER_GTE44),1)
CFLAGS += -march=i586
endif
