#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace jfg::boot::native {
int run(const char *executable_path, const char *rom_path, unsigned watchdog_ms,
        bool child_mode, const char *child_token, const char *marker_handle,
        unsigned retrace_target, unsigned poll_target, bool play_mode);
}

namespace {
constexpr unsigned kDefaultWatchdogMs = 30000U;
constexpr unsigned kMaximumWatchdogMs = 3600000U;
constexpr unsigned kMaximumRetraceTarget = 1000000U;

struct NativeProfile {
  std::string rom;
  std::string flash;
  std::string controller_pak;
};

bool valid_profile_value(const std::string_view value) {
  return !value.empty() && value.size() <= 32767U &&
         value.find('\n') == std::string_view::npos &&
         value.find('\r') == std::string_view::npos &&
         value.find('\0') == std::string_view::npos;
}

bool load_profile(const std::string_view path, NativeProfile &profile) {
  if (!valid_profile_value(path))
    return false;
  std::ifstream stream(std::filesystem::path(std::string(path)),
                       std::ios::binary);
  if (!stream)
    return false;
  std::string line;
  if (!std::getline(stream, line) || line != "jfg-native-profile-v1")
    return false;
  bool rom_seen = false;
  bool flash_seen = false;
  bool pak_seen = false;
  while (std::getline(stream, line)) {
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    const std::size_t separator = line.find('=');
    if (separator == std::string::npos)
      return false;
    const std::string_view key(line.data(), separator);
    const std::string value = line.substr(separator + 1U);
    if (!valid_profile_value(value))
      return false;
    if (key == "rom" && !rom_seen) {
      profile.rom = value;
      rom_seen = true;
    } else if (key == "flash" && !flash_seen) {
      profile.flash = value;
      flash_seen = true;
    } else if (key == "controller_pak" && !pak_seen) {
      profile.controller_pak = value;
      pak_seen = true;
    } else {
      return false;
    }
  }
  return stream.eof() && rom_seen && flash_seen && pak_seen;
}

bool absolute_profile_path(const std::string_view input,
                           std::string &output) {
  if (!valid_profile_value(input))
    return false;
  std::error_code error;
  const std::filesystem::path path =
      std::filesystem::absolute(
          std::filesystem::path(std::string(input)), error)
          .lexically_normal();
  if (error || path.empty())
    return false;
  output = path.string();
  return valid_profile_value(output);
}

bool persist_profile(const std::string_view path,
                     const NativeProfile &profile) {
  if (!valid_profile_value(path) || !valid_profile_value(profile.rom) ||
      !valid_profile_value(profile.flash) ||
      !valid_profile_value(profile.controller_pak))
    return false;
  const std::filesystem::path destination{std::string(path)};
  if (destination.empty() || destination.filename().empty())
    return false;
  std::error_code error;
  const std::filesystem::path parent = destination.parent_path();
  if (!parent.empty()) {
    std::filesystem::create_directories(parent, error);
    if (error)
      return false;
  }
  std::filesystem::path temporary = destination;
  temporary += ".tmp";
  {
    std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
    stream << "jfg-native-profile-v1\n"
           << "rom=" << profile.rom << '\n'
           << "flash=" << profile.flash << '\n'
           << "controller_pak=" << profile.controller_pak << '\n';
    stream.flush();
    if (!stream)
      return false;
  }
#if defined(_WIN32)
  if (MoveFileExW(temporary.c_str(), destination.c_str(),
                  MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) == 0) {
    std::filesystem::remove(temporary, error);
    return false;
  }
#else
  std::filesystem::rename(temporary, destination, error);
  if (error) {
    std::filesystem::remove(temporary, error);
    return false;
  }
#endif
  return true;
}

bool configure_profile_environment(const NativeProfile &profile) {
#if defined(_WIN32)
  return _putenv_s("JFG_PHASE8_SAVE_PATH", profile.flash.c_str()) == 0 &&
         _putenv_s("JFG_PHASE8_CONTROLLER_PAK_PATH",
                   profile.controller_pak.c_str()) == 0;
#else
  return setenv("JFG_PHASE8_SAVE_PATH", profile.flash.c_str(), 1) == 0 &&
         setenv("JFG_PHASE8_CONTROLLER_PAK_PATH",
                profile.controller_pak.c_str(), 1) == 0;
#endif
}

bool resolve_profile(const std::string_view config_path,
                     const std::string_view rom_override,
                     const std::string_view flash_override,
                     const std::string_view pak_override,
                     const bool persist,
                     NativeProfile &profile) {
  const bool has_config = !config_path.empty();
  if (has_config) {
    std::error_code error;
    const bool exists = std::filesystem::exists(
        std::filesystem::path(std::string(config_path)), error);
    if (error || (exists && !load_profile(config_path, profile)))
      return false;
  }
  if (!rom_override.empty() && !absolute_profile_path(rom_override, profile.rom))
    return false;
  if (!flash_override.empty() &&
      !absolute_profile_path(flash_override, profile.flash))
    return false;
  if (!pak_override.empty() &&
      !absolute_profile_path(pak_override, profile.controller_pak))
    return false;
  if (has_config && (profile.flash.empty() || profile.controller_pak.empty())) {
    std::string absolute_config;
    if (!absolute_profile_path(config_path, absolute_config))
      return false;
    const std::filesystem::path directory =
        std::filesystem::path(absolute_config).parent_path();
    if (profile.flash.empty())
      profile.flash = (directory / "jfg.flash").string();
    if (profile.controller_pak.empty())
      profile.controller_pak = (directory / "controller-1.pak").string();
  }
  if (!valid_profile_value(profile.rom) ||
      (!profile.flash.empty() && !valid_profile_value(profile.flash)) ||
      (!profile.controller_pak.empty() &&
       !valid_profile_value(profile.controller_pak)))
    return false;
  if (persist && has_config && !persist_profile(config_path, profile))
    return false;
  return (profile.flash.empty() && profile.controller_pak.empty()) ||
         configure_profile_environment(profile);
}

unsigned default_watchdog_for_retraces(const unsigned retrace_target) {
  // Title/menu probes normally complete within 20 ms per requested retrace,
  // but the first post-save overlay has materially heavier generated CPU and
  // graphics work. Keep the watchdog bounded while allowing that live path to
  // make continuous progress.
  const auto scaled = static_cast<unsigned long long>(retrace_target) * 40ULL;
  return static_cast<unsigned>((std::min)(
      static_cast<unsigned long long>(kMaximumWatchdogMs),
      (std::max)(static_cast<unsigned long long>(kDefaultWatchdogMs),
                 scaled)));
}

bool parse_timeout(std::string_view text, unsigned &output) {
  unsigned value = 0U;
  const auto [end, error] =
      std::from_chars(text.data(), text.data() + text.size(), value);
  if (error != std::errc{} || end != text.data() + text.size() || value == 0U ||
      value > kMaximumWatchdogMs) {
    return false;
  }
  output = value;
  return true;
}

bool parse_retrace_target(std::string_view text, unsigned &output) {
  unsigned value = 0U;
  const auto [end, error] =
      std::from_chars(text.data(), text.data() + text.size(), value);
  if (error != std::errc{} || end != text.data() + text.size() || value < 3U ||
      value > kMaximumRetraceTarget) {
    return false;
  }
  output = value;
  return true;
}

bool parse_poll_target(std::string_view text, unsigned &output) {
  unsigned value = 0U;
  const auto [end, error] =
      std::from_chars(text.data(), text.data() + text.size(), value);
  if (error != std::errc{} || end != text.data() + text.size() || value == 0U ||
      value > kMaximumRetraceTarget) {
    return false;
  }
  output = value;
  return true;
}

void usage() {
  std::fprintf(
      stderr,
      "usage: jfg-native-boot [--config <path>] [--rom <path>] "
      "[--save <path>] [--controller-pak <path>] "
      "[--watchdog-ms <1..3600000>] "
      "[--probe-retraces <3..1000000> | --probe-polls <1..1000000> | --play] "
      "(a config may supply the ROM and persistent-device paths; "
      "default: max(30000 ms, 40 ms/retrace), 3 retraces)\n");
}
} // namespace

