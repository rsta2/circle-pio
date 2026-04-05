#
# Makefile
#

EXAMPLE ?= logic_analyser2

CIRCLE_PIO_HOME = ../..

OBJS	= main.o kernel.o webserver.o recorder.o logic_analyser.o

CONTENT	= index.h style.h favicon.h

EXTRACLEAN = $(CONTENT)

include $(CIRCLE_PIO_HOME)/Rules.mk

%.h: %.html
	@echo "  GEN   $@"
	@$(CIRCLEHOME)/tools/converttool $< > $@

%.h: %.css
	@echo "  GEN   $@"
	@$(CIRCLEHOME)/tools/converttool $< > $@

%.h: %.ico
	@echo "  GEN   $@"
	@$(CIRCLEHOME)/tools/converttool -b $< > $@

webserver.o: $(CONTENT)

copy: $(TARGET).img
	cp $(TARGET).img ../$(TARGET)-$(EXAMPLE).img
