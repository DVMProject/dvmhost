
# Digital Voice Modem Host

The DVM Host software provides the host computer implementation of a mixed-mode DMR/P25 or dedicated-mode DMR or P25 repeater system that talks to the actual modem hardware. The host software; is the portion of a complete Over-The-Air modem implementation that performs the data processing, decision making and FEC correction for a digital repeater.

This project is a direct fork of the MMDVMHost (https://github.com/g4klx/MMDVMHost) project, and combines the MMDVMCal (https://github.com/g4klx/MMDVMCal) project into a single package.

Please feel free to reach out to us for help, comments or otherwise, on our Discord: https://discord.gg/3pBe8xgrEz

## Building

Please see the various Makefile included in the project for more information. (All following information assumes familiarity with the standard Linux make system.)

The DVM Host software does not have any specific library dependancies and is written to be as library-free as possible. A basic GCC install is usually all thats needed to compile.

* Makefile - This makefile is used for building binaries.
    - Use the ARCH parameter to change the architecture. (e.g. ```make ARCH=rpi-arm```)
        - ARCH=arm - Generic ARM compilation with installed cross-compiler tools (see just below).
        - ARCH=rpi-arm - Raspberry Pi ARM compliation using the ```https://github.com/raspberrypi/tools.git``` (see just below).

Use the ```make``` command to build the software.

* For RPi using Debian/Ubuntu OS install the standard ARM embedded toolchain (typically arm-gcc-none-eabi).
  1. Switch to "/opt" and checkout ```https://github.com/raspberrypi/tools.git```.

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

