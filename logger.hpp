#pragma once

#include <windows.h>

#include <cstdio>
#include <string>

class Logger {
public:
    Logger() {
        char temp_directory[MAX_PATH] = {0};
        GetTempPathA(MAX_PATH, temp_directory);
        path_ = std::string(temp_directory) + "screenguard_debug_log.txt";
        file_ = std::fopen(path_.c_str(), "w");
    }

    ~Logger() {
        if (file_ != nullptr) {
            std::fclose(file_);
        }
    }

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(Logger&&) = delete;

    bool is_open() const {
        return file_ != nullptr;
    }

    const std::string& path() const {
        return path_;
    }

    void write_line(const char* text) {
        if (file_ == nullptr) {
            return;
        }
        std::fprintf(file_, "[%lu] %s\n", static_cast<unsigned long>(GetTickCount()), text);
        std::fflush(file_);
    }

private:
    std::FILE* file_ = nullptr;
    std::string path_;
};
