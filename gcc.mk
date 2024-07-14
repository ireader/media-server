RELEASE ?= 0 # default debug
UNICODE ?= 0 # default ansi
VERSION ?= 0
FILENAME ?= 1
ARCHBITS ?=  # 32/64 bits

ifeq ($(shell uname -m), x86_64)
	ARCHBITS = 64
else ifeq ($(shell getconf LONG_BIT), 64)
	ARCHBITS = 64
else ifeq ($(shell arch), x86_64)
	ARCHBITS = 64
endif

ifdef PLATFORM
	CROSS:=$(PLATFORM)-
else 
	CROSS:=
	OSID := $(shell awk -F'=' '/^ID=/ {print $$2}' /etc/os-release | tr -d '"')
	OSVERSIONID := $(shell awk -F'=' '/^VERSION_ID=/ {print $$2}' /etc/os-release | tr -d '"')
	PLATFORM:=${OSID}$(OSVERSIONID)-linux$(ARCHBITS)
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

ifeq ($(RELEASE),1)
	CFLAGS += -Wall -O2 -s
	CXXFLAGS += $(CFLAGS)
	DEFINES += NDEBUG
else
	CFLAGS += -g -Wall
#	CFLAGS += -fsanitize=address
	CXXFLAGS += $(CFLAGS)
	DEFINES += DEBUG _DEBUG
endif

ifeq ($(FILENAME),1)
	CFLAGS += -Wno-builtin-macro-redefined -D'__FILE_NAME__="$(notdir $<)"'
endif

ifeq ($(VERSION),1)
	VERSIONFILE = version.o
endif

# default don't export anything
CFLAGS += -fvisibility=hidden

COMPILE.CC = $(CC) $(addprefix -I,$(INCLUDES)) $(addprefix -D,$(DEFINES)) $(CFLAGS)
COMPILE.CXX = $(CXX) $(addprefix -I,$(INCLUDES)) $(addprefix -D,$(DEFINES)) $(CXXFLAGS)

#-------------------------Link---------------------------
#
#--------------------------------------------------------------------
ifeq ($(STATIC_LINK),1)
    LDFLAGS += -static
endif

#-------------------------Compile Output---------------------------
#
#--------------------------------------------------------------------
ifeq ($(UNICODE),1)
	OUTPATH += unicode.$(BUILD).$(PLATFORM)
else
	OUTPATH += $(BUILD).$(PLATFORM)
endif
OBJSPATH = $(OUTPATH)/objs

OBJECT_FILES := $(patsubst %.c,%.o,$(patsubst %.cpp,%.o,$(SOURCE_FILES)))
OBJECT_FILES := $(addprefix $(OBJSPATH)/,$(OBJECT_FILES))
DEPENDENCE_FILES := $(OBJECT_FILES:%.o=%.d)
MKDIR = @mkdir -p $(dir $@)

#--------------------------Makefile Rules----------------------------
#
#--------------------------------------------------------------------
$(OUTPATH)/$(OUTFILE): $(OBJECT_FILES) $(STATIC_LIBS) $(VERSIONFILE) 
ifeq ($(OUTTYPE),0)
	$(CXX) -o $@ -Wl,-rpath=. $(LDFLAGS) $^ $(addprefix -L,$(LIBPATHS)) $(addprefix -l,$(LIBS))
else
ifeq ($(OUTTYPE),1)
	$(CXX) -o $@ -shared -fPIC -rdynamic -Wl,-rpath=. $(LDFLAGS) $^ $(addprefix -L,$(LIBPATHS)) $(addprefix -l,$(LIBS))
else
	@echo -e "\033[35m	AR	$(notdir $@)\033[0m"
	@$(AR) -rc $@ $^
endif
endif
	@echo make ok, output: $(OUTPATH)/$(OUTFILE)

$(OBJSPATH)/%.o : %.c
	$(MKDIR)
	@$(COMPILE.CC) -c -o $@ $<
	@echo -e "\033[35m	CC	$(notdir $@)\033[0m"
	
$(OBJSPATH)/%.o : %.cpp
	$(MKDIR)
	@$(COMPILE.CXX) -c -o $@ $<
	@echo -e "\033[35m	CXX	$(notdir $@)\033[0m"

$(OBJSPATH)/%.d: %.c
	$(MKDIR)
	@echo -e "\033[32m	CREATE	$(notdir $@)\033[0m"
	@rm -f $@; \
	 $(COMPILE.CC) -MM $(CFLAGS) $< > $@.$$$$; \
     sed 's,\($(notdir $*)\)\.o[ :]*,$*\.o $@ : ,g' < $@.$$$$ > $@; \
     rm -f $@.$$$$
$(OBJSPATH)/%.d: %.cpp
	$(MKDIR)
	@echo -e "\033[32m	CREATE	$(notdir $@)\033[0m"
	@rm -f $@; \
	 $(COMPILE.CXX) -MM $(CXXFLAGS) $< > $@.$$$$; \
     sed 's,\($(notdir $*)\)\.o[ :]*,$*\.o $@ : ,g' < $@.$$$$ > $@; \
     rm -f $@.$$$$

ifeq ($(MAKECMDGOALS), clean)
else ifeq ($(MAKECMDGOALS), debug)
else
sinclude $(DEPENDENCE_FILES)
endif

version.o: version.ver $(OBJECT_FILES) $(STATIC_LIBS)
	$(ROOT)/gitver.sh version.ver version.c
	@$(COMPILE.CXX) -c -o $@ version.c

.PHONY: clean
clean:
	@echo -e "\033[35m	 rm -rf *.o  *.d version.h $(OUTPATH)/$(OUTFILE) \033[0m"
	@rm -f $(OBJECT_FILES) $(OUTPATH)/$(OUTFILE) $(DEPENDENCE_FILES)
	@rm -f version.c version.o

debug:
	echo $(OUTPATH)/$(OUTFILE)
	echo $(OBJECT_FILES)
	echo $(DEPENDENCE_FILES)