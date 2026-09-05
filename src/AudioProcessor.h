// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "AudioRing.h"
#include "SignalAnalyzer.h"

namespace Luma {
// Consumer cadence and stream discontinuities live here so deterministic tests
// exercise the same path as the worker. tick() and ingress use the same monotonic clock.
class AudioProcessor {
public:
    explicit AudioProcessor(AudioRing &source) : ring(source) {}
    Features tick(double time);
    uint32_t expiredBlocks() const { return expired; }
private:
    AudioRing &ring;
    SignalAnalyzer analyzer;
    double previousTime = 0, lastAudio = 0, timeout = 0.12;
    uint32_t generation = 0;
    uint32_t expired = 0;
    uint64_t sequence = 0;
    bool haveBlock = false;
};
}
