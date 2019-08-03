RELEASE ?= 0 # default debug
UNICODE ?= 0 # default ansi

ifdef PLATFORM
	CROSS:=$(PLATFORM)-
else 
	CROSS:=
	PLATFORM:=linux
endif

ifeq ($(RELEASE),1)
	BUILD:=release
else
	BUILD:=debug
endif

KERNEL := $(shell uname -s)
ifeq ($(KERNEL),Linux)
	DEFINES += OS_LINUX
endif
ifeq ($(KERNEL),Darwin)
	DEFINES += OS_MAC
endif

#--------------------------------Compile-----------------------------
#
#--------------------------------------------------------------------
AR := $(CROSS)ar
CC := $(CROSS)gcc
CXX := $(CROSS)g++
CFLAGS += -Wall -fPIC
CXXFLAGS += -Wall
DEPFLAGS = -MMD -MP -MF $(OUTPATH)/$(*F).d

ifeq ($(RELEASE),1)
	CFLAGS += -Wall -O2
	CXXFLAGS += $(CFLAGS)
	DEFINES += NDEBUG
else
	CFLAGS += -g -Wall
#	CFLAGS += -fsanitize=address
	CXXFLAGS += $(CFLAGS)
	DEFINES += DEBUG _DEBUG
endif

# default don't export anything
CFLAGS += -fvisibility=hidden

COMPILE.CC = $(CC) $(addprefix -I,$(INCLUDES)) $(addprefix -D,$(DEFINES)) $(CFLAGS)
COMPILE.CXX = $(CXX) $(addprefix -I,$(INCLUDES)) $(addprefix -D,$(DEFINES)) $(CXXFLAGS)

#-------------------------Compile Output---------------------------
#
#--------------------------------------------------------------------
ifeq ($(UNICODE),1)
	OUTPATH += unicode.$(BUILD).$(PLATFORM)
else
	OUTPATH += $(BUILD).$(PLATFORM)
endif

# make output dir
$(shell mkdir -p $(OUTPATH) > /dev/null)

OBJECT_FILES := $(patsubst %.c,%.o,$(patsubst %.cpp,%.o,$(SOURCE_FILES)))
DEPENDENCE_FILES := $(OBJECT_FILES:%.o=%.d)
DEPENDENCE_FILES := $(foreach file,$(DEPENDENCE_FILES),$(OUTPATH)/$(notdir $(file)))

#--------------------------Makefile Rules----------------------------
#
#--------------------------------------------------------------------
$(OUTPATH)/$(OUTFILE): $(OBJECT_FILES) $(STATIC_LIBS)
ifeq ($(OUTTYPE),0)
	$(CXX) -o $@ -Wl,-rpath . $(LDFLAGS) $^ $(addprefix -L,$(LIBPATHS)) $(addprefix -l,$(LIBS))
else
ifeq ($(OUTTYPE),1)
	$(CXX) -o $@ -shared -fpic -rdynamic -Wl,-rpath . $(LDFLAGS) $^ $(addprefix -L,$(LIBPATHS)) $(addprefix -l,$(LIBS))
else
	$(AR) -rc $@ $^
endif
endif
	@echo make ok, output: $(OUTPATH)/$(OUTFILE)

%.o : %.c
	$(COMPILE.CC) -c $(DEPFLAGS) -o $@ $<
	
%.o : %.cpp
	$(COMPILE.CXX) -c $(DEPFLAGS) -o $@ $<
	
-include $(DEPENDENCE_FILES)

version.h : version.ver
	$(ROOT)/svnver.sh version.ver version.h

.PHONY: clean
clean:
	rm -f $(OBJECT_FILES) $(OUTPATH)/$(OUTFILE) $(DEPENDENCE_FILES)
