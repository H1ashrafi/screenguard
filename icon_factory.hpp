#pragma once

#include <windows.h>

class IconFactory {
public:
    static HICON solid_square(COLORREF fill_color) {
        constexpr int kSide = 32;
        const RECT area{0, 0, kSide, kSide};

        HDC screen_dc = GetDC(nullptr);
        HDC color_dc = CreateCompatibleDC(screen_dc);
        HBITMAP color_bitmap = CreateCompatibleBitmap(screen_dc, kSide, kSide);
        HGDIOBJ previous_color = SelectObject(color_dc, color_bitmap);

        HBRUSH fill_brush = CreateSolidBrush(fill_color);
        FillRect(color_dc, &area, fill_brush);
        HBRUSH border_brush = CreateSolidBrush(RGB(24, 24, 24));
        FrameRect(color_dc, &area, border_brush);
        DeleteObject(fill_brush);
        DeleteObject(border_brush);
        SelectObject(color_dc, previous_color);

        HBITMAP mask_bitmap = CreateBitmap(kSide, kSide, 1, 1, nullptr);
        HDC mask_dc = CreateCompatibleDC(screen_dc);
        HGDIOBJ previous_mask = SelectObject(mask_dc, mask_bitmap);
        FillRect(mask_dc, &area, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
        SelectObject(mask_dc, previous_mask);
        DeleteDC(mask_dc);

        ICONINFO icon_info{};
        icon_info.fIcon = TRUE;
        icon_info.hbmMask = mask_bitmap;
        icon_info.hbmColor = color_bitmap;
        HICON icon = CreateIconIndirect(&icon_info);

        DeleteObject(color_bitmap);
        DeleteObject(mask_bitmap);
        DeleteDC(color_dc);
        ReleaseDC(nullptr, screen_dc);
        return icon;
    }
};
