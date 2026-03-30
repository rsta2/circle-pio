#
# Makefile
#

EXAMPLE ?= hello

CIRCLE_PIO_HOME = ../..

OBJS	= main.o kernel.o $(EXAMPLE).o

EXTRACLEAN += *.pio.h

DEFINE += -DPICO_DEFAULT_LED_PIN=4

include $(CIRCLE_PIO_HOME)/Rules.mk

$(EXAMPLE).c: $(EXAMPLE).pio.h

copy: $(TARGET).img
	cp $(TARGET).img ../$(TARGET)-$(EXAMPLE).img
