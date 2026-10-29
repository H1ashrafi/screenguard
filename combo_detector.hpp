#pragma once

#include <cstdint>

class ComboDetector {
public:
    enum class Key { kControl, kShift, kX, kOther };

    static Key classify(std::uint32_t virtual_key_code) {
        switch (virtual_key_code) {
            case kVirtualKeyControl:
            case kVirtualKeyLeftControl:
            case kVirtualKeyRightControl:
                return Key::kControl;
            case kVirtualKeyShift:
            case kVirtualKeyLeftShift:
            case kVirtualKeyRightShift:
                return Key::kShift;
            case kVirtualKeyX:
                return Key::kX;
            default:
                return Key::kOther;
        }
    }

    bool on_key_event(Key key, bool pressed) {
        switch (key) {
            case Key::kControl:
                control_is_down_ = pressed;
                break;
            case Key::kShift:
                shift_is_down_ = pressed;
                break;
            case Key::kX:
                x_is_down_ = pressed;
                break;
            case Key::kOther:
                return false;
        }
        if (!pressed) {
            trigger_is_latched_ = false;
            return false;
        }
        const bool all_three_now_pressed = control_is_down_ && shift_is_down_ && x_is_down_;
        if (all_three_now_pressed && !trigger_is_latched_) {
            trigger_is_latched_ = true;
            return true;
        }
        return false;
    }

    bool all_three_pressed() const {
        return control_is_down_ && shift_is_down_ && x_is_down_;
    }

private:
    static constexpr std::uint32_t kVirtualKeyControl = 0x11;
    static constexpr std::uint32_t kVirtualKeyLeftControl = 0xA2;
    static constexpr std::uint32_t kVirtualKeyRightControl = 0xA3;
    static constexpr std::uint32_t kVirtualKeyShift = 0x10;
    static constexpr std::uint32_t kVirtualKeyLeftShift = 0xA0;
    static constexpr std::uint32_t kVirtualKeyRightShift = 0xA1;
    static constexpr std::uint32_t kVirtualKeyX = 0x58;

    bool control_is_down_ = false;
    bool shift_is_down_ = false;
    bool x_is_down_ = false;
    bool trigger_is_latched_ = false;
};
