
# Digital Voice Modem Host

The DVM Host software provides the host computer implementation of a mixed-mode DMR, P25 and/or NXDN or dedicated-mode DMR, P25 or NXDN repeater system that talks to the actual modem hardware. The host software; is the portion of a complete Over-The-Air modem implementation that performs the data processing, decision making and FEC correction for a digital repeater.

This project is a direct fork of the MMDVMHost (https://github.com/g4klx/MMDVMHost) project, and combines the MMDVMCal (https://github.com/g4klx/MMDVMCal) project into a single package.

Please feel free to reach out to us for help, comments or otherwise, on our Discord: https://discord.gg/3pBe8xgrEz

## Building

This project utilizes CMake for its build system. (All following information assumes familiarity with the standard Linux make system.)

The DVM Host software does not have any specific library dependancies and is written to be as library-free as possible. A basic GCC/G++ install is usually all thats needed to compile.

### Build Instructions
1. Clone the repository. ```git clone https://github.com/DVMProject/dvmhost.git```
2. Switch into the "dvmhost" folder. Create a new folder named "build" and switch into it.
	```
	# cd dvmhost
	dvmhost # mkdir build
	dvmhost # cd build
	```
3. Run CMake with any specific options required. (Where [options] is any various compilation options you require.)
	```
	dvmhost/build # cmake [options] ..
	...
	-- Build files have been written to: dvmhost/build
	dvmhost/build # make
	```
If cross-compiling is required (for either ARM 32bit, 64bit or old Raspberry Pi ARM 32bit), the CMake build system has some options:
* ```-DCROSS_COMPILE_ARM=1``` - This will cross-compile dvmhost for ARM 32bit.
* ```-DCROSS_COMPILE_AARCH64=1``` - This will cross-compile dvmhost for ARM 64bit.
* ```-DCROSS_COMPILE_RPI_ARM=1``` - This will cross-compile for old Raspberry Pi ARM 32 bit. (see below)

Please note cross-compliation requires you to have the appropriate development packages installed for your system. For ARM 32-bit, on Debian/Ubuntu OS install the "arm-linux-gnueabihf-gcc" and "arm-linux-gnueabihf-g++" packages. For ARM 64-bit, on Debian/Ubuntu OS install the "aarch64-linux-gnu-gcc" and "aarch64-linux-gnu-g++" packages.

* For old RPi 1 using Debian/Ubuntu OS install the standard ARM embedded toolchain (typically "arm-none-eabi-gcc" and "arm-none-eabi-g++").
  1. Switch to "/opt" and checkout ```https://github.com/raspberrypi/tools.git```.

### Compiled Protocol Options

These are the protocols that are compiled-in to the host for data processing. By default, support for both DMR and P25 protocols are enabled. And, support for the NXDN protocol is disabled. What "compiled-in" support means is whether or not the host will perform *any* processing for the specified protocol (and this is regardless of whether or not the ```config.yml``` has a protocol specified for being enabled or not).

In order to change what protocol support is compiled into the host, these are the CMake options to supply:
* ```-DENABLE_DMR=1``` - This will enable compiled-in DMR protocol support.
* ```-DENABLE_P25=1``` - This will enable compiled-in P25 protocol support.
* ```-DENABLE_NXDN=1``` - This will enable compiled-in NXDN protocol support.

**NXDN Support Note**: NXDN support is currently experimental.

## Configuration

When first setting up a DVM instance, it is required to properly set the "Logical Channel ID" (or LCN ID) data and then calibrate the modem.

### Initial Setup
1. Edit ```config.yml``` and ensure the settings for the modem are correct, find the "modem" section in "system". Check that the uart protocol has the appropriate UART port and port speed set (the config.yml defaults to /dev/ttyUSB0 and 115200).
2. Start ```dvmhost``` as follows: ```/path/to/dvmhost -c /path/to/config.yml --setup```. This will start the dvmhost setup mode.
3. Set the channel ID using the "i" command. This will select the identity table bandplan entry to use for frequency selection. The bandplan information is contained within the ```iden_table.dat``` file. The identity table information will also appear during dvmhost startup like this:
```
M: ... (HOST) Channel Id 0: BaseFrequency = 851006250Hz, TXOffsetMhz = -45.000000MHz, BandwidthKhz = 12.500000KHz, SpaceKhz = 6.250000KHz
M: ... (HOST) Channel Id 1: BaseFrequency = 762006250Hz, TXOffsetMhz = 30.000000MHz, BandwidthKhz = 12.500000KHz, SpaceKhz = 6.250000KHz
M: ... (HOST) Channel Id 15: BaseFrequency = 935001250Hz, TXOffsetMhz = -39.000000MHz, BandwidthKhz = 12.500000KHz, SpaceKhz = 6.250000KHz
M: ... (HOST) Channel Id 2: BaseFrequency = 450000000Hz, TXOffsetMhz = 5.000000MHz, BandwidthKhz = 12.500000KHz, SpaceKhz = 6.250000KHz
```
4. Set the channel number using either the "c" or "f" command. The "f" command is recommended as it will automatically calculate the appropriate channel number from the DVM's transmit frequency.
5. Save the configuration using "s" and quit setup mode with "q".

### Transmit Calibration
1. Start ```dvmhost``` as follows: ```/path/to/dvmhost -c /path/to/config.yml --cal```. This will start the dvmhost calibration mode. The best way to calibrate the DVM is to use a radio from which you can receive and transmit the appropriate test patterns (for example using ASTRO25 Tuner and an XTS radio to use the "Bit Error Rate" functions under Performance Testing).
2. Depending on which protocol you are calibration with, enter DMR BS 1031 Hz Test Pattern (M) or P25 1011 Hz Test Pattern (NAC293 ID1 TG1) (P).
3. Ensure the TXLevel is set to 50 (it should be by default, "`" will display current values, use "T" [increase] and "t" [decrease] if necessary to set it to 50).
4. If the hardware in use has a TX potentiometer, set it to the to minimum level.
5. Start Tx (press spacebar to toggle Tx).
6. While observing the BER via whatever means available, adjust the TX potentiometer for the lowest received BER. If necessary also adjust the software TXLevel for some fine tuning with the "T" (increase) and "t" (decrease).
7. Stop Tx (press spacebar to toggle Tx).
8. Save the configuration using "s" and quit calibration mode with "q".

### Receive Calibration
1. Start ```dvmhost``` as follows: ```/path/to/dvmhost -c /path/to/config.yml --cal```. This will start the dvmhost calibration mode. The best way to calibrate the DVM is to use a radio from which you can receive and transmit the appropriate test patterns (for example using ASTRO25 Tuner and an XTS radio to use the "Transmitter Test Pattern" functions under Performance Testing).
2. Depending on which protocol you are calibration with, enter DMR BS 1031 Hz Test Pattern (M) or P25 1011 Hz Test Pattern (P).
3. Ensure the RXLevel is set to 50 (it should be by default, "`" will display current values, use "R" [increase] and "r" [decrease] if necessary to set it to 50).
4. If the hardware in use has a RX potentiometer, set it to the to minimum level. (If using something like the RepeaterBuilder STM32 board, decrease both the coarse and fine potentiometers to minimum level.)
5. Depending on which protocol you are calibration with, enter DMR MS 1031 Hz Test Pattern (J) or P25 1011 Hz Test Pattern (j).
6. While observing the BER via the calibration console, adjust the RX potentiometer(s) for the lowest received BER. If necessary also adjust the software RXLevel for some fine tuning with the "R" (increase) and "r" (decrease).
7. Save the configuration using "s" and quit calibration mode with "q".

### Calibration Notes
* During Transmit Calibration, it may be necessary to adjust the symbol levels directly. Normally this isn't required as the DVM will just work, but some radios require some fine adjustment of the symbol levels, this is exposed in the calibration mode.
* Unusually high BER >10% and other various receive problems may be due to the radio/hotspot being off frequency and requiring some adjustment. Even a slight frequency drift can be catastrophic for proper digital modulation. The recommendation is to ensure the interfaced radio does not have an overall reference frequency drift > +/- 150hz. An unusually high BER can also be explained by DC level offsets in the signal paths, or issues with the FM deviation levels on the interfaced radio being too high or too low.

## Command Line Parameters

```usage: ./dvmhost [-vh] [-f] [--cal] [--setup] [-c <configuration file>] [--remote [-a <address>] [-p <port>]]

  -f        foreground mode
  --cal     calibration mode
  --setup   setup mode

  -c <file> specifies the configuration file to use

  --remote  remote modem mode
  -a        remote modem command address
  -p        remote modem command port

  -v        show version information
  -h        show this screen
  --        stop handling options```

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

This project is licensed under the GPLv2 License - see the [LICENSE.md](LICENSE.md) file for details. Use of this project is intended, for amateur and/or educational use ONLY. Any other use is at the risk of user and all commercial purposes is strictly discouraged.

