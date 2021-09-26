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
	$(MAKE) -C libdash
	$(MAKE) -C libflv
	$(MAKE) -C libhls
	$(MAKE) -C libmkv
	$(MAKE) -C libmov
	$(MAKE) -C libmpeg
	$(MAKE) -C librtmp
	$(MAKE) -C librtp
	$(MAKE) -C librtsp
	$(MAKE) -C libsip
	
clean:
	$(MAKE) -C libdash clean
	$(MAKE) -C libflv clean
	$(MAKE) -C libhls clean
	$(MAKE) -C libmkv clean
	$(MAKE) -C libmov clean
	$(MAKE) -C libmpeg clean
	$(MAKE) -C librtmp clean
	$(MAKE) -C librtp clean
	$(MAKE) -C librtsp clean
	$(MAKE) -C libsip clean
	$(MAKE) -C test clean
	
.PHONY : test
test:
	$(MAKE) -C ../avcodec
	$(MAKE) -C ../sdk
	$(MAKE) -C test
	ln -sf ../sdk/libaio/$(BUILD).$(PLATFORM)/libaio.so . &&  ./test/$(BUILD).$(PLATFORM)/test
