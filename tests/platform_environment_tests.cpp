#include "jfg/platform/environment.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace {

int failures = 0;

void check(const bool condition, const std::string_view message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

void set(const char* name, const char* value) {
#if defined(_WIN32)
    _putenv_s(name, value);
#else
    setenv(name, value, 1);
#endif
}

}  // namespace

int main() {
    using namespace jfg::platform;

    char* copy = reinterpret_cast<char*>(1);
    std::size_t length = 99U;
    check(dupenv_s(&copy, &length, "JFG_TEST_UNSET_VARIABLE") == 0 && copy == nullptr && length == 0U,
          "an unset variable yields a null buffer and success");

    set("JFG_TEST_FLAG", "1");
    check(dupenv_s(&copy, &length, "JFG_TEST_FLAG") == 0 && copy != nullptr &&
              std::string_view(copy) == "1" && length == 2U,
          "dupenv_s copies the value including its terminator");
    std::free(copy);

    char flag[2]{};
    check(get_environment_variable("JFG_TEST_FLAG", flag, 2U) == 1UL && flag[0] == '1',
          "a one-character value fits a two-byte buffer");
    set("JFG_TEST_LONG", "abcdef");
    check(get_environment_variable("JFG_TEST_LONG", flag, 2U) == 7UL,
          "an oversized value reports the required size including the terminator");
    check(get_environment_variable("JFG_TEST_UNSET_VARIABLE", flag, 2U) == 0UL,
          "an unset variable reports zero");

    std::filesystem::path path;
    check(get_environment_path("JFG_TEST_UNSET_VARIABLE", path) == PathVariable::unset,
          "an unset path variable is reported as unset");
    set("JFG_TEST_PATH", "some/location");
    check(get_environment_path("JFG_TEST_PATH", path) == PathVariable::ok &&
              path == std::filesystem::path("some/location"),
          "a path variable round-trips");
    set("JFG_TEST_HUGE", std::string(40000U, 'x').c_str());
    check(get_environment_path("JFG_TEST_HUGE", path) == PathVariable::too_long,
          "the 32768-character bound is enforced on every platform");

    if (failures != 0) {
        std::cerr << failures << " platform environment test(s) failed\n";
        return 1;
    }
    std::cout << "platform environment tests passed\n";
    return 0;
}
