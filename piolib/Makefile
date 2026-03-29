#
# Makefile
#

CIRCLE_PIO_HOME = ..

OBJS	= piolib.o pio_rp1.o library_piochips.o porting.o

DEFINE	+= -DLIBRARY_BUILD

libpio.a: $(OBJS)
	@echo "  AR    $@"
	@rm -f $@
	@$(AR) cr $@ $(OBJS)

include $(CIRCLE_PIO_HOME)/Rules.mk
