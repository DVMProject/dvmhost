/**
* Digital Voice Modem - Host Software
* GPLv2 Open Source. Use is subject to license terms.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
*
* @package DVM / Host Software
*
*/
//
// Based on code from the finalcut project. (https://github.com/gansm/finalcut)
// Licensed under the LGPLv2 License (https://opensource.org/licenses/LGPL-2.0)
//
/*
*   Copyright (C) 2012-2023 by Markus Gans
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
#if !defined(__BER_DISPLAY_WND_H__)
#define __BER_DISPLAY_WND_H__

#include "host/setup/HostSetup.h"

#include <array>
#include <map>
#include <vector>

#include <final/final.h>
using namespace finalcut;

// ---------------------------------------------------------------------------
//  Class Declaration
//      This class implements the bit error rate display window.
// ---------------------------------------------------------------------------

class HOST_SW_API BERDisplayWnd final : public finalcut::FDialog
{
public:
    /// <summary>
    /// Initializes a new instance of the BERDisplayWnd class.
    /// </summary>
    /// <param name="widget"></param>
    explicit BERDisplayWnd(FWidget* widget = nullptr) : FDialog{widget}
    {
        m_code = {
            /*
            ** Segments are drawn as follows:
            **
            **  H A I
            **  F G B
            **  E D C
            */
            //             h  v  v  h  v  v  h  h  v
            //             a  b  c  d  e  f  g  h  i
            { '0', Segment{1, 1, 1, 1, 1, 1, 0, 1, 2} },
            { '1', Segment{0, 1, 1, 0, 0, 0, 0, 0, 2} },
            { '2', Segment{1, 1, 2, 1, 1, 2, 1, 1, 2} },
            { '3', Segment{1, 1, 1, 1, 2, 0, 1, 1, 2} },
            { '4', Segment{0, 1, 1, 0, 0, 1, 1, 1, 2} },
            { '5', Segment{1, 2, 1, 1, 2, 1, 1, 1, 2} },
            { '6', Segment{1, 2, 1, 1, 1, 1, 1, 1, 2} },
            { '7', Segment{1, 1, 1, 0, 0, 0, 0, 1, 2} },
            { '8', Segment{1, 1, 1, 1, 1, 1, 1, 1, 2} },
            { '9', Segment{1, 1, 1, 1, 2, 1, 1, 1, 2} },
            { 'A', Segment{1, 1, 1, 0, 1, 1, 1, 1, 2} },
            { 'B', Segment{0, 2, 1, 1, 1, 1, 1, 1, 0} },
            { 'C', Segment{1, 0, 2, 1, 1, 1, 0, 1, 2} },
            { 'D', Segment{0, 1, 1, 1, 1, 2, 1, 0, 2} },
            { 'E', Segment{1, 0, 2, 1, 1, 1, 1, 1, 2} },
            { 'F', Segment{1, 0, 0, 0, 1, 1, 1, 1, 2} }
        };
    }
    /// <summary>Copy constructor.</summary>
    BERDisplayWnd(const BERDisplayWnd&) = delete;
    /// <summary>Move constructor.</summary>
    BERDisplayWnd(BERDisplayWnd&&) noexcept = delete;
    /// <summary>Finalizes an instance of the ModemStatusWnd class.</summary>
    ~BERDisplayWnd() noexcept override = default;

    /// <summary>Disable copy assignment operator (=).</summary>
    auto operator= (const BERDisplayWnd&) -> BERDisplayWnd& = delete;
    /// <summary>Disable move assignment operator (=).</summary>
    auto operator= (BERDisplayWnd&&) noexcept -> BERDisplayWnd& = delete;

    /// <summary>Disable set X coordinate.</summary>
    void setX(int, bool = true) override { }
    /// <summary>Disable set Y coordinate.</summary>
    void setY(int, bool = true) override { }
    /// <summary>Disable set position.</summary>
    void setPos(const FPoint&, bool = true) override { }

    /// <summary>
    /// Helper to set the BER text.
    /// </summary>
    void ber(std::string str) 
    {
        if (str.empty()) {
            return;
        }

        m_ber = str;
        std::transform(m_ber.begin(), m_ber.end(), m_ber.begin(), ::toupper);
        redraw();
    }

    /// <summary>Helper to set the segment color.</summary>
    void segmentColor(FColor color) { m_segmentColor = color; }

