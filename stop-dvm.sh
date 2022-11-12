#!/bin/bash
#/**
#* Digital Voice Modem - Host Software
#* GPLv2 Open Source. Use is subject to license terms.
#* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
#*
#* @package DVM / Host Software
#*
#*/
#/*
#*   Copyright (C) 2022 by Bryan Biedenkapp N2PLL
#*
#*   This program is free software; you can redistribute it and/or modify
#*   it under the terms of the GNU General Public License as published by
#*   the Free Software Foundation; either version 2 of the License, or
#*   (at your option) any later version.
#*
#*   This program is distributed in the hope that it will be useful,
#*   but WITHOUT ANY WARRANTY; without even the implied warranty of
#*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#*   GNU General Public License for more details.
#*
#*   You should have received a copy of the GNU General Public License
#*   along with this program; if not, write to the Free Software
#*   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#*/
PID=`pgrep dvmhost`
pgrep dvmhost >/dev/null
if [ $? -eq 0 ]; then
    kill -15 $PID
    sleep 2s
    pgrep dvmhost >/dev/null
    if [ $? -eq 0 ]; then
        kill -9 $PID
    fi
fi

PID=`pgrep dvm-watchdog.sh`
pgrep dvm-watchdog.sh >/dev/null
if [ $? -eq 0 ]; then
    kill -9 $PID
fi

