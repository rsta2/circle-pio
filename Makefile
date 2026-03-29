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
	cd circle-stdlib/libs/circle && git checkout --quiet develop

unpatch:
	cd circle-stdlib && git submodule update --quiet libs/circle

submodules:
	git submodule update --init link circle-stdlib
	cd circle-stdlib/libs && git submodule update --init circle circle-newlib
	cd circle-stdlib/libs/circle && git submodule update --init addon/wlan/hostap
