#!/bin/bash
#/**
#* Digital Voice Modem - Host Software
#* GPLv2 Open Source. Use is subject to license terms.
#* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
#*
#* @package DVM / Host Software
#*
#*/
PID=`pgrep dvm-watchdog.sh`
pgrep dvm-watchdog.sh >/dev/null
if [ $? -eq 0 ]; then
    kill -9 $PID
fi

