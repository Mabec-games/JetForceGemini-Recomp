#pragma once

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace jfg::platform {

// Copies an environment variable into a malloc'd buffer that the caller
// releases with std::free. An unset variable yields a null buffer and success,
// matching the Windows CRT's _dupenv_s.
inline int dupenv_s(char **buffer, std::size_t *size, const char *name) noexcept {
#if defined(_WIN32)
    return ::_dupenv_s(buffer, size, name);
#else
    if (buffer == nullptr || name == nullptr)
        return 22;
    *buffer = nullptr;
    if (size != nullptr)
        *size = 0U;
    const char *const value = std::getenv(name);
    if (value == nullptr)
        return 0;
    const std::size_t length = std::strlen(value) + 1U;
    char *const copy = static_cast<char *>(std::malloc(length));
    if (copy == nullptr)
        return 12;
    std::memcpy(copy, value, length);
    *buffer = copy;
    if (size != nullptr)
        *size = length;
    return 0;
#endif
}

// Matches GetEnvironmentVariableA: returns the value length without the
// terminator when it fits, the required size including the terminator when it
// does not, and zero when the variable is unset.
inline unsigned long get_environment_variable(const char *name, char *buffer,
                                              const unsigned long capacity) noexcept {
#if defined(_WIN32)
    return ::GetEnvironmentVariableA(name, buffer, capacity);
#else
    const char *const value = name == nullptr ? nullptr : std::getenv(name);
    if (value == nullptr)
        return 0UL;
    const std::size_t length = std::strlen(value);
    if (length + 1U > capacity)
        return static_cast<unsigned long>(length + 1U);
    std::memcpy(buffer, value, length + 1U);
    return static_cast<unsigned long>(length);
#endif
}

enum class PathVariable { unset, ok, too_long };

// Reads a path-valued variable. Windows limits environment values to 32767
// characters; that bound is kept on every platform so launchers behave alike.
inline PathVariable get_environment_path(const char *name,
                                         std::filesystem::path &output) {
    constexpr std::size_t kLimit = 32768U;
#if defined(_WIN32)
    std::wstring wide_name;
    for (const char *c = name; *c != '\0'; ++c)
        wide_name.push_back(static_cast<wchar_t>(*c));
    std::wstring value(kLimit, L'\0');
    const DWORD length = ::GetEnvironmentVariableW(
        wide_name.c_str(), value.data(), static_cast<DWORD>(kLimit));
    if (length == 0U)
        return PathVariable::unset;
    if (length >= kLimit)
        return PathVariable::too_long;
    value.resize(length);
    output = std::filesystem::path(value);
    return PathVariable::ok;
#else
    const char *const value = std::getenv(name);
    if (value == nullptr || *value == '\0')
        return PathVariable::unset;
    if (std::strlen(value) >= kLimit)
        return PathVariable::too_long;
    output = std::filesystem::path(value);
    return PathVariable::ok;
#endif
}

}  // namespace jfg::platform
