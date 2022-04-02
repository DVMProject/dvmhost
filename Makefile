CC          = gcc
CXX         = g++
STRIP       = strip
armCC       = arm-linux-gnueabihf-gcc
armCXX      = arm-linux-gnueabihf-g++
armSTRIP    = arm-linux-gnueabihf-strip
rpi-armCC   = /opt/tools/arm-bcm2708/arm-linux-gnueabihf/bin/arm-linux-gnueabihf-gcc
rpi-armCXX  = /opt/tools/arm-bcm2708/arm-linux-gnueabihf/bin/arm-linux-gnueabihf-g++ 
rpi-armSTRIP= /opt/tools/arm-bcm2708/arm-linux-gnueabihf/bin/arm-linux-gnueabihf-strip

CFLAGS  = -g -O3 -Wall -std=c++0x -pthread -I.
CCFLAGS = -I..
EXTFLAGS=
LIBS    = -lpthread -lutil
LDFLAGS = -g

HOST_BIN = dvmhost
HOST_OBJECTS = \
		edac/AMBEFEC.o \
		edac/BCH.o \
		edac/BPTC19696.o \
		edac/CRC.o \
		edac/Golay2087.o \
		edac/Golay24128.o \
		edac/Hamming.o \
		edac/QR1676.o \
		edac/RS129.o \
		edac/RS634717.o \
		edac/SHA256.o \
		dmr/acl/AccessControl.o \
		dmr/data/Data.o \
		dmr/data/DataHeader.o \
		dmr/data/EMB.o \
		dmr/data/EmbeddedData.o \
		dmr/edac/Trellis.o \
		dmr/lc/CSBK.o \
		dmr/lc/FullLC.o \
		dmr/lc/LC.o \
		dmr/lc/PrivacyLC.o \
		dmr/lc/ShortLC.o \
		dmr/Control.o \
		dmr/ControlPacket.o \
		dmr/DataPacket.o \
		dmr/Slot.o \
		dmr/SlotType.o \
		dmr/Sync.o \
		dmr/VoicePacket.o \
		lookups/IdenTableLookup.o \
		lookups/RadioIdLookup.o \
		lookups/RSSIInterpolator.o \
		lookups/TalkgroupIdLookup.o \
		p25/acl/AccessControl.o \
		p25/data/DataBlock.o \
		p25/data/DataHeader.o \
		p25/data/LowSpeedData.o \
		p25/dfsi/LC.o \
		p25/dfsi/DFSITrunkPacket.o \
		p25/dfsi/DFSIVoicePacket.o \
		p25/edac/Trellis.o \
		p25/lc/LC.o \
		p25/lc/TDULC.o \
		p25/lc/TSBK.o \
		p25/Audio.o \
		p25/Control.o \
		p25/DataPacket.o \
		p25/NID.o \
		p25/Sync.o \
		p25/TrunkPacket.o \
		p25/P25Utils.o \
		p25/VoicePacket.o \
		modem/port/IModemPort.o \
		modem/port/ISerialPort.o \
		modem/port/ModemNullPort.o \
		modem/port/UARTPort.o \
		modem/port/PseudoPTYPort.o \
		modem/port/UDPPort.o \
		modem/Modem.o \
		network/UDPSocket.o \
		network/RemoteControl.o \
		network/BaseNetwork.o \
		network/Network.o \
		yaml/Yaml.o \
		host/calibrate/Console.o \
		host/calibrate/HostCal.o \
		host/setup/HostSetup.o \
		host/Host.o \
		Log.o \
		Mutex.o \
		Thread.o \
		Timer.o \
		StopWatch.o \
		Utils.o \
		HostMain.o
CMD_BIN = dvmcmd
CMD_OBJECTS = \
        remote/RemoteCommand.cmd.o \
        edac/SHA256.cmd.o \
        network/UDPSocket.cmd.o \
		Log.cmd.o

