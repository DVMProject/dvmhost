# Digital Voice Modem Host

The DVM Host software provides the host computer implementation of a mixed-mode DMR, P25 and/or NXDN or dedicated-mode DMR, P25 or NXDN repeater system that talks to the actual modem hardware. The host software; is the portion of a complete Over-The-Air modem implementation that performs the data processing, decision making and FEC correction for a digital repeater.

This project is a direct fork of the MMDVMHost (https://github.com/g4klx/MMDVMHost) project, and combines the MMDVMCal (https://github.com/g4klx/MMDVMCal) project into a single package.

Please feel free to reach out to us for help, comments or otherwise, on our Discord: https://discord.gg/3pBe8xgrEz

The main executable program for this project is `dvmhost`, this is the host software that connects to the DVM modems (both repeater and hotspot). However, it is important to note this project will also contains the following:

- `dvmcmd` a simple command-line utility to send remote control commands to a DVM host instance with REST API configured.
- `dvmmon` a TUI utility that allows semi-realtime console-based monitoring of DVM host instances (this tool is only available when project wide TUI support is enabled!).
- `dvmfne` a conference bridge FNE. (See the Conference Bridge FNE section below.)

## Building

This project utilizes CMake for its build system. (All following information assumes familiarity with the standard Linux make system.)

The DVM Host software requires the library dependancies below. Generally, the software attempts to be as portable as possible and as library-free as possible. A basic GCC/G++ install, with libasio and ncurses is usually all that is needed to compile.

### Dependencies

`apt-get install libasio-dev libncurses-dev`

- ASIO Library (https://think-async.com/Asio/); on Debian/Ubuntu Linux's: `apt-get install libasio-dev`
- ncurses; on Debian/Ubuntu Linux's: `apt-get install libncurses-dev`

Alternatively, if you download the ASIO library from the ASIO website and extract it to a location, you can specify the path to the ASIO library using: `-DWITH_ASIO=/path/to/asio`. This method is required when cross-compiling for old Raspberry Pi ARM 32 bit.

If cross-compiling ensure you install the appropriate libraries, for example for AARCH64/ARM64:
```
sudo dpkg --add-architecture arm64
sudo apt-get update
sudo apt-get install libasio-dev:arm64 libncurses-dev:arm64
```

### Build Instructions

1. Clone the repository. `git clone https://github.com/DVMProject/dvmhost.git`
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

- `-DCROSS_COMPILE_ARM=1` - This will cross-compile dvmhost for generic ARM 32bit. (RPi4 running 32-bit distro's can fall into this category [on Debian/Rasbpian anything bullseye or newer])
- `-DCROSS_COMPILE_AARCH64=1` - This will cross-compile dvmhost for generic ARM 64bit. (RPi4 running 64-bit distro's can fall into this category [on Debian/Rasbpian anything bullseye or newer])
- `-DCROSS_COMPILE_RPI_ARM=1` - This will cross-compile for old Raspberry Pi ARM 32 bit. (typically this will be the RPi1, 2 and 3 platforms; see build notes, linked below)

Please note cross-compliation requires you to have the appropriate development packages installed for your system. For ARM 32-bit, on Debian/Ubuntu OS install the "arm-linux-gnueabihf-gcc" and "arm-linux-gnueabihf-g++" packages. For ARM 64-bit, on Debian/Ubuntu OS install the "aarch64-linux-gnu-gcc" and "aarch64-linux-gnu-g++" packages.

[See build notes](#build-notes).

### Setup TUI (Text-based User Interface)

Since, DVM Host 3.5, the old calibration and setup modes have been deprecated in favor of a ncurses-based TUI. This TUI is optional, and DVM Host can still be compiled without it for systems or devices that cannot utilize it.

- `-DENABLE_SETUP_TUI=0` - This will disable the setup/calibration TUI interface.
- `-DENABLE_TUI_SUPPORT=0` - This will disable TUI support project wide. Any projects that require TUI support will not compile, or will have any TUI components disabled.

### Compiled Protocol Options

These are the protocols that are compiled-in to the host for data processing. By default, support for DMR, P25 and NXDN protocols are enabled. What "compiled-in" support means is whether or not the host will perform _any_ processing for the specified protocol (and this is regardless of whether or not the `config.yml` has a protocol specified for being enabled or not).

In order to change what protocol support is compiled into the host, these are the CMake options to supply:

- `-DENABLE_DMR=1` - This will enable compiled-in DMR protocol support.
- `-DENABLE_P25=1` - This will enable compiled-in P25 protocol support.
- `-DENABLE_NXDN=1` - This will enable compiled-in NXDN protocol support.

## Configuration

When first setting up a DVM instance, it is required to properly set the "Logical Channel ID" (or LCN ID) data and then calibrate the modem.

### Initial Setup

The following setups assume the host is compiled with the setup TUI mode (if availble). It is possible to setup the modem without the setup TUI, and requires manually modifying `config.yml` and the `iden_table.dat` files.

1. Edit `config.yml` and ensure the settings for the modem are correct, find the "modem" section in "system". Check that the uart protocol has the appropriate UART port and port speed set (the config.yml defaults to /dev/ttyUSB0 and 115200).
2. Start `dvmhost` as follows: `/path/to/dvmhost -c /path/to/config.yml --setup`. This will start the dvmhost setup TUI mode.
3. Using the TUI user interface, use the "Setup" menu to set default parameters.
    3.1. The "Logging & Data Configuration" submenu allows you to alter the various logging file paths and levels, as well as paths to data files (such as the `iden_table.dat` file).
    3.2. The "System Configuration" submenu allows you to alter various modem port and speed, system settings, and mode settings configuration.
    3.3. The "Site Parameters" submenu allows you to alter various CW morse identification, and site parameters.
    3.4. The "Channel Configuration" submenu allows you to alter the configured channel for the modem you are configuring.
4. After altering settings, use the "File" menu, "Save Settings" menu option to save the desired configuration.
5. Quit setup mode (some settings changes require a restart of the software to be effective) using, "File" menu, "Quit".

### Transmit Calibration (using setup TUI, if available)

1. Start `dvmhost` as follows: `/path/to/dvmhost -c /path/to/config.yml --setup`. This will start the dvmhost setup TUI mode. The best way to calibrate the DVM is to use a radio from which you can receive and transmit the appropriate test patterns (for example using ASTRO25 Tuner and an XTS radio to use the "Bit Error Rate" functions under Performance Testing).
2. Depending on which protocol you are calibration with, use the "Calibrate" menu, and select the appropriate mode using the "Operational Mode" submenu. (For example, select [Tx] DMR BS 1031 Hz Test Pattern for DMR or [Tx] P25 1011 Hz Test Pattern (NAC293 ID1 TG1) for P25.)
3. Open the "Level Adjustment" window by either, using the "Calibrate" menu and selecting "Level Adjustment" or if capable, pressing F5 on the keyboard.
4. Ensure the TX Level is set to 50 (it should be by default, you can use the spinbox in the "Level Adjustment" window to change the value, if necessary to set it to 50).
5. If the hardware in use has a TX potentiometer, set it to the to minimum level.
6. Start Tx (click "Transmit" or press F12 on the keyboard).
7. While observing the BER via whatever means available, adjust the TX potentiometer for the lowest received BER. If necessary also adjust the software TX Level for some fine tuning with the spinbox in the "Level Adjustment" window.
8. Stop Tx (click "Transmit" or press F12 on the keyboard).
9. After altering settings, use the "File" menu, "Save Settings" menu option to save the desired configuration.
10. Quit setup mode, if done doing calibration, using, "File" menu, "Quit".

### Transmit Calibration (using old calibration CLI)

1. Start `dvmhost` as follows: `/path/to/dvmhost -c /path/to/config.yml --cal`. This will start the dvmhost calibration mode. The best way to calibrate the DVM is to use a radio from which you can receive and transmit the appropriate test patterns (for example using ASTRO25 Tuner and an XTS radio to use the "Bit Error Rate" functions under Performance Testing).
2. Depending on which protocol you are calibration with, enter DMR BS 1031 Hz Test Pattern (M) or P25 1011 Hz Test Pattern (NAC293 ID1 TG1) (P).
3. Ensure the TXLevel is set to 50 (it should be by default, "\`" will display current values, use "T" [increase] and "t" [decrease] if necessary to set it to 50).
4. If the hardware in use has a TX potentiometer, set it to the to minimum level.
5. Start Tx (press spacebar to toggle Tx).
6. While observing the BER via whatever means available, adjust the TX potentiometer for the lowest received BER. If necessary also adjust the software TXLevel for some fine tuning with the "T" (increase) and "t" (decrease).
7. Stop Tx (press spacebar to toggle Tx).
8. Save the configuration using "s" and quit calibration mode with "q".

### Receive Calibration (using setup TUI, if available)

1. Start `dvmhost` as follows: `/path/to/dvmhost -c /path/to/config.yml --setup`. This will start the dvmhost setup TUI mode. The best way to calibrate the DVM is to use a radio from which you can receive and transmit the appropriate test patterns (for example using ASTRO25 Tuner and an XTS radio to use the "Transmitter Test Pattern" functions under Performance Testing).
2. Depending on which protocol you are calibration with, use the "Calibrate" menu, and select the appropriate mode using the "Operational Mode" submenu. (For example, select [Rx] DMR BS 1031 Hz Test Pattern for DMR or [Rx] P25 1011 Hz Test Pattern (NAC293 ID1 TG1) for P25.)
3. Open the "Level Adjustment" window by either, using the "Calibrate" menu and selecting "Level Adjustment" or if capable, pressing F5 on the keyboard.
4. Ensure the RX Level is set to 50 (it should be by default, you can use the spinbox in the "Level Adjustment" window to change the value, if necessary to set it to 50).
5. If the hardware in use has a RX potentiometer, set it to the to minimum level. (If using something like the RepeaterBuilder STM32 board, decrease both the coarse and fine potentiometers to minimum level.)
7. While observing the BER via the setup TUI (Receive BER shows a large window in the top-right corner of the TUI when in a Rx BER test mode), adjust the RX potentiometer(s) for the lowest received BER. If necessary also adjust the software RX Level for some fine tuning with the spinbox in the "Level Adjustment" window.
8. After altering settings, use the "File" menu, "Save Settings" menu option to save the desired configuration.
9. Quit setup mode, if done doing calibration, using, "File" menu, "Quit".

### Receive Calibration (using old calibration CLI)

1. Start `dvmhost` as follows: `/path/to/dvmhost -c /path/to/config.yml --cal`. This will start the dvmhost calibration mode. The best way to calibrate the DVM is to use a radio from which you can receive and transmit the appropriate test patterns (for example using ASTRO25 Tuner and an XTS radio to use the "Transmitter Test Pattern" functions under Performance Testing).
2. Depending on which protocol you are calibration with, enter DMR BS 1031 Hz Test Pattern (M) or P25 1011 Hz Test Pattern (P).
3. Ensure the RXLevel is set to 50 (it should be by default, "\`" will display current values, use "R" [increase] and "r" [decrease] if necessary to set it to 50).
4. If the hardware in use has a RX potentiometer, set it to the to minimum level. (If using something like the RepeaterBuilder STM32 board, decrease both the coarse and fine potentiometers to minimum level.)
5. Depending on which protocol you are calibration with, enter DMR MS 1031 Hz Test Pattern (J) or P25 1011 Hz Test Pattern (j).
6. While observing the BER via the calibration console, adjust the RX potentiometer(s) for the lowest received BER. If necessary also adjust the software RXLevel for some fine tuning with the "R" (increase) and "r" (decrease).
7. Save the configuration using "s" and quit calibration mode with "q".

### Calibration Notes

- During Transmit Calibration, it may be necessary to adjust the symbol levels directly. Normally this isn't required as the DVM will just work, but some radios require some fine adjustment of the symbol levels, this is exposed in the calibration mode.
- Unusually high BER >10% and other various receive problems may be due to the radio/hotspot being off frequency and requiring some adjustment. Even a slight frequency drift can be catastrophic for proper digital modulation. The recommendation is to ensure the interfaced radio does not have an overall reference frequency drift > +/- 150hz. An unusually high BER can also be explained by DC level offsets in the signal paths, or issues with the FM deviation levels on the interfaced radio being too high or too low.

## Command Line Parameters

```
usage: ./dvmhost [-vhf] [--setup] [-c <configuration file>] [--remote [-a <address>] [-p <port>]]

  -v        show version information
  -h        show this screen
  -f        foreground mode

  --setup   setup mode

  -c <file> specifies the configuration file to use

  --remote  remote modem mode
  -a        remote modem command address
  -p        remote modem command port

  --        stop handling options
```

## Conference Bridge FNE

The DVMHost project will also build `dvmfne` which is a simple conference bridge style FNE. This FNE uses its own configuration file (see `fne-config.example.yml`). 

The conference bridge FNE is a simplistic FNE, meant for simple single-master small-scale deployments. It, like the full-scale FNE, defines rules for available talkgroups and manages calls. Unlike the full-scale FNE, the conference bridge FNE does not have multi-system routing or support multiple masters. It can peer to other FNEs, however, unlike full-scale FNE the "embedded FNE" does not have provisioning for talkgroup mutuation (i.e. talkgroup number rewriting, where on System A TG123 routes to System B TG456), all TGs must be one to one across peers. 

The conference bridge FNE is meant as an easier alternative to a full-scale FNE where complex routing or multiple masters are not required.

## Build Notes

- The installation path of "/opt/dvm" is still supported by the CMake Makefile (and will be for the forseeable future); after compiling, in order to install to this path simply use: `make old_install`.
- The installation of the systemd service is also still supported by the CMake Makefile, after using `make old_install`, simply use: `make old_install-service`.

- The old RPi 1, 2 or 3 cross-compile *requires* a downloaded copy of ASIO pointed to with the `-DWITH_ASIO=/path/to/asio`.
- By default when cross-compiling for old RPi 1 using the Debian/Ubuntu OS, the toolchain will attempt to fetch and clone the tools automatically. If you already have a copy of these tools, you can specify the location for them with the `-DWITH_RPI_ARM_TOOLS=/path/to/tools`
- For old RPi 1, 2 or 3 using Debian/Ubuntu OS install the standard ARM embedded toolchain (typically "arm-none-eabi-gcc" and "arm-none-eabi-g++").
  1. Switch to "/opt" and checkout `https://github.com/raspberrypi/tools.git`.
- The old RPi 1, 2 or 3 builds do not support the TUI when cross compiling. If you require the TUI on these platforms, you have to build the dvmhost directly on the target platform vs cross compiling.


There is an automated process to generate a tarball package if required as well, after compiling simply run: `make tarball`. This will generate a tarball package, the tarball package contains the similar pathing that the `make old_install` would generate.

## Security Warnings

It is highly recommended that the REST API interface not be exposed directly to the internet. If such exposure is wanted/needed, it is highly recommended to proxy the dvmhost REST API through a modern web server (like nginx for example) rather then directly exposing dvmhost's REST API port.

## Raspberry Pi Preparation

Some extra notes for those who are using the Raspberry Pi, default Raspbian OS or Debian OS installations. You will not be able to flash or access the STM32 modem unless you do some things beforehand.

1. Disable the Bluetooth services. Bluetooth will share the GPIO serial interface on `/dev/ttyAMA0`. On Rasbian OS or Debian OS, this is done by: `sudo systemctl disable bluetooth` then adding `dtoverlay=disable-bt` to `/boot/config.txt`.
1. The default Rasbian OS and Debian OS will have a getty instance listening on `/dev/ttyAMA0`. This can conflict with the STM32, and is best if disabled. On Rasbian OS or Debian OS, this is done by: `systemctl disable serial-getty@ttyAMA0.service`
1. On Debian Bookworm-based builds of Raspian OS, the getty instance on `/dev/ttyAMA0` gets rebuilt on boot via a systemd generator, even if you've already disabled it.  You'll need to disable this generator with: `sudo systemctl mask serial-getty@ttyAMA0.service`
1. There's a default boot option which is also listening on the GPIO serial interface. This **must be disabled**. Open the `/boot/cmdline.txt` file in your favorite editor (vi or pico) and remove the `console=serial0,115200` part.

The steps above can be done by the following commands:

```shell
sudo systemctl disable bluetooth.service serial-getty@ttyAMA0.service
sudo systemctl mask serial-getty@ttyAMA0.service
grep '^dtoverlay=disable-bt' /boot/config.txt || echo 'dtoverlay=disable-bt' | sudo tee -a /boot/config.txt
sudo sed -i 's/^console=serial0,115200 *//' /boot/cmdline.txt
```

After finishing these steps, reboot.

## License

This project is licensed under the GPLv2 License - see the [LICENSE](LICENSE) file for details. Use of this project is intended, for amateur and/or educational use ONLY. Any other use is at the risk of user and all commercial purposes is strictly discouraged.