#if defined(_WIN32)
bool utf8_from_wide(const wchar_t *input, std::string &output) {
  const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, input, -1,
                                       nullptr, 0, nullptr, nullptr);
  if (size <= 1 || size > 32768)
    return false;
  output.resize(static_cast<std::size_t>(size));
  if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, input, -1,
                          output.data(), size, nullptr, nullptr) != size)
    return false;
  output.pop_back();
  return true;
}

int wmain(int argc, wchar_t **argv) {
  if(argc==2 && std::wcscmp(argv[1],L"--frontend-version")==0){std::puts("jfg-frontend-1");return 0;}
  const wchar_t *rom = nullptr;
  const wchar_t *config = nullptr;
  const wchar_t *save = nullptr;
  const wchar_t *controller_pak = nullptr;
  const wchar_t *child_token = nullptr;
  const wchar_t *marker_handle = nullptr;
  bool child_mode = false;
  bool timeout_explicit = false;
  bool retrace_target_explicit = false;
  bool poll_target_explicit = false;
  bool play_mode = false;
  unsigned timeout = kDefaultWatchdogMs;
  unsigned retrace_target = 3U;
  unsigned poll_target = 0U;
  for (int index = 1; index < argc; ++index) {
    if (std::wcscmp(argv[index], L"--rom") == 0 && index + 1 < argc) {
      rom = argv[++index];
    } else if (std::wcscmp(argv[index], L"--config") == 0 &&
               index + 1 < argc) {
      config = argv[++index];
    } else if (std::wcscmp(argv[index], L"--save") == 0 &&
               index + 1 < argc) {
      save = argv[++index];
    } else if (std::wcscmp(argv[index], L"--controller-pak") == 0 &&
               index + 1 < argc) {
      controller_pak = argv[++index];
    } else if (std::wcscmp(argv[index], L"--watchdog-ms") == 0 &&
               index + 1 < argc) {
      std::string value;
      if (!utf8_from_wide(argv[++index], value) ||
          !parse_timeout(value, timeout)) {
        usage();
        return 64;
      }
      timeout_explicit = true;
    } else if (std::wcscmp(argv[index], L"--_jfg-phase6-child") == 0) {
      child_mode = true;
    } else if (std::wcscmp(argv[index], L"--probe-retraces") == 0 &&
               index + 1 < argc) {
      std::string value;
      if (!utf8_from_wide(argv[++index], value) ||
          !parse_retrace_target(value, retrace_target)) {
        usage();
        return 64;
      }
      retrace_target_explicit = true;
    } else if (std::wcscmp(argv[index], L"--probe-polls") == 0 &&
               index + 1 < argc) {
      std::string value;
      if (!utf8_from_wide(argv[++index], value) ||
          !parse_poll_target(value, poll_target)) {
        usage();
        return 64;
      }
      poll_target_explicit = true;
    } else if (std::wcscmp(argv[index], L"--play") == 0) {
      play_mode = true;
    } else if (std::wcscmp(argv[index], L"--_jfg-phase6-token") == 0 &&
               index + 1 < argc) {
      child_token = argv[++index];
    } else if (std::wcscmp(argv[index], L"--_jfg-phase6-marker") == 0 &&
               index + 1 < argc) {
      marker_handle = argv[++index];
    } else {
      usage();
      return 64;
    }
  }
  std::string executable_utf8, rom_utf8, config_utf8, save_utf8, pak_utf8,
      token_utf8, marker_utf8;
  if ((play_mode && (retrace_target_explicit || poll_target_explicit)) ||
      (retrace_target_explicit && poll_target_explicit) ||
      !utf8_from_wide(argv[0], executable_utf8) ||
      (rom != nullptr && !utf8_from_wide(rom, rom_utf8)) ||
      (config != nullptr && !utf8_from_wide(config, config_utf8)) ||
      (save != nullptr && !utf8_from_wide(save, save_utf8)) ||
      (controller_pak != nullptr &&
       !utf8_from_wide(controller_pak, pak_utf8)) ||
      (child_mode && (child_token == nullptr || marker_handle == nullptr ||
                      !utf8_from_wide(child_token, token_utf8) ||
                      !utf8_from_wide(marker_handle, marker_utf8)))) {
    usage();
    return 64;
  }
  NativeProfile profile;
  if (!resolve_profile(config_utf8, rom_utf8, save_utf8, pak_utf8,
                       !child_mode, profile)) {
    std::fputs("native boot setup failed: persistent profile\n", stderr);
    return 2;
  }
  if (!timeout_explicit)
    timeout = default_watchdog_for_retraces(
        poll_target_explicit ? poll_target : retrace_target);
  return jfg::boot::native::run(executable_utf8.c_str(), profile.rom.c_str(),
                                timeout, child_mode, token_utf8.c_str(),
                                marker_utf8.c_str(), retrace_target, poll_target,
                                play_mode);
}
#else
int main(int argc, char **argv) {
  const char *rom = nullptr;
  const char *config = nullptr;
  const char *save = nullptr;
  const char *controller_pak = nullptr;
  const char *child_token = nullptr;
  const char *marker_handle = nullptr;
  bool child_mode = false;
  bool timeout_explicit = false;
  bool retrace_target_explicit = false;
  bool poll_target_explicit = false;
  bool play_mode = false;
  unsigned timeout = kDefaultWatchdogMs;
  unsigned retrace_target = 3U;
  unsigned poll_target = 0U;
  if (argc == 2 && std::strcmp(argv[1], "--frontend-version") == 0) {
    std::puts("jfg-frontend-1");
    return 0;
  }
  for (int index = 1; index < argc; ++index) {
    if (std::strcmp(argv[index], "--rom") == 0 && index + 1 < argc) {
      rom = argv[++index];
    } else if (std::strcmp(argv[index], "--config") == 0 &&
               index + 1 < argc) {
      config = argv[++index];
    } else if (std::strcmp(argv[index], "--save") == 0 &&
               index + 1 < argc) {
      save = argv[++index];
    } else if (std::strcmp(argv[index], "--controller-pak") == 0 &&
               index + 1 < argc) {
      controller_pak = argv[++index];
    } else if (std::strcmp(argv[index], "--watchdog-ms") == 0 &&
               index + 1 < argc && parse_timeout(argv[++index], timeout)) {
      timeout_explicit = true;
      continue;
    } else if (std::strcmp(argv[index], "--_jfg-phase6-child") == 0) {
      child_mode = true;
    } else if (std::strcmp(argv[index], "--probe-retraces") == 0 &&
               index + 1 < argc &&
               parse_retrace_target(argv[++index], retrace_target)) {
      retrace_target_explicit = true;
      continue;
    } else if (std::strcmp(argv[index], "--probe-polls") == 0 &&
               index + 1 < argc &&
               parse_poll_target(argv[++index], poll_target)) {
      poll_target_explicit = true;
      continue;
    } else if (std::strcmp(argv[index], "--play") == 0) {
      play_mode = true;
    } else if (std::strcmp(argv[index], "--_jfg-phase6-token") == 0 &&
               index + 1 < argc) {
      child_token = argv[++index];
    } else if (std::strcmp(argv[index], "--_jfg-phase6-marker") == 0 &&
               index + 1 < argc) {
      marker_handle = argv[++index];
    } else {
      usage();
      return 64;
    }
  }
  if ((play_mode && (retrace_target_explicit || poll_target_explicit)) ||
      (retrace_target_explicit && poll_target_explicit) ||
      (child_mode && (child_token == nullptr || marker_handle == nullptr))) {
    usage();
    return 64;
  }
  NativeProfile profile;
  if (!resolve_profile(config == nullptr ? "" : config,
                       rom == nullptr ? "" : rom,
                       save == nullptr ? "" : save,
                       controller_pak == nullptr ? "" : controller_pak,
                       !child_mode, profile)) {
    std::fputs("native boot setup failed: persistent profile\n", stderr);
    return 2;
  }
  if (!timeout_explicit)
    timeout = default_watchdog_for_retraces(
        poll_target_explicit ? poll_target : retrace_target);
  return jfg::boot::native::run(argv[0], profile.rom.c_str(), timeout,
                                child_mode, child_token, marker_handle,
                                retrace_target, poll_target, play_mode);
}
#endif
