#
# Rules.mk
#

CIRCLE_PIO_HOME ?= ../..

CIRCLE_STDLIB_DIR ?= $(CIRCLE_PIO_HOME)/circle-stdlib

include $(CIRCLE_STDLIB_DIR)/Config.mk

CIRCLEHOME ?= $(CIRCLE_STDLIB_DIR)/libs/circle
NEWLIBDIR ?= $(CIRCLE_STDLIB_DIR)/install/$(NEWLIB_ARCH)

include $(CIRCLEHOME)/Rules.mk

C_STANDARD = -std=c23

DEFINE += -D_POSIX_C_SOURCE=200809L \
	  -D_GNU_SOURCE \
	  -D__LINUX_ERRNO_EXTENSIONS__

INCLUDE += -I $(CIRCLE_PIO_HOME)/driver \
	   -I $(CIRCLE_PIO_HOME)/piolib/include \
	   -I $(NEWLIBDIR)/include \
	   -I $(CIRCLE_STDLIB_DIR)/include

LIBS += $(CIRCLE_PIO_HOME)/driver/libpiodriver.a \
	$(CIRCLE_PIO_HOME)/piolib/libpio.a \
	$(NEWLIBDIR)/lib/libm.a \
	$(NEWLIBDIR)/lib/libc.a \
	$(NEWLIBDIR)/lib/libcirclenewlib.a \
	$(CIRCLEHOME)/addon/SDCard/libsdcard.a \
	$(CIRCLEHOME)/lib/usb/libusb.a \
	$(CIRCLEHOME)/lib/input/libinput.a \
	$(CIRCLEHOME)/addon/fatfs/libfatfs.a \
	$(CIRCLEHOME)/lib/fs/libfs.a \
	$(CIRCLEHOME)/addon/wlan/hostap/wpa_supplicant/libwpa_supplicant.a \
	$(CIRCLEHOME)/addon/wlan/libwlan.a \
	$(CIRCLEHOME)/lib/net/libnet.a \
	$(CIRCLEHOME)/lib/sched/libsched.a \
	$(CIRCLEHOME)/lib/libcircle.a

-include $(DEPS)
