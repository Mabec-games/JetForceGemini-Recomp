// Verifies, with no ROM, the host mechanism behind the Linux MMIO monitor:
// a PROT_NONE page faults on first access, the SIGSEGV handler opens it for
// exactly one instruction using the x86 trap flag, and the SIGTRAP handler
// observes the retired instruction and closes the page again. The runner's
// device emulation (src/boot/native_boot.cpp) depends on every access taking
// this two-phase path, for reads and for writes alike.
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <sys/mman.h>
#include <ucontext.h>

namespace {

constexpr std::size_t kPageSize = 0x1000U;
void* g_page = nullptr;
volatile std::sig_atomic_t g_faults = 0;
volatile std::sig_atomic_t g_write_faults = 0;
volatile std::sig_atomic_t g_steps = 0;
volatile std::sig_atomic_t g_pending = 0;

void on_fault(int, siginfo_t* info, void* context) {
    auto* const user = static_cast<ucontext_t*>(context);
    const auto address = reinterpret_cast<std::uintptr_t>(info->si_addr);
    const auto begin = reinterpret_cast<std::uintptr_t>(g_page);
    if (address < begin || address >= begin + kPageSize) {
        std::signal(SIGSEGV, SIG_DFL);
        return;
    }
    (void)::mprotect(g_page, kPageSize, PROT_READ | PROT_WRITE);
    g_faults = g_faults + 1;
    if ((user->uc_mcontext.gregs[REG_ERR] & 0x2) != 0)
        g_write_faults = g_write_faults + 1;
    g_pending = 1;
    user->uc_mcontext.gregs[REG_EFL] |= 0x100;
}

void on_step(int, siginfo_t*, void* context) {
    auto* const user = static_cast<ucontext_t*>(context);
    if (!g_pending)
        return;
    g_pending = 0;
    g_steps = g_steps + 1;
    (void)::mprotect(g_page, kPageSize, PROT_NONE);
    user->uc_mcontext.gregs[REG_EFL] &= ~static_cast<greg_t>(0x100);
}

}  // namespace

int main() {
    g_page = ::mmap(nullptr, kPageSize, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (g_page == MAP_FAILED)
        return 2;
    struct sigaction action{};
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_SIGINFO;
    action.sa_sigaction = on_fault;
    if (::sigaction(SIGSEGV, &action, nullptr) != 0)
        return 2;
    action.sa_sigaction = on_step;
    if (::sigaction(SIGTRAP, &action, nullptr) != 0)
        return 2;

    volatile std::uint32_t* const registers = static_cast<volatile std::uint32_t*>(g_page);
    constexpr int kRounds = 500;
    for (int round = 0; round < kRounds; ++round) {
        registers[1] = static_cast<std::uint32_t>(round);
        if (registers[1] != static_cast<std::uint32_t>(round)) {
            std::fputs("FAIL: a trapped write was not visible to the following read\n", stderr);
            return 1;
        }
    }
    if (g_faults != 2 * kRounds || g_write_faults != kRounds || g_steps != 2 * kRounds) {
        std::fprintf(stderr, "FAIL: faults=%d writes=%d steps=%d (expected %d/%d/%d)\n",
                     g_faults, g_write_faults, g_steps, 2 * kRounds, kRounds, 2 * kRounds);
        return 1;
    }
    std::puts("mmio trap mechanism tests passed");
    return 0;
}
