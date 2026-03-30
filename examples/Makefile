#
# Makefile
#

all: examples pico-examples logic_analyser

examples:
	EXAMPLE=ws2812 make -C piolib copy
	EXAMPLE=piotest make -C piolib copy
	EXAMPLE=apitest make -C piolib copy
	EXAMPLE=genseq make -C piolib copy
	EXAMPLE=pwm make -C piolib copy
	EXAMPLE=quadrature_encoder make -C piolib copy
	EXAMPLE=dpi_csync make -C piolib copy
	EXAMPLE=rp1sm make -C piolib copy

.PHONY: pico-examples
pico-examples:
	EXAMPLE=hello make -C pico-examples copy
	EXAMPLE=addition make -C pico-examples copy
	EXAMPLE=pwm make -C pico-examples copy
	EXAMPLE=ws2812 make -C pico-examples copy

.PHONY: logic_analyser
logic_analyser:
	make -C logic_analyser copy

clean:
	make -C piolib clean
	make -C pico-examples clean
	make -C logic_analyser clean
	rm -f ./kernel*.img
