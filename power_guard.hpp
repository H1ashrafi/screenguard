#pragma once

#include <windows.h>

class PowerGuard {
public:
    PowerGuard() {
        SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED);
    }

    ~PowerGuard() {
        SetThreadExecutionState(ES_CONTINUOUS);
    }

    PowerGuard(const PowerGuard&) = delete;
    PowerGuard& operator=(const PowerGuard&) = delete;
    PowerGuard(PowerGuard&&) = delete;
    PowerGuard& operator=(PowerGuard&&) = delete;
};
