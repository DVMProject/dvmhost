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
R_PATH=/opt/dvm
pushd ${R_PATH} >/dev/null

if [ "`pwd`" != "${R_PATH}" ]; then exit 99; fi
if [ -z $1 ]; then exit 2; fi

CONFIG=$1
if [ ! -f ${R_PATH}/${CONFIG} ]; then exit 3; fi
PID_FILE=$2
if [ -z $2 ]; then
    STRIPPED_CONFIG=`basename ${CONFIG}`
    STRIPPED_CONFIG=${STRIPPED_CONFIG%.yml}
    PID_FILE=/tmp/dvmhost.${STRIPPED_CONFIG}.pid
fi

COMMAND="${R_PATH}/bin/dvmhost -c ${R_PATH}/${CONFIG}"
nice -n -20 ${COMMAND}

if [ -e /tmp/${CONFIG}.pid ]; then rm -f /tmp/${CONFIG}.pid; fi

PID=`pgrep -f "${R_PATH}/${CONFIG}"`

echo "${PID}" > $PID_FILE
pgrep -f "[d]vm-watchdog.*${PID_FILE}" >/dev/null
if [ $? -ne 0 ]; then
    ${R_PATH}/dvm-watchdog.sh $PID_FILE $CONFIG &
fi

popd >/dev/null
