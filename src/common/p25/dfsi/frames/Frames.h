// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024 Patrick McDonnell, W3AXL
 *  Copyright (C) 2024-2025 Bryan Biedenkapp, N2PLL
 *
 */
#if !defined(__DFSI_FRAMES_H__)
#define __DFSI_FRAMES_H__

#include "Defines.h"

// TIA
#include "common/p25/dfsi/frames/StartOfStream.h"
#include "common/p25/dfsi/frames/ControlOctet.h"
#include "common/p25/dfsi/frames/BlockHeader.h"
#include "common/p25/dfsi/frames/FullRateVoice.h"

// "The" Manufacturer
#include "common/p25/dfsi/frames/MotStartOfStream.h"
#include "common/p25/dfsi/frames/MotStartVoiceFrame.h"
#include "common/p25/dfsi/frames/MotFullRateVoice.h"
#include "common/p25/dfsi/frames/MotTDULCFrame.h"
#include "common/p25/dfsi/frames/MotTSBKFrame.h"

// FSC
#include "common/p25/dfsi/frames/fsc/FSCMessage.h"
#include "common/p25/dfsi/frames/fsc/FSCACK.h"
#include "common/p25/dfsi/frames/fsc/FSCConnect.h"
#include "common/p25/dfsi/frames/fsc/FSCReportSelModes.h"
#include "common/p25/dfsi/frames/fsc/FSCDisconnect.h"
#include "common/p25/dfsi/frames/fsc/FSCHeartbeat.h"
#include "common/p25/dfsi/frames/fsc/FSCSelChannel.h"

#endif // __DFSI_FRAMES_H__