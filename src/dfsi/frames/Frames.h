// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - DFSI Peer Application
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / DFSI Peer Application
* @derivedfrom MMDVMHost (https://github.com/g4klx/MMDVMHost)
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2024 Patrick McDonnell, W3AXL
*   Copyright (C) 2024 Bryan Biedenkapp, N2PLL
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

#endif // __DFSI_FRAMES_H__