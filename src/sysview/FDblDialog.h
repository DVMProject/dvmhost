// SPDX-License-Identifier: GPL-2.0-only
/*
 * Digital Voice Modem - FNE System View
 * GPLv2 Open Source. Use is subject to license terms.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 *  Copyright (C) 2024 Bryan Biedenkapp, N2PLL
 *
 */
/**
 * @file FDblDialog.h
 * @ingroup fneSysView
 */
#if !defined(__F_DBL_DIALOG_H__)
#define __F_DBL_DIALOG_H__

#include "common/Defines.h"

#include <final/final.h>
using namespace finalcut;

// ---------------------------------------------------------------------------
//  Class Declaration
// ---------------------------------------------------------------------------

/**
 * @brief This class implements the double-border dialog.
 * @ingroup fneSysView
 */
class HOST_SW_API FDblDialog : public finalcut::FDialog {
public:
    /**
     * @brief Initializes a new instance of the FDblDialog class.
     * @param widget 
     */
    explicit FDblDialog(FWidget* widget = nullptr) : finalcut::FDialog{widget}
    {
        /* stub */
    }

protected:
    /**
     * @brief 
     */
    void drawBorder() override
    {
        if (!hasBorder())
            return;        

        setColor();

        FRect box{{1, 2}, getSize()};
        box.scaleBy(0, -1);

        FRect rect = box;
        if (rect.x1_ref() > rect.x2_ref())
            std::swap(rect.x1_ref(), rect.x2_ref());

        if (rect.y1_ref() > rect.y2_ref())
            std::swap(rect.y1_ref(), rect.y2_ref());

        rect.x1_ref() = std::max(rect.x1_ref(), 1);
        rect.y1_ref() = std::max(rect.y1_ref(), 1);
        rect.x2_ref() = std::min(rect.x2_ref(), rect.x1_ref() + int(getWidth()) - 1);
        rect.y2_ref() = std::min(rect.y2_ref(), rect.y1_ref() + int(getHeight()) - 1);

        if (box.getWidth() < 3)
            return;

        // Use box-drawing characters to draw a border
        constexpr std::array<wchar_t, 8> box_char
        {{
            static_cast<wchar_t>(0x2554),   // ╔
            static_cast<wchar_t>(0x2550),   // ═
            static_cast<wchar_t>(0x2557),   // ╗
            static_cast<wchar_t>(0x2551),   // ║
            static_cast<wchar_t>(0x2551),   // ║
            static_cast<wchar_t>(0x255A),   // ╚
            static_cast<wchar_t>(0x2550),   // ═
            static_cast<wchar_t>(0x255D)    // ╝
        }};

        drawGenericBox(this, box, box_char);
    }
};

#endif // __F_DBL_DIALOG_H__
