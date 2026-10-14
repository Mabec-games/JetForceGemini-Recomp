#pragma once
#include <string_view>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cstdlib>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace jfg {
// This separate stream contains only host diagnostic categories, never guest
// bytes or paths. It is opened/flushed per event so process crashes retain it.
inline void support_event(const std::string_view event) noexcept {
#if defined(_WIN32)
    if (event.empty() || event.size() > 160U ||
        event.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789=_-/") != std::string_view::npos) return;
    wchar_t path[32768]{};
    const DWORD length = GetEnvironmentVariableW(L"JFG_SUPPORT_LOG", path, 32768U);
    if (length == 0U || length >= 32768U) return;
    HANDLE file = CreateFileW(path, FILE_APPEND_DATA | GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                              nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    LARGE_INTEGER size{};
    if (GetFileSizeEx(file, &size) && size.QuadPart <= 65536 - 162) {
        char line[162]{};
        for (std::size_t i = 0; i < event.size(); ++i) line[i] = event[i];
        line[event.size()] = '\n';
        DWORD written = 0;
        (void)WriteFile(file, line, static_cast<DWORD>(event.size() + 1U), &written, nullptr);
        (void)FlushFileBuffers(file);
    }
    CloseHandle(file);
#else
    if (event.empty() || event.size() > 160U ||
        event.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789=_-/") != std::string_view::npos) return;
    const char *const path = std::getenv("JFG_SUPPORT_LOG");
    if (path == nullptr || *path == '\0') return;
    const int file = ::open(path, O_WRONLY | O_APPEND | O_CREAT | O_CLOEXEC, 0600);
    if (file < 0) return;
    struct stat info{};
    if (::fstat(file, &info) == 0 && info.st_size <= 65536 - 162) {
        char line[162]{};
        for (std::size_t i = 0; i < event.size(); ++i) line[i] = event[i];
        line[event.size()] = '\n';
        (void)::write(file, line, event.size() + 1U);
        (void)::fsync(file);
    }
    ::close(file);
#endif
}
} // namespace jfg
