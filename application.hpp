#pragma once

#include <windows.h>

#include <cstdio>
#include <optional>

#include "camera_capture.hpp"
#include "indicator.hpp"
#include "input_lock.hpp"
#include "logger.hpp"
#include "power_guard.hpp"
#include "tray_icon.hpp"

class Application {
public:
    Application(HINSTANCE instance, bool auto_lock_on_start, bool debug_logging)
        : instance_(instance),
          auto_lock_on_start_(auto_lock_on_start),
          debug_logging_(debug_logging) {}

    ~Application() {
        if (single_instance_mutex_ != nullptr) {
            CloseHandle(single_instance_mutex_);
        }
    }

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    int run() {
        if (!claim_single_instance()) {
            MessageBoxW(nullptr, L"ScreenGuard is already running.", L"ScreenGuard",
                        MB_OK | MB_ICONINFORMATION);
            return 0;
        }
        register_main_window_class();
        create_main_window();
        if (main_window_ == nullptr) {
            return 1;
        }

        power_guard_.emplace();
        indicator_.emplace(instance_, main_window_, kIndicatorClickMessage);
        tray_icon_.emplace(main_window_, kTrayCallbackMessage);
        input_lock_.emplace(main_window_, kStateChangedMessage);

        if (!input_lock_->hooks_are_installed()) {
            MessageBoxW(main_window_,
                        L"Could not install the input hooks.",
                        L"ScreenGuard", MB_OK | MB_ICONERROR);
            return 1;
        }

        input_lock_->set_click_region(indicator_->screen_rect());

        if (debug_logging_) {
            start_debug_logging();
        }

        if (auto_lock_on_start_) {
            SetTimer(main_window_, kAutoLockTimerId, kAutoLockDelayMilliseconds, nullptr);
        }

        return pump_messages();
    }

private:
    static constexpr UINT kTrayCallbackMessage = WM_APP + 1;
    static constexpr UINT kStateChangedMessage = WM_APP + 2;
    static constexpr UINT kIndicatorClickMessage = WM_APP + 3;
    static constexpr UINT kMenuCommandToggle = 1001;
    static constexpr UINT kMenuCommandExit = 1002;
    static constexpr UINT_PTR kAutoLockTimerId = 1;
    static constexpr UINT kAutoLockDelayMilliseconds = 2000;
    static constexpr const wchar_t* kMainClassName = L"ScreenGuardMainWindow";
    static constexpr const wchar_t* kSingleInstanceName = L"Local\\ScreenGuardSingleInstance";

    void start_debug_logging() {
        debug_logger_.emplace();
        if (!debug_logger_->is_open() || !input_lock_.has_value()) {
            return;
        }
        input_lock_->set_logger(&debug_logger_.value());
        input_lock_->set_blocking_enabled(false);
        char buffer[96];
        std::snprintf(buffer, sizeof(buffer), "SESSION START hooks=%d blocking=disabled",
                      input_lock_->hooks_are_installed() ? 1 : 0);
        debug_logger_->write_line(buffer);
    }

    bool claim_single_instance() {
        single_instance_mutex_ = CreateMutexW(nullptr, TRUE, kSingleInstanceName);
        return GetLastError() != ERROR_ALREADY_EXISTS;
    }

    void register_main_window_class() {
        WNDCLASSEXW window_class{};
        window_class.cbSize = sizeof(window_class);
        window_class.lpfnWndProc = &Application::window_proc;
        window_class.hInstance = instance_;
        window_class.lpszClassName = kMainClassName;
        RegisterClassExW(&window_class);
    }

    void create_main_window() {
        main_window_ = CreateWindowExW(WS_EX_TOOLWINDOW, kMainClassName, L"ScreenGuard",
                                       WS_OVERLAPPED, 0, 0, 0, 0, nullptr, nullptr,
                                       instance_, this);
    }

