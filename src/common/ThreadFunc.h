/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
//
// Based on code from the MMDVMHost project. (https://github.com/g4klx/MMDVMHost)
// Licensed under the GPLv2 License (https://opensource.org/licenses/GPL-2.0)
//
/*
*   Copyright (C) 2023 by Bryan Biedenkapp N2PLL
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, write to the Free Software
*   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/
#if !defined(__THREAD_FUNC_H__)
#define __THREAD_FUNC_H__

#include "common/Defines.h"
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
    virtual void entry()
    {
        if (m_entry != nullptr)
            m_entry();
    }

private:
    std::function<void()> m_entry;
};

#endif // __THREAD_FUNC_H__
