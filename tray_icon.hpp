#pragma once

#include <windows.h>
#include <shellapi.h>

#include "icon_factory.hpp"

class TrayIcon {
public:
    TrayIcon(HWND owner_window, UINT callback_message)
        : unlocked_icon_(IconFactory::solid_square(RGB(40, 170, 70))),
          locked_icon_(IconFactory::solid_square(RGB(210, 45, 45))) {
        data_.cbSize = sizeof(data_);
        data_.hWnd = owner_window;
        data_.uID = kTrayIconId;
        data_.uCallbackMessage = callback_message;
        data_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        data_.hIcon = unlocked_icon_;
        set_tooltip(L"ScreenGuard - Unlocked");
        Shell_NotifyIconW(NIM_ADD, &data_);
    }

    ~TrayIcon() {
        Shell_NotifyIconW(NIM_DELETE, &data_);
        if (unlocked_icon_ != nullptr) {
            DestroyIcon(unlocked_icon_);
        }
        if (locked_icon_ != nullptr) {
            DestroyIcon(locked_icon_);
        }
    }

    TrayIcon(const TrayIcon&) = delete;
    TrayIcon& operator=(const TrayIcon&) = delete;
    TrayIcon(TrayIcon&&) = delete;
    TrayIcon& operator=(TrayIcon&&) = delete;

    void reflect_state(bool locked) {
        data_.uFlags = NIF_ICON | NIF_TIP;
        data_.hIcon = locked ? locked_icon_ : unlocked_icon_;
        set_tooltip(locked ? L"ScreenGuard - Locked" : L"ScreenGuard - Unlocked");
        Shell_NotifyIconW(NIM_MODIFY, &data_);
        data_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    }

private:
    static constexpr UINT kTrayIconId = 1;

    void set_tooltip(const wchar_t* text) {
        lstrcpynW(data_.szTip, text, ARRAYSIZE(data_.szTip));
    }

    HICON unlocked_icon_;
    HICON locked_icon_;
    NOTIFYICONDATAW data_{};
};
