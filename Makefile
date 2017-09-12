all:
	$(MAKE) -C libflv
	$(MAKE) -C librtmp
	$(MAKE) -C libmpeg
	$(MAKE) -C libhls
	$(MAKE) -C librtp
	$(MAKE) -C librtsp
	$(MAKE) -C libmov
	$(MAKE) -C libdash
	
clean:
	$(MAKE) -C libflv clean
	$(MAKE) -C librtmp clean
	$(MAKE) -C libmpeg clean
	$(MAKE) -C libhls clean
	$(MAKE) -C librtp clean
	$(MAKE) -C librtsp clean
	$(MAKE) -C libmov clean
	$(MAKE) -C libdash clean
