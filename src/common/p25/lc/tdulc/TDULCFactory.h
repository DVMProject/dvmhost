// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - Common Library
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2023 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file TDULCFactory.h
 * @ingroup p25_lc
 * @file TDULCFactory.cpp
 * @ingroup p25_lc
 */
#if !defined(__P25_LC__TDULC_FACTORY_H__)
#define  __P25_LC__TDULC_FACTORY_H__

#include "common/Defines.h"
#include "common/edac/RS634717.h"

#include "common/p25/lc/TDULC.h"
#include "common/p25/lc/tdulc/LC_ADJ_STS_BCAST.h"
#include "common/p25/lc/tdulc/LC_CALL_TERM.h"
#include "common/p25/lc/tdulc/LC_CONV_FALLBACK.h"
#include "common/p25/lc/tdulc/LC_GROUP_UPDT.h"
#include "common/p25/lc/tdulc/LC_GROUP.h"
#include "common/p25/lc/tdulc/LC_IDEN_UP.h"
#include "common/p25/lc/tdulc/LC_NET_STS_BCAST.h"
#include "common/p25/lc/tdulc/LC_PRIVATE.h"
#include "common/p25/lc/tdulc/LC_RFSS_STS_BCAST.h"
#include "common/p25/lc/tdulc/LC_SYS_SRV_BCAST.h"
#include "common/p25/lc/tdulc/LC_TEL_INT_VCH_USER.h"
#include "common/p25/lc/tdulc/LC_TDULC_RAW.h"

#include "common/p25/lc/tdulc/LC_FAILSOFT.h"

namespace p25
{
    namespace lc
    {
        namespace tdulc
        {
            // ---------------------------------------------------------------------------
            //  Class Declaration
            // ---------------------------------------------------------------------------

            /**
             * @brief Helper class to instantiate an instance of a TDULC.
             * @ingroup p25_lc
             */
            class HOST_SW_API TDULCFactory {
            public:
                /**
                 * @brief Initializes a new instance of the TDULCFactory class.
                 */
                TDULCFactory();
                /**
                 * @brief Finalizes a instance of the TDULCFactory class.
                 */
                ~TDULCFactory();

                /**
                 * @brief Create an instance of a TDULC.
                 * @param[in] data Buffer containing TDULC packet data to decode.
                 * @returns TDULC* Instance of a TDULC representing the decoded data.
                 */
                static std::unique_ptr<TDULC> createTDULC(const uint8_t* data);

            private:
                static edac::RS634717 m_rs;

                /**
                 * @brief Decode a TDULC.
                 * @param tdulc Instance of a TDULC.
                 * @param[in] data Buffer containing TDULC packet data to decode.
                 * @returns TDULC* Instance of a TDULC representing the decoded data.
                 */
                static std::unique_ptr<TDULC> decode(TDULC* tdulc, const uint8_t* data);
            };
        } // namespace tdulc
    } // namespace lc
} // namespace p25

#endif // __P25_LC__TDULC_FACTORY_H__
