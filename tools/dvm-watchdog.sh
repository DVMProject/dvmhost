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

if [ -z $1 ]; then exit 1; fi
if [ -z $2 ]; then exit 1; fi

PID_FILE=$1
if [ ! -e $PID_FILE ]; then exit 98; fi
CONFIG=$2
if [ ! -f ${R_PATH}/${CONFIG} ]; then exit 3; fi

PID=`cat $PID_FILE`
while [ true ]; do
    PS_OUT=`ps --no-headers ${PID}`
    if [ ${#PS_OUT} -eq 0 ]; then
        logger "dvmhost PID ${PID} not running -- restarting ${PID_FILE} ${CONFIG}"
        ${R_PATH}/start-dvm.sh ${CONFIG} ${PID_FILE} >/dev/null
        ERRNO=$?
        if [ $ERRNO -ne 0 ]; then
            logger "dvmhost start-dvm.sh script failed to start; errno ${ERRNO}"
        fi
        PID=`cat $PID_FILE`
    fi
    sleep 5s
done

popd >/dev/null