    static LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM w_param,
                                        LPARAM l_param) {
        if (message == WM_NCCREATE) {
            auto* create = reinterpret_cast<CREATESTRUCTW*>(l_param);
            SetWindowLongPtrW(window, GWLP_USERDATA,
                              reinterpret_cast<LONG_PTR>(create->lpCreateParams));
        }
        auto* self =
            reinterpret_cast<Application*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (self != nullptr) {
            return self->handle_message(window, message, w_param, l_param);
        }
        return DefWindowProcW(window, message, w_param, l_param);
    }

    LRESULT handle_message(HWND window, UINT message, WPARAM w_param, LPARAM l_param) {
        switch (message) {
            case kStateChangedMessage:
                apply_state(w_param != 0);
                return 0;
            case kIndicatorClickMessage:
                if (input_lock_.has_value()) {
                    input_lock_->toggle();
                }
                return 0;
            case kTrayCallbackMessage:
                if (LOWORD(l_param) == WM_RBUTTONUP || LOWORD(l_param) == WM_CONTEXTMENU) {
                    show_tray_menu();
                }
                return 0;
            case WM_COMMAND:
                handle_menu_command(LOWORD(w_param));
                return 0;
            case WM_TIMER:
                if (w_param == kAutoLockTimerId) {
                    KillTimer(main_window_, kAutoLockTimerId);
                    if (input_lock_.has_value() && !input_lock_->is_locked()) {
                        input_lock_->set_locked(true);
                    }
                }
                return 0;
            case WM_DESTROY:
                input_lock_.reset();
                tray_icon_.reset();
                indicator_.reset();
                power_guard_.reset();
                PostQuitMessage(0);
                return 0;
            default:
                return DefWindowProcW(window, message, w_param, l_param);
        }
    }

    void handle_menu_command(WORD command_id) {
        if (command_id == kMenuCommandToggle && input_lock_.has_value()) {
            input_lock_->toggle();
        } else if (command_id == kMenuCommandExit) {
            DestroyWindow(main_window_);
        }
    }

    void apply_state(bool locked) {
        if (locked && input_lock_.has_value()) {
            input_lock_->release_inputs_held_before_lock();
        }
        if (tray_icon_.has_value()) {
            tray_icon_->reflect_state(locked);
        }
        if (indicator_.has_value()) {
            if (locked) {
                indicator_->show(true);
            } else {
                indicator_->hide();
            }
            if (input_lock_.has_value()) {
                input_lock_->set_click_region(indicator_->screen_rect());
            }
        }
        if (!debug_logging_) {
            CameraCapture::capture_async(locked ? L"LOCKED" : L"UNLOCKED");
        }
    }

    void show_tray_menu() {
        const bool locked = input_lock_.has_value() && input_lock_->is_locked();
        HMENU menu = CreatePopupMenu();
        AppendMenuW(menu, MF_STRING, kMenuCommandToggle,
                    locked ? L"Unlock  (Ctrl+Shift+X)" : L"Lock  (Ctrl+Shift+X)");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, kMenuCommandExit, L"Exit");
        POINT cursor{};
        GetCursorPos(&cursor);
        SetForegroundWindow(main_window_);
        TrackPopupMenu(menu, TPM_RIGHTBUTTON, cursor.x, cursor.y, 0, main_window_, nullptr);
        PostMessageW(main_window_, WM_NULL, 0, 0);
        DestroyMenu(menu);
    }

    int pump_messages() {
        MSG message{};
        while (GetMessageW(&message, nullptr, 0, 0) > 0) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        return static_cast<int>(message.wParam);
    }

    HINSTANCE instance_;
    bool auto_lock_on_start_ = false;
    bool debug_logging_ = false;
    HWND main_window_ = nullptr;
    HANDLE single_instance_mutex_ = nullptr;
    std::optional<Logger> debug_logger_;
    std::optional<PowerGuard> power_guard_;
    std::optional<Indicator> indicator_;
    std::optional<TrayIcon> tray_icon_;
    std::optional<InputLock> input_lock_;
};
