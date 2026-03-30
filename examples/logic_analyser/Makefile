#
# Makefile
#

EXAMPLE ?= logic_analyser

CIRCLE_PIO_HOME = ../..

OBJS	= main.o kernel.o $(EXAMPLE).o

include $(CIRCLE_PIO_HOME)/Rules.mk

copy: $(TARGET).img
	cp $(TARGET).img ../$(TARGET)-$(EXAMPLE).img
