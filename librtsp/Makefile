ROOT:=../../sdk

#--------------------------------Output------------------------------
# OUTTYPE: 0-exe, 1-dll, 2-static
#--------------------------------------------------------------------
OUTTYPE = 2
OUTFILE = librtsp.a

#-------------------------------Include------------------------------
#
# INCLUDES = $(addprefix -I,$(INCLUDES)) # add -I prefix
#--------------------------------------------------------------------
INCLUDES = . \
					./include \
					$(ROOT)/include \
					$(ROOT)/libhttp/include \
					../librtp/include \
					../libmpeg/include

#-------------------------------Source-------------------------------
#
#--------------------------------------------------------------------
SOURCE_PATHS = source client media
SOURCE_FILES = $(foreach dir,$(SOURCE_PATHS),$(wildcard $(dir)/*.cpp))
SOURCE_FILES += $(foreach dir,$(SOURCE_PATHS),$(wildcard $(dir)/*.c))

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

LIBS = 

STATIC_LIBS =

#-----------------------------DEFINES--------------------------------
#
# DEFINES := $(addprefix -D,$(DEFINES)) # add -L prefix
#--------------------------------------------------------------------
DEFINES = OS_INT64_TYPE

include $(ROOT)/gcc.mk
