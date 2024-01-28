// SPDX-License-Identifier: GPL-2.0-only
/**
* Digital Voice Modem - Common Library
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Common Library
* @license GPLv2 License (https://opensource.org/licenses/GPL-2.0)
*
*   Copyright (C) 2023 Bryan Biedenkapp, N2PLL
*
*/
#if !defined(__THREAD_FUNC_H__)
#define __THREAD_FUNC_H__

#include "common/Thread.h"

#include <cassert>
#include <functional>

// ---------------------------------------------------------------------------
//  Class Declaration
//      Implements a simple function threading mechanism.
// ---------------------------------------------------------------------------

class HOST_SW_API ThreadFunc : public Thread {
public:
    /// <summary>Initializes a new instance of the ThreadFunc class.</summary>
    ThreadFunc(std::function<void()>&& e) : Thread(),
        m_entry(e)
    {
        assert(e != nullptr);
    }

    /// <summary>User-defined function to run for the thread main.</summary>
    void entry() override
    {
        if (m_entry != nullptr)
            m_entry();
    }

private:
    std::function<void()> m_entry;
};

#endif // __THREAD_FUNC_H__
