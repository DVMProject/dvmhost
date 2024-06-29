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
 * @file ThreadFunc.h
 * @ingroup threading
 */
#if !defined(__THREAD_FUNC_H__)
#define __THREAD_FUNC_H__

#include "common/Thread.h"

#include <cassert>
#include <functional>

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief Creates and controls a thread based around an anonymous lambda function.
 * @ingroup threading
 */
class HOST_SW_API ThreadFunc : public Thread {
public:
    /**
     * @brief Initializes a new instance of the ThreadFunc class.
     * @param e Anonymous function to use as the thread main.
     */
    ThreadFunc(std::function<void()>&& e) : Thread(),
        m_entry(e)
    {
        assert(e != nullptr);
    }

    /**
     * @brief User-defined function to run for the thread main.
     */
    void entry() override
    {
        if (m_entry != nullptr)
            m_entry();
    }

private:
    std::function<void()> m_entry;
};

#endif // __THREAD_FUNC_H__
