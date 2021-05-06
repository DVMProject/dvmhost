#!/usr/bin/env python
#
# Digital Voice Modem - Fixed Network Equipment
# GPLv2 Open Source. Use is subject to license terms.
# DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
#
# @package DVM / dvmhost
#
###############################################################################
#   Copyright (C) 2021 Bryan Biedenkapp <gatekeep@gmail.com>
#
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 3 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program; if not, write to the Free Software Foundation,
#   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
###############################################################################
from __future__ import print_function

from pprint import pprint

# ---------------------------------------------------------------------------
#   Program Entry Point
# ---------------------------------------------------------------------------

if __name__ == '__main__':
    import argparse
    import os

    MAX_FREQ_GAP = 25500000
    HZ_MHZ = float(1000000)

    print('Digital Voice Modem -> Identity Table Calculator D01.00')

    # CLI argument parser
    parser = argparse.ArgumentParser()
    parser.add_argument('--base', action = 'store', dest = 'BaseFrequency', type = int, help = 'Base Frequency (in Hz)', required = True)
    parser.add_argument('--spacing', action = 'store', dest = 'Spacing', type = float, help = 'Channel Spacing (in KHz)', required = True)
    parser.add_argument('--offset', action = 'store', dest = 'InputOffset', type = float, help = 'Input Offset (in MHz)', required = True)
    parser.add_argument('--bandwidth', action = 'store', dest = 'Bandwidth', type = float, help = 'Bandwidth (in KHz)', required = True)
    parser.add_argument('--tx', action = 'store', dest = 'TxFrequency', type = int, help = 'Transmit Frequency (in Hz, this should be within the band of the Base Frequency)', required = False)
    parser.add_argument('--ch-no', action = 'store', dest = 'ChannelNo', type = str, help = 'Logical Channel Number', required = False)
    cli_args = parser.parse_args()

    if (not cli_args.TxFrequency) and (not cli_args.ChannelNo):
        print ('ERROR: Tx Frequency or Channel Number must be spcified!')
        quit()
    if (cli_args.TxFrequency) and (cli_args.ChannelNo):
        print ('ERROR: Tx Frequency or Channel Number must be spcified!')
        quit()
    if (cli_args.TxFrequency):
        if (cli_args.TxFrequency < cli_args.BaseFrequency):
            print ('ERROR: Tx Frequency (' + '%.5f' % float(cli_args.TxFrequency / HZ_MHZ) + ') is out of band range for base frequency (' + '%.5f' % float(cli_args.BaseFrequency / HZ_MHZ) + '). ' + \
                'Tx Frequency must be greater then the base frequency!')
            quit()
        if (cli_args.TxFrequency > (cli_args.BaseFrequency + MAX_FREQ_GAP)):
            print ('ERROR: Tx Frequency (' + '%.5f' % float(cli_args.TxFrequency / HZ_MHZ) + ') is out of band range for base frequency (' + '%.5f' % float(cli_args.BaseFrequency / HZ_MHZ) + '). ' + \
                'Tx Frequency must be no more then 25.5 Mhz higher then the base frequency!')
            quit()

    print ('\r\nIdentity Data:')

    print ('Base Frequency: ' + '%.5f' % float(cli_args.BaseFrequency / HZ_MHZ) + ' MHz' + 
           '\r\nChannel Spacing: ' + '%.2f' % cli_args.Spacing + ' KHz' +
           '\r\nReceive Offset: ' + '%.3f' % cli_args.InputOffset + ' MHz' +
           '\r\nChannel Bandwidth: ' + '%.2f' % cli_args.Bandwidth + ' KHz')

    print ('\r\nIdentity Table Line: "' + 'xx,' + str(cli_args.BaseFrequency) + ',' + str(cli_args.Spacing) + ',' +
           '%.3f' % cli_args.InputOffset + ',' + str(cli_args.Bandwidth) + ',"')

    print ('\r\nChannel Data:')
    offsetHz = cli_args.InputOffset * HZ_MHZ

    if cli_args.ChannelNo:
        space = cli_args.Spacing / 0.125
        chNo = int(cli_args.ChannelNo, 16)

        rxFrequency = (cli_args.BaseFrequency + ((space * 125) * chNo)) + offsetHz
        txFrequency = (cli_args.BaseFrequency + ((space * 125) * chNo))
        
        print ('\r\nChannel Number: ' + '%x' % chNo)

        print ('\r\nTx Frequency: ' + '%.5f' % (float(txFrequency / HZ_MHZ)) + ' MHz' +
               '\r\nRx Frequency: ' + '%.5f' % (float(rxFrequency / HZ_MHZ)) + ' MHz')
    else:
        rxFrequency = int(cli_args.TxFrequency + offsetHz)

        print ('Tx Frequency: ' + '%.5f' % (float(cli_args.TxFrequency / HZ_MHZ)) + ' MHz' +
               '\r\nRx Frequency: ' + '%.5f' % (float(rxFrequency / HZ_MHZ)) + ' MHz')

        spaceHz = int(cli_args.Spacing * 1000)
        offsetHz = int(cli_args.InputOffset * HZ_MHZ)

        rootFreq = cli_args.TxFrequency - cli_args.BaseFrequency
        chNo = rootFreq / spaceHz

        print ('\r\nChannel Number: ' + '%x' % chNo)
