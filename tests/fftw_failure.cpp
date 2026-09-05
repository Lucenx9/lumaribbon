// SPDX-License-Identifier: GPL-3.0-or-later
// Linked only into the recovery test: fail the next requested plan.
#include <fftw3.h>
#include <atomic>
#include <dlfcn.h>

namespace { std::atomic<bool> failNext{false}; }
extern "C" void luma_fail_next_fftw_plan() { failNext.store(true); }
extern "C" fftwf_plan fftwf_plan_dft_r2c_1d(int n, float *input, fftwf_complex *output, unsigned flags) {
    if (failNext.exchange(false)) return nullptr;
    using CreatePlan = fftwf_plan (*)(int, float *, fftwf_complex *, unsigned);
    static const auto createPlan = reinterpret_cast<CreatePlan>(dlsym(RTLD_NEXT, "fftwf_plan_dft_r2c_1d"));
    return createPlan ? createPlan(n, input, output, flags) : nullptr;
}
