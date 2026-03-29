#
# Makefile
#

CIRCLE_PIO_HOME = ..

all: libpiodriver.a

include $(CIRCLE_PIO_HOME)/Rules.mk

OBJS	= rp1mailbox.o rp1firmware.o rp1pio.o

libpiodriver.a: $(OBJS)
	@echo "  AR    $@"
	@rm -f $@
	@$(AR) cr $@ $(OBJS)

CFLAGS += -Wno-narrowing

# using coherent memory region
ifeq ($(strip $(AARCH)),32)
CFLAGS += -mno-unaligned-access
else
CFLAGS += -mstrict-align
endif

-include $(DEPS)
