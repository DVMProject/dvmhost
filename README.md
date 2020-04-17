# Digital Voice Modem Host

The DVM Host software provides the host computer implementation of a mixed-mode DMR/P25 or dedicated-mode DMR or P25 repeater system that talks to the actual modem hardware. The host software; is the portion of a complete Over-The-Air modem implementation that performs the data processing, decision making and FEC correction for a digital repeater.

This project is a direct fork of the MMDVMHost (https://github.com/g4klx/MMDVMHost) project, and combines the MMDVMCal (https://github.com/g4klx/MMDVMCal) project into a single package.

## Building

Please see the various Makefile included in the project for more information. (All following information assumes familiarity with the standard Linux make system.)

The DVM Host software does not have any specific library dependancies and is written to be as library-free as possible. A basic GCC install is usually all thats needed to compile.

* Makefile - This makefile is used for building binaries for the native installed GCC.
* Makefile.arm - This makefile is used for cross-compiling for a ARM platform.

Use the ```make``` command to build the software.

## License

This project is licensed under the GPLv2 License - see the [LICENSE.md](LICENSE.md) file for details. Use of this project is intended, strictly for amateur and educational use ONLY. Any other use is at the risk of user and all commercial purposes are strictly forbidden.

