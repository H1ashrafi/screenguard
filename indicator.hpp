#pragma once

#include <windows.h>

class Indicator {
public:
    Indicator(HINSTANCE instance, HWND notify_window, UINT click_message)
        : instance_(instance), notify_window_(notify_window), click_message_(click_message) {
        register_window_class();
        create_window();
    }

    ~Indicator() {
        if (window_ != nullptr) {
            DestroyWindow(window_);
        }
        UnregisterClassW(kClassName, instance_);
    }

    Indicator(const Indicator&) = delete;
    Indicator& operator=(const Indicator&) = delete;
    Indicator(Indicator&&) = delete;
    Indicator& operator=(Indicator&&) = delete;

    void show(bool locked) {
        locked_ = locked;
        move_to_top_right();
        ShowWindow(window_, SW_SHOWNOACTIVATE);
        InvalidateRect(window_, nullptr, TRUE);
        UpdateWindow(window_);
    }

    void hide() {
        ShowWindow(window_, SW_HIDE);
    }

    RECT screen_rect() const {
        RECT rect{};
        GetWindowRect(window_, &rect);
        return rect;
    }

private:
    static constexpr int kDotSize = 8;
    static constexpr int kTopMargin = 38;
    static constexpr int kRightMargin = 20;
    static constexpr COLORREF kTransparentColor = RGB(0, 0, 0);
    static constexpr COLORREF kLockedColor = RGB(230, 30, 30);
    static constexpr COLORREF kIdleColor = RGB(120, 128, 140);
    static constexpr const wchar_t* kClassName = L"ScreenGuardIndicatorWindow";

    void register_window_class() {
        WNDCLASSEXW window_class{};
        window_class.cbSize = sizeof(window_class);
        window_class.lpfnWndProc = &Indicator::window_proc;
        window_class.hInstance = instance_;
        window_class.hCursor = LoadCursorW(nullptr, IDC_HAND);
        window_class.lpszClassName = kClassName;
        RegisterClassExW(&window_class);
    }

    void create_window() {
        const DWORD extended_style =
            WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE;
        window_ = CreateWindowExW(extended_style, kClassName, L"", WS_POPUP, 0, 0, kDotSize,
                                  kDotSize, nullptr, nullptr, instance_, this);
        if (window_ != nullptr) {
            SetLayeredWindowAttributes(window_, kTransparentColor, 0, LWA_COLORKEY);
        }
    }

    void move_to_top_right() {
        const int screen_width = GetSystemMetrics(SM_CXSCREEN);
        const int x = screen_width - kDotSize - kRightMargin;
        const int y = kTopMargin;
        SetWindowPos(window_, HWND_TOPMOST, x, y, kDotSize, kDotSize,
                     SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }

    void paint(HWND window) const {
        PAINTSTRUCT paint_struct{};
        HDC device_context = BeginPaint(window, &paint_struct);
        RECT client{};
        GetClientRect(window, &client);
        HBRUSH transparent_brush = CreateSolidBrush(kTransparentColor);
        FillRect(device_context, &client, transparent_brush);
        DeleteObject(transparent_brush);
        const COLORREF color = locked_ ? kLockedColor : kIdleColor;
        HBRUSH dot_brush = CreateSolidBrush(color);
        HPEN dot_pen = CreatePen(PS_SOLID, 1, color);
        HGDIOBJ previous_brush = SelectObject(device_context, dot_brush);
        HGDIOBJ previous_pen = SelectObject(device_context, dot_pen);
        Ellipse(device_context, client.left, client.top, client.right, client.bottom);
        SelectObject(device_context, previous_brush);
        SelectObject(device_context, previous_pen);
        DeleteObject(dot_brush);
        DeleteObject(dot_pen);
        EndPaint(window, &paint_struct);
    }

    static LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM w_param,
                                        LPARAM l_param) {
        if (message == WM_NCCREATE) {
            auto* create = reinterpret_cast<CREATESTRUCTW*>(l_param);
            SetWindowLongPtrW(window, GWLP_USERDATA,
                              reinterpret_cast<LONG_PTR>(create->lpCreateParams));
        }
        auto* self = reinterpret_cast<Indicator*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (self != nullptr) {
            if (message == WM_PAINT) {
                self->paint(window);
                return 0;
            }
            if (message == WM_LBUTTONUP) {
                PostMessageW(self->notify_window_, self->click_message_, 0, 0);
                return 0;
            }
            if (message == WM_MOUSEACTIVATE) {
                return MA_NOACTIVATE;
            }
        }
        return DefWindowProcW(window, message, w_param, l_param);
    }

    HINSTANCE instance_;
    HWND notify_window_;
    UINT click_message_;
    HWND window_ = nullptr;
    bool locked_ = false;
};
