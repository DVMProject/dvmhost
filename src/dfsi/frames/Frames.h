// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - DFSI V.24/UDP Software
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024 Patrick McDonnell, W3AXL
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
 *
 */
#if !defined(__DFSI_FRAMES_H__)
#define __DFSI_FRAMES_H__

#include "Defines.h"

// TIA
#include "frames/StartOfStream.h"
#include "frames/ControlOctet.h"
#include "frames/BlockHeader.h"
#include "frames/FullRateVoice.h"

// "The" Manufacturer
#include "frames/MotFullRateVoice.h"
#include "frames/MotStartOfStream.h"
#include "frames/MotStartVoiceFrame.h"
#include "frames/MotVoiceHeader1.h"
#include "frames/MotVoiceHeader2.h"

// FSC
#include "frames/fsc/FSCMessage.h"
#include "frames/fsc/FSCResponse.h"
#include "frames/fsc/FSCACK.h"
#include "frames/fsc/FSCConnect.h"
#include "frames/fsc/FSCConnectResponse.h"
#include "frames/fsc/FSCDisconnect.h"
#include "frames/fsc/FSCHeartbeat.h"

#endif // __DFSI_FRAMES_H__