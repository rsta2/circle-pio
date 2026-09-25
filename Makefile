#
# Makefile
#

all:
	$(MAKE) -C circle-stdlib
	$(MAKE) -C driver
	$(MAKE) -C piolib

clean:
	$(MAKE) -C piolib clean
	$(MAKE) -C driver clean
	$(MAKE) -C circle-stdlib mrproper

patch:
	@echo Patching is not necessary any more.

unpatch:
	@echo Patching is not necessary any more.

submodules:
	git submodule update --init circle-stdlib
	cd circle-stdlib/libs && git submodule update --init circle circle-newlib
	cd circle-stdlib/libs/circle && git submodule update --init addon/wlan/hostap
