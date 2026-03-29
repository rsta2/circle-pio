#
# Makefile
#

EXAMPLE ?= ws2812

CIRCLE_PIO_HOME = ../..

PIOLIB_EXAMPLES_HOME ?= $(CIRCLE_PIO_HOME)/piolib/examples

OBJS	= main.o kernel.o $(PIOLIB_EXAMPLES_HOME)/$(EXAMPLE).o

EXTRACLEAN += $(PIOLIB_EXAMPLES_HOME)/*.d $(PIOLIB_EXAMPLES_HOME)/*.o

include $(CIRCLE_PIO_HOME)/Rules.mk

copy: $(TARGET).img
	cp $(TARGET).img ../$(TARGET)-$(EXAMPLE).img
