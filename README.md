circle-pio
==========

> Raspberry Pi is a trademark of Raspberry Pi Ltd.

The project provides support for the programmable input/output block (PIO) in the Raspberry Pi 5 within Circle. It uses circle-stdlib as a submodule.

The following links refer to PIO support for Raspberry Pi OS and for the RP2040 Microcontroller. This information can help to program the PIO in Circle.

* [PIOLib: A userspace library for PIO control](https://www.raspberrypi.com/news/piolib-a-userspace-library-for-pio-control)
* [RP2040 datasheet with PIO section](https://pip.raspberrypi.com/documents/RP-008371-DS-1-rp2040-datasheet.pdf)
* [hardware_pio API documentation in Pico C SDK](https://www.raspberrypi.com/documentation/pico-sdk/hardware.html#group_hardware_pio)

Build
-----

This project builds best on a Linux host. You need to install a bare-metal toolchain from [here](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads). The build has been tested with GCC version *14.3.Rel1* from this website.

You can download the circle-pio source code and the necessary submodule using:

```
git clone https://github.com/rsta2/circle-pio.git
cd circle-pio
make submodules
```

Currently the latest Circle version has to be fetched with:

```
make patch
```

Then configure and build circle-stdlib. The `-p` option must be applied with an absolute path to the toolchain binaries, if they are not in the `PATH` environment variable.

```
./configure -r 5 -p aarch64-none-elf-
make -j
```

Now you can go to the examples directory and build it:

```
cd examples
make -j
```

The respective kernel image has to be copied to the SD card as *kernel_2712.img*.

Cleanup project with:

```
make clean
make unpatch
```
