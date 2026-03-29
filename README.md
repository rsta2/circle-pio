circle-pio
==========

The project provides support for the Programmable I/O (PIO) peripheral of the Raspberry Pi 5 for Circle. The project uses circle-stdlib as a submodule.

Building
--------

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
