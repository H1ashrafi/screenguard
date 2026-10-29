#include "combo_detector.hpp"

#include <cstdio>

namespace {

int g_failure_count = 0;

void check(bool condition, const char* label) {
    if (condition) {
        std::printf("ok:   %s\n", label);
    } else {
        std::printf("FAIL: %s\n", label);
        ++g_failure_count;
    }
}

using Key = ComboDetector::Key;

void fires_once_when_all_three_pressed_in_order() {
    ComboDetector detector;
    check(!detector.on_key_event(Key::kControl, true), "control alone does not fire");
    check(!detector.on_key_event(Key::kShift, true), "control plus shift does not fire");
    check(detector.on_key_event(Key::kX, true), "control plus shift plus x fires");
}

void does_not_refire_while_keys_are_held() {
    ComboDetector detector;
    detector.on_key_event(Key::kControl, true);
    detector.on_key_event(Key::kShift, true);
    check(detector.on_key_event(Key::kX, true), "first completion fires");
    check(!detector.on_key_event(Key::kX, true), "auto repeat of x does not refire");
    check(!detector.on_key_event(Key::kControl, true), "auto repeat of control does not refire");
}

void rearms_after_release_then_press() {
    ComboDetector detector;
    detector.on_key_event(Key::kControl, true);
    detector.on_key_event(Key::kShift, true);
    check(detector.on_key_event(Key::kX, true), "completion fires");
    check(!detector.on_key_event(Key::kX, false), "releasing x does not fire");
    check(detector.on_key_event(Key::kX, true), "pressing x again refires");
}

void order_of_presses_does_not_matter() {
    ComboDetector detector;
    detector.on_key_event(Key::kX, true);
    detector.on_key_event(Key::kShift, true);
    check(detector.on_key_event(Key::kControl, true), "x then shift then control fires on control");
}

void unrelated_keys_are_ignored() {
    ComboDetector detector;
    detector.on_key_event(Key::kControl, true);
    detector.on_key_event(Key::kShift, true);
    check(!detector.on_key_event(Key::kOther, true), "other key does not fire");
    check(detector.on_key_event(Key::kX, true), "combo still completes after other key");
}

void partial_combo_never_fires() {
    ComboDetector detector;
    check(!detector.on_key_event(Key::kControl, true), "control down");
    check(!detector.on_key_event(Key::kX, true), "control plus x without shift does not fire");
    check(!detector.all_three_pressed(), "not all three pressed");
}

void releasing_one_key_breaks_the_combo() {
    ComboDetector detector;
    detector.on_key_event(Key::kControl, true);
    detector.on_key_event(Key::kShift, true);
    check(detector.on_key_event(Key::kX, true), "fires once");
    detector.on_key_event(Key::kControl, false);
    check(!detector.all_three_pressed(), "combo broken after control release");
}

void classify_maps_every_relevant_key() {
    check(ComboDetector::classify(0x11) == Key::kControl, "0x11 is control");
    check(ComboDetector::classify(0xA2) == Key::kControl, "left control is control");
    check(ComboDetector::classify(0xA3) == Key::kControl, "right control is control");
    check(ComboDetector::classify(0x10) == Key::kShift, "0x10 is shift");
    check(ComboDetector::classify(0xA0) == Key::kShift, "left shift is shift");
    check(ComboDetector::classify(0xA1) == Key::kShift, "right shift is shift");
    check(ComboDetector::classify(0x58) == Key::kX, "0x58 is the x trigger");
    check(ComboDetector::classify(0x5A) == Key::kOther, "z is other");
    check(ComboDetector::classify(0x41) == Key::kOther, "letter a is other");
}

}

int main() {
    fires_once_when_all_three_pressed_in_order();
    does_not_refire_while_keys_are_held();
    rearms_after_release_then_press();
    order_of_presses_does_not_matter();
    unrelated_keys_are_ignored();
    partial_combo_never_fires();
    releasing_one_key_breaks_the_combo();
    classify_maps_every_relevant_key();

    if (g_failure_count == 0) {
        std::printf("\nALL PASSED\n");
        return 0;
    }
    std::printf("\n%d CHECK(S) FAILED\n", g_failure_count);
    return 1;
}
