# Digital Voice Modem Host

The DVM Host software provides the host computer implementation of a mixed-mode DMR/P25 or dedicated-mode DMR or P25 repeater system that talks to the actual modem hardware. The host software; is the portion of a complete Over-The-Air modem implementation that performs the data processing, decision making and FEC correction for a digital repeater.

This project is a direct fork of the MMDVMHost (https://github.com/g4klx/MMDVMHost) project, and combines the MMDVMCal (https://github.com/g4klx/MMDVMCal) project into a single package.

## Building

Please see the various Makefile included in the project for more information. (All following information assumes familiarity with the standard Linux make system.)

The DVM Host software does not have any specific library dependancies and is written to be as library-free as possible. A basic GCC install is usually all thats needed to compile.

* Makefile - This makefile is used for building binaries for the native installed GCC.
* Makefile.arm - This makefile is used for cross-compiling for a ARM platform.

Use the ```make``` command to build the software.

## Notes

Some extra notes for those who are using the Raspberry Pi, default Raspbian OS or Debian OS installations. You will not be able to flash or access the STM32 modem unless you do some things beforehand. 

1. Disable the Bluetooth services. Bluetooth will share the GPIO serial interface on ```/dev/ttyAMA0```. On Rasbian OS or Debian OS, this is done by: ```systemctl disable bluetooth```
2. The default Rasbian OS and Debian OS will have a getty instance listening on ```/dev/ttyAMA0```. This can conflict with the STM32, and is best if disabled. On Rasbian OS or Debian OS, this is done by: ```systemctl disable serial-getty@ttyAMA0.service```
3. There's a default boot option which is also listening on the GPIO serial interface. This **must be disabled**. Open the ```/boot/config.txt``` file in your favorite editor (vi or pico) and change it from:
 ```console=serial0,115200 console=tty1 root=PARTUUID=[this is dynamic per partition] rootfstype=ext4 elevator=deadline fsck.repair=yes rootwait```
 to
 ```console=tty1 root=PARTUUID=[this is dynamic per partition] rootfstype=ext4 elevator=deadline fsck.repair=yes rootwait```
 
 All thats being done is to remove the ```console=serial0,115200``` part. Do not change anything else. Save the file, then reboot.

## License

This project is licensed under the GPLv2 License - see the [LICENSE.md](LICENSE.md) file for details. Use of this project is intended, strictly for amateur and educational use ONLY. Any other use is at the risk of user and all commercial purposes are strictly forbidden.

