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
EXTFLAGS=
LIBS    = -lpthread -lutil
LDFLAGS = -g

BIN = dvmhost
OBJECTS = \
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

all: dvmhost
dvmhost: $(OBJECTS) 
		$($(ARCH)CXX) $(OBJECTS) $(CFLAGS) $(EXTFLAGS) $(LIBS) -o $(BIN)
%.o: %.cpp
		$($(ARCH)CXX) $(CFLAGS) $(EXTFLAGS) -c -o $@ $<
strip:
		$($(ARCH)STRIP) $(BIN)
clean:
		$(RM) $(BIN) $(OBJECTS) *.o *.d *.bak *~

install: all
		@mkdir -p /opt/dvm
		install -m 755 $(BIN) /opt/dvm/bin/

install-config-files:
		@mkdir -p /opt/dvm
		@cp -n config.example.yml /opt/dvm/config.yml
		@cp -n iden_table.example.dat /opt/dvm/iden_table.dat
		@cp -n rid_acl.example.dat /opt/dvm/rid_acl.dat
		@cp -n tg_acl.example.dat /opt/dvm/tg_acl.dat
		@sed -i 's/filePath: ./filePath: \/opt\/dvm\/log\//' /opt/dvm/config.yml
		@sed -i 's/activityFilePath: ./activityFilePath: \/opt\/dvm\/log\//' /opt/dvm/config.yml
		@sed -i 's/file: iden_table.dat/file: \/opt\/dvm\/iden_table.dat/' /opt/dvm/config.yml
		@sed -i 's/file: rid_acl.dat/file: \/opt\/dvm\/rid_acl.dat/' /opt/dvm/config.yml
		@sed -i 's/file: tg_acl.dat/file: \/opt\/dvm\/tg_acl.dat/' /opt/dvm/config.yml

install-service: install install-config-files
		@useradd --user-group -M --system dvmhost --shell /bin/false || true
		@usermod --groups dialout --append dvmhost || true
		@mkdir /opt/dvm/log || true
		@chown dvmhost:dvmhost /opt/dvm/log
		@cp ./linux/dvmhost.service /lib/systemd/system/
		@systemctl enable dvmhost.service

uninstall-service:
		@systemctl stop dvmhost.service || true
		@systemctl disable dvmhost.service || true
		@rm -f /lib/systemd/system/dvmhost.service || true