all: dvmhost dvmcmd
dvmhost: $(HOST_OBJECTS) 
		$($(ARCH)CXX) $(HOST_OBJECTS) $(CFLAGS) $(EXTFLAGS) $(LIBS) -o $(HOST_BIN)
dvmcmd: $(CMD_OBJECTS)
		$($(ARCH)CXX) $(CMD_OBJECTS) $(CFLAGS) $(CCFLAGS) $(EXTFLAGS) $(LIBS) -o $(CMD_BIN)
%.o: %.cpp
		$($(ARCH)CXX) $(CFLAGS) $(EXTFLAGS) -c -o $@ $<
%.cmd.o: %.cpp
		$($(ARCH)CXX) $(CFLAGS) $(CCFLAGS) $(EXTFLAGS) -c -o $@ $<
strip:
		-$($(ARCH)STRIP) $(HOST_BIN)
		-$($(ARCH)STRIP) $(CMD_BIN)
clean:
		$(RM) $(HOST_BIN) $(HOST_OBJECTS) $(CMD_BIN) $(CMD_OBJECTS) *.o *.d *.bak *~
		$(RM) -r dpkg_build
		$(RM) dvmhost_1.0.0* dvmhost-dbgsym*.deb

install: all
		mkdir -p /opt/dvm/bin || true
		install -m 755 $(BIN) /opt/dvm/bin/
		mkdir -p /opt/dvm || true
		install -m 644 config.example.yml /opt/dvm/config.yml
		install -m 644 iden_table.dat /opt/dvm/iden_table.dat
		install -m 644 RSSI.dat /opt/dvm/RSSI.dat
		install -m 644 rid_acl.example.dat /opt/dvm/rid_acl.dat
		install -m 644 tg_acl.example.dat /opt/dvm/tg_acl.dat
		install -m 755 start-dvm.sh /opt/dvm
		install -m 755 stop-dvm.sh /opt/dvm
		install -m 755 dvm-watchdog.sh /opt/dvm
		install -m 755 stop-watchdog.sh /opt/dvm
		sed -i 's/filePath: ./filePath: \/opt\/dvm\/log\//' /opt/dvm/config.yml
		sed -i 's/activityFilePath: ./activityFilePath: \/opt\/dvm\/log\//' /opt/dvm/config.yml
		sed -i 's/file: iden_table.dat/file: \/opt\/dvm\/iden_table.dat/' /opt/dvm/config.yml
		sed -i 's/file: rid_acl.dat/file: \/opt\/dvm\/rid_acl.dat/' /opt/dvm/config.yml
		sed -i 's/file: tg_acl.dat/file: \/opt\/dvm\/tg_acl.dat/' /opt/dvm/config.yml
		mkdir -p /opt/dvm/log || true

dpkg: clean
		which debuild || (echo "debuild is missing? Is 'devscripts' package installed?"; exit 1)
		tar -caf ../dvmhost_1.0.0.orig.tar.gz --exclude=.git --exclude=.gitattributes --exclude=.gitignore --exclude=.vscode -v .
		mv ../dvmhost_1.0.0.orig.tar.gz .
		mkdir -p dpkg_build
		cd dpkg_build; tar xvf ../dvmhost_1.0.0.orig.tar.gz .; debuild -us -uc

install-service: install
		@useradd --user-group -M --system dvmhost --shell /bin/false || true
		@usermod --groups dialout --append dvmhost || true
		@chown dvmhost:dvmhost /opt/dvm/config.yml
		@chown dvmhost:dvmhost /opt/dvm/iden_table.dat
		@chown dvmhost:dvmhost /opt/dvm/RSSI.dat
		@chown dvmhost:dvmhost /opt/dvm/rid_acl.dat
		@chown dvmhost:dvmhost /opt/dvm/tg_acl.dat
		@chown dvmhost:dvmhost /opt/dvm/log
		@cp ./linux/dvmhost.service /lib/systemd/system/

uninstall-service:
		@rm -f /lib/systemd/system/dvmhost.service || true
