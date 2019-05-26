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

all:
	$(MAKE) -C libflv
	$(MAKE) -C librtmp
	$(MAKE) -C libmpeg
	$(MAKE) -C libhls
	$(MAKE) -C librtp
	$(MAKE) -C librtsp
	$(MAKE) -C libmov
	$(MAKE) -C libdash
	$(MAKE) -C libsip
	
clean:
	$(MAKE) -C libflv clean
	$(MAKE) -C librtmp clean
	$(MAKE) -C libmpeg clean
	$(MAKE) -C libhls clean
	$(MAKE) -C librtp clean
	$(MAKE) -C librtsp clean
	$(MAKE) -C libmov clean
	$(MAKE) -C libdash clean
	$(MAKE) -C libsip clean
	$(MAKE) -C test clean
	
.PHONY : test
test:
	$(MAKE) -C ../sdk
	$(MAKE) -C test
	ln -sf ../sdk/libaio/$(BUILD).$(PLATFORM)/libaio.so . &&  ./test/$(BUILD).$(PLATFORM)/test
