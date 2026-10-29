#pragma once

#include <windows.h>

#include <array>
#include <atomic>
#include <cstdio>

#include "combo_detector.hpp"
#include "logger.hpp"

#ifndef LLKHF_LOWER_IL_INJECTED
#define LLKHF_LOWER_IL_INJECTED 0x00000002
#endif
#ifndef LLMHF_LOWER_IL_INJECTED
#define LLMHF_LOWER_IL_INJECTED 0x00000002
#endif

class InputLock {
public:
    InputLock(HWND notify_window, UINT state_message)
        : notify_window_(notify_window), state_message_(state_message) {
        instance_ = this;
        HINSTANCE self_module = GetModuleHandleW(nullptr);
        keyboard_hook_ = SetWindowsHookExW(WH_KEYBOARD_LL, &InputLock::keyboard_thunk,
                                           self_module, 0);
        mouse_hook_ = SetWindowsHookExW(WH_MOUSE_LL, &InputLock::mouse_thunk,
                                        self_module, 0);
    }

    ~InputLock() {
        if (keyboard_hook_) {
            UnhookWindowsHookEx(keyboard_hook_);
        }
        if (mouse_hook_) {
            UnhookWindowsHookEx(mouse_hook_);
        }
        instance_ = nullptr;
    }

    InputLock(const InputLock&) = delete;
    InputLock& operator=(const InputLock&) = delete;
    InputLock(InputLock&&) = delete;
    InputLock& operator=(InputLock&&) = delete;

    bool hooks_are_installed() const {
        return keyboard_hook_ != nullptr && mouse_hook_ != nullptr;
    }

    void set_logger(Logger* logger) {
        logger_ = logger;
    }

    void set_blocking_enabled(bool enabled) {
        blocking_enabled_ = enabled;
    }

    void set_click_region(const RECT& region) {
        click_region_ = region;
    }

    bool is_locked() const {
        return locked_.load(std::memory_order_relaxed);
    }

    void toggle() {
        set_locked(!locked_.load(std::memory_order_relaxed));
    }

    void set_locked(bool locked) {
        locked_.store(locked, std::memory_order_relaxed);
        PostMessageW(notify_window_, state_message_, static_cast<WPARAM>(locked), 0);
    }

    void release_inputs_held_before_lock() {
        release_held_keyboard_keys();
        release_held_mouse_buttons();
    }

private:
    static constexpr LRESULT kBlockThisEvent = 1;

    enum MouseButtonIndex {
        kMouseLeft = 0,
        kMouseRight,
        kMouseMiddle,
        kMouseExtra1,
        kMouseExtra2,
        kMouseButtonCount
    };

    void release_held_keyboard_keys() {
        for (DWORD virtual_key_code = 0; virtual_key_code < kVirtualKeyCount; ++virtual_key_code) {
            if (!delivered_key_is_down_[virtual_key_code]) {
                continue;
            }
            INPUT synthetic_key_up{};
            synthetic_key_up.type = INPUT_KEYBOARD;
            synthetic_key_up.ki.wVk = static_cast<WORD>(virtual_key_code);
            synthetic_key_up.ki.dwFlags = KEYEVENTF_KEYUP;
            SendInput(1, &synthetic_key_up, sizeof(INPUT));
            delivered_key_is_down_[virtual_key_code] = false;
        }
    }

    void release_held_mouse_buttons() {
        for (std::size_t button = 0; button < kMouseButtonCount; ++button) {
            if (!delivered_mouse_button_is_down_[button]) {
                continue;
            }
            INPUT synthetic_button_up{};
            synthetic_button_up.type = INPUT_MOUSE;
            synthetic_button_up.mi.dwFlags = mouse_up_flag(button);
            if (button == kMouseExtra1) {
                synthetic_button_up.mi.mouseData = static_cast<DWORD>(XBUTTON1);
            } else if (button == kMouseExtra2) {
                synthetic_button_up.mi.mouseData = static_cast<DWORD>(XBUTTON2);
            }
            SendInput(1, &synthetic_button_up, sizeof(INPUT));
            delivered_mouse_button_is_down_[button] = false;
        }
    }

    static DWORD mouse_up_flag(std::size_t button) {
        switch (button) {
            case kMouseLeft:
                return MOUSEEVENTF_LEFTUP;
            case kMouseRight:
                return MOUSEEVENTF_RIGHTUP;
            case kMouseMiddle:
                return MOUSEEVENTF_MIDDLEUP;
            default:
                return MOUSEEVENTF_XUP;
        }
    }

    static std::size_t extra_button_index(DWORD mouse_data) {
        return (HIWORD(mouse_data) == XBUTTON1) ? kMouseExtra1 : kMouseExtra2;
    }

    static LRESULT CALLBACK keyboard_thunk(int code, WPARAM event_kind, LPARAM detail) {
        if (instance_ != nullptr) {
            return instance_->on_keyboard(code, event_kind, detail);
        }
        return CallNextHookEx(nullptr, code, event_kind, detail);
    }

    static LRESULT CALLBACK mouse_thunk(int code, WPARAM event_kind, LPARAM detail) {
        if (instance_ != nullptr) {
            return instance_->on_mouse(code, event_kind, detail);
        }
        return CallNextHookEx(nullptr, code, event_kind, detail);
    }

