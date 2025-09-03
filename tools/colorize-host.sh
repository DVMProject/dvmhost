#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-only
#/**
#* Digital Voice Modem - Host Software
#* GPLv2 Open Source. Use is subject to license terms.
#* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
#*
#*/
#/*
#*   Copyright (C) 2025 by Bryan Biedenkapp N2PLL
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
LOG_COLOR="s#W:#\x1b[1m\x1b[33m&#; s#E:#\x1b[1m\x1b[31m&#; s#M:#\x1b[0m&#; s#I:#\x1b[0m&#; s#D:#\x1b[1m\x1b[34m&\x1b[0m\x1b[1m#;"

DMR_COLOR="s#VOICE#\x1b[36m&#; s#TERMINATOR_WITH_LC#\x1b[0m\x1b[32m&#; s#CSBK#\x1b[0m\x1b[35m&#"
P25_COLOR="s#LDU#\x1b[36m&#; s#TDU#\x1b[0m\x1b[32m&#; s#HDU#\x1b[0m\x1b[32m&#; s#TSDU#\x1b[0m\x1b[35m&#"
NXDN_COLOR="s#VCALL#\x1b[36m&#; s#TX_REL#\x1b[0m\x1b[32m&#"
AFF_COLOR="s#Affiliations#\x1b[1m\x1b[36m&#; s#Affiliation#\x1b[1m\x1b[36m&#;"

RF_HIGHLIGHT="s#RF#\x1b[1m\x1b[35m&\x1b[0m#;"
NET_HIGHLIGHT="s#NET#\x1b[1m\x1b[36m&\x1b[0m#;"

sed "${LOG_COLOR}; ${RF_HIGHLIGHT}; ${NET_HIGHLIGHT}; ${DMR_COLOR}; ${P25_COLOR}; ${NXDN_COLOR}; ${AFF_COLOR}"
