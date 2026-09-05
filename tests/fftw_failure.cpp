// SPDX-License-Identifier: GPL-3.0-or-later
// Linked only into the recovery test: inject standard or unexpected failures.
#include <fftw3.h>
#include <atomic>
#include <dlfcn.h>

namespace {
enum class Failure { None, Allocation, Unexpected };
struct UnexpectedFailure {};
std::atomic<Failure> failNext{Failure::None};
}
extern "C" void luma_fail_next_fftw_plan() { failNext.store(Failure::Allocation); }
extern "C" void luma_throw_next_fftw_plan() { failNext.store(Failure::Unexpected); }
extern "C" fftwf_plan fftwf_plan_dft_r2c_1d(int n, float *input, fftwf_complex *output, unsigned flags) {
    const auto failure = failNext.exchange(Failure::None);
    if (failure == Failure::Allocation) return nullptr;
    if (failure == Failure::Unexpected) throw UnexpectedFailure{};
    using CreatePlan = fftwf_plan (*)(int, float *, fftwf_complex *, unsigned);
    static const auto createPlan = reinterpret_cast<CreatePlan>(dlsym(RTLD_NEXT, "fftwf_plan_dft_r2c_1d"));
    return createPlan ? createPlan(n, input, output, flags) : nullptr;
}