    LRESULT on_keyboard(int code, WPARAM event_kind, LPARAM detail) {
        if (code != HC_ACTION) {
            return CallNextHookEx(nullptr, code, event_kind, detail);
        }
        const auto* event = reinterpret_cast<const KBDLLHOOKSTRUCT*>(detail);
        const bool is_injected =
            (event->flags & (LLKHF_INJECTED | LLKHF_LOWER_IL_INJECTED)) != 0;
        if (is_injected) {
            return CallNextHookEx(nullptr, code, event_kind, detail);
        }
        const bool is_press = (event_kind == WM_KEYDOWN || event_kind == WM_SYSKEYDOWN);
        const bool is_release = (event_kind == WM_KEYUP || event_kind == WM_SYSKEYUP);
        log_physical_heartbeat();
        if (is_press || is_release) {
            const ComboDetector::Key key = ComboDetector::classify(event->vkCode);
            log_combo_key(key, is_press);
            if (combo_.on_key_event(key, is_press)) {
                log_combo_fired();
                toggle();
                if (blocking_enabled_) {
                    return kBlockThisEvent;
                }
            }
        }
        if (blocking_enabled_ && locked_.load(std::memory_order_relaxed)) {
            return kBlockThisEvent;
        }
        remember_delivered_state(event->vkCode, is_press, is_release);
        return CallNextHookEx(nullptr, code, event_kind, detail);
    }

    static const char* combo_key_name(ComboDetector::Key key) {
        switch (key) {
            case ComboDetector::Key::kControl:
                return "CTRL";
            case ComboDetector::Key::kShift:
                return "SHIFT";
            case ComboDetector::Key::kX:
                return "X";
            default:
                return nullptr;
        }
    }

    void log_physical_heartbeat() {
        if (logger_ == nullptr) {
            return;
        }
        ++physical_event_count_;
        if (physical_event_count_ % kHeartbeatInterval != 0) {
            return;
        }
        char buffer[64];
        std::snprintf(buffer, sizeof(buffer), "ALIVE physical_events=%u", physical_event_count_);
        logger_->write_line(buffer);
    }

    void log_combo_key(ComboDetector::Key key, bool is_press) {
        if (logger_ == nullptr) {
            return;
        }
        const char* name = combo_key_name(key);
        if (name == nullptr) {
            return;
        }
        char buffer[64];
        std::snprintf(buffer, sizeof(buffer), "KEY name=%s down=%d", name, is_press ? 1 : 0);
        logger_->write_line(buffer);
    }

    void log_combo_fired() {
        if (logger_ != nullptr) {
            logger_->write_line("COMBO FIRED");
        }
    }

    void remember_delivered_state(DWORD virtual_key_code, bool is_press, bool is_release) {
        if (virtual_key_code >= kVirtualKeyCount) {
            return;
        }
        if (is_press) {
            delivered_key_is_down_[virtual_key_code] = true;
        } else if (is_release) {
            delivered_key_is_down_[virtual_key_code] = false;
        }
    }

    LRESULT on_mouse(int code, WPARAM event_kind, LPARAM detail) {
        if (code != HC_ACTION) {
            return CallNextHookEx(nullptr, code, event_kind, detail);
        }
        const auto* event = reinterpret_cast<const MSLLHOOKSTRUCT*>(detail);
        const bool is_injected =
            (event->flags & (LLMHF_INJECTED | LLMHF_LOWER_IL_INJECTED)) != 0;
        if (is_injected) {
            return CallNextHookEx(nullptr, code, event_kind, detail);
        }
        if (blocking_enabled_ && locked_.load(std::memory_order_relaxed) &&
            !passes_while_locked(event_kind, event->pt)) {
            return kBlockThisEvent;
        }
        remember_mouse_button_state(event_kind, event->mouseData);
        return CallNextHookEx(nullptr, code, event_kind, detail);
    }

    bool passes_while_locked(WPARAM event_kind, const POINT& position) const {
        if (event_kind == WM_MOUSEMOVE) {
            return true;
        }
        if (event_kind == WM_LBUTTONUP && delivered_mouse_button_is_down_[kMouseLeft]) {
            return true;
        }
        const bool is_left_click = (event_kind == WM_LBUTTONDOWN || event_kind == WM_LBUTTONUP);
        return is_left_click && PtInRect(&click_region_, position) != FALSE;
    }

    void remember_mouse_button_state(WPARAM event_kind, DWORD mouse_data) {
        switch (event_kind) {
            case WM_LBUTTONDOWN:
                delivered_mouse_button_is_down_[kMouseLeft] = true;
                break;
            case WM_LBUTTONUP:
                delivered_mouse_button_is_down_[kMouseLeft] = false;
                break;
            case WM_RBUTTONDOWN:
                delivered_mouse_button_is_down_[kMouseRight] = true;
                break;
            case WM_RBUTTONUP:
                delivered_mouse_button_is_down_[kMouseRight] = false;
                break;
            case WM_MBUTTONDOWN:
                delivered_mouse_button_is_down_[kMouseMiddle] = true;
                break;
            case WM_MBUTTONUP:
                delivered_mouse_button_is_down_[kMouseMiddle] = false;
                break;
            case WM_XBUTTONDOWN:
                delivered_mouse_button_is_down_[extra_button_index(mouse_data)] = true;
                break;
            case WM_XBUTTONUP:
                delivered_mouse_button_is_down_[extra_button_index(mouse_data)] = false;
                break;
            default:
                break;
        }
    }

    static constexpr DWORD kVirtualKeyCount = 256;
    static constexpr unsigned kHeartbeatInterval = 20;
    static InputLock* instance_;

    HWND notify_window_;
    UINT state_message_;
    HHOOK keyboard_hook_ = nullptr;
    HHOOK mouse_hook_ = nullptr;
    std::atomic<bool> locked_{false};
    ComboDetector combo_;
    std::array<bool, kVirtualKeyCount> delivered_key_is_down_{};
    std::array<bool, kMouseButtonCount> delivered_mouse_button_is_down_{};
    Logger* logger_ = nullptr;
    RECT click_region_{};
    bool blocking_enabled_ = true;
    unsigned physical_event_count_ = 0;
};

inline InputLock* InputLock::instance_ = nullptr;