private:
    std::string m_ber;

    struct Segment
    {
        unsigned char a : 2;
        unsigned char b : 2;
        unsigned char c : 2;
        unsigned char d : 2;
        unsigned char e : 2;
        unsigned char f : 2;
        unsigned char g : 2;
        unsigned char h : 2;
        unsigned char i : 2;
        unsigned char   : 2;  // padding bit
    };
    std::map<wchar_t, Segment> m_code{};
    std::array<FString, 3> m_line{};

    FColor m_segmentColor{FColor::LightRed};

    /// <summary>
    ///
    /// </summary>
    void initLayout() override
    {
        FDialog::setText("Receive BER");

        const auto& rootWidget = getRootWidget();

        FDialog::setGeometry(FPoint{(int)rootWidget->getClientWidth() - 26, 2}, FSize{25, 7});
        FDialog::setMinimumSize(FSize{25, 7});
        FDialog::setResizeable(false);
        FDialog::setMinimizable(false);
        FDialog::setTitlebarButtonVisibility(false);
        FDialog::setShadow(false);
        FDialog::setAlwaysOnTop(true);

        FDialog::initLayout();
    }

    /// <summary>
    ///
    /// </summary>
    void draw() override
    {
        std::vector<FVTermBuffer> vtbuffer(3);
        FDialog::draw();

        setColor(FColor::LightGray, FColor::Black);
        finalcut::drawBorder(this, FRect(FPoint{1, 2}, FPoint{25, 7}));

        for (const auto& ch : m_ber) {
            const FColorPair color{m_segmentColor, FColor::Black};
            get7Segment(ch);

            for (std::size_t i = 0; i < 3; i++)
                vtbuffer[i] << color << m_line[i] << " ";
        }

        const std::size_t length = vtbuffer[0].getLength();
        const FVTermBuffer leftSpace = length < 23 ? FVTermBuffer() << FString(23 - length, ' ') : FVTermBuffer();
        print() << FPoint{2, 3} << leftSpace << vtbuffer[0]
                << FPoint{2, 4} << leftSpace << vtbuffer[1]
                << FPoint{2, 5} << leftSpace << vtbuffer[2]
                << FPoint{2, 6} << FString{23, ' '};
    }

    /// <summary>
    ///
    /// </summary>
    /// <param name="c"></param>
    void get7Segment(const wchar_t c)
    {
        for (std::size_t i = 0; i < 3; i++)
            m_line[i].clear();

        switch (c) {
        case ':':
            m_line[0] = ' ';
            m_line[1] = '.';
            m_line[2] = '.';
            break;

        case '.':
            m_line[0] = ' ';
            m_line[1] = ' ';
            m_line[2] = (wchar_t)(0x2584);
            break;

        case '-':
            m_line[0] << ' ' << ' ' << ' ';
            m_line[1] << (wchar_t)(0x2584) << (wchar_t)(0x2584) << (wchar_t)(0x2584);
            m_line[2] << ' ' << ' ' << ' ';
            break;

        default:
            // hexadecimal digit from 0 up to F
            if (m_code.find(c) != m_code.end()) {
                const Segment& s = m_code[c];
                constexpr std::array<wchar_t, 3> h{{0x20, 0x2584, 0x2588}};
                constexpr std::array<wchar_t, 3> v{{0x20, 0x2588, 0x2584}};

                m_line[0] << h[s.h] << h[s.a] << v[s.i];
                m_line[1] << v[s.f] << h[s.g] << v[s.b];
                m_line[2] << v[s.e] << h[s.d] << v[s.c];
            }
        }
    }
};

#endif // __BER_DISPLAY_WND_H__