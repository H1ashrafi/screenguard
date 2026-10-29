#include <windows.h>

#include <cstring>

#include "application.hpp"

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR command_line, int) {
    const char* arguments = command_line != nullptr ? command_line : "";
    const bool auto_lock_on_start = std::strstr(arguments, "autolock") != nullptr;
    const bool debug_logging = std::strstr(arguments, "debug") != nullptr;
    Application application(instance, auto_lock_on_start, debug_logging);
    return application.run();
}
