#
# Makefile
#

examples:
	EXAMPLE=ws2812 make -C piolib copy
	EXAMPLE=piotest make -C piolib copy
	EXAMPLE=apitest make -C piolib copy
	EXAMPLE=genseq make -C piolib copy
	EXAMPLE=pwm make -C piolib copy
	EXAMPLE=quadrature_encoder make -C piolib copy
	EXAMPLE=dpi_csync make -C piolib copy
	EXAMPLE=rp1sm make -C piolib copy

clean:
	make -C piolib clean
	rm -f ./kernel*.img
