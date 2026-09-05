// SPDX-License-Identifier: GPL-3.0-or-later
#include "AudioProcessor.h"
#include <algorithm>

namespace Luma {
Features AudioProcessor::tick(double time) {
    const auto dt = float(std::clamp(time - previousTime, 0.0, 10.0));
    previousTime = time;
    AudioBlock block;
    bool received = false;
    for (unsigned n = 0; n < AudioRing::Capacity && ring.pop(block); ++n) {
        // Large quanta legitimately arrive less often. Expire time spent waiting
        // for this consumer, without rejecting normally delivered low-rate audio.
        const double budget = std::max(0.12, 1.5 * (double(block.quantumFrames) + AudioBlock::Capacity) / block.rate);
        if (time - block.receivedAt > budget) {
            ++expired;
            continue;
        }
        if (!received && time - lastAudio > timeout) analyzer.decay(dt);
        if (haveBlock && (block.generation != generation || block.sequence != sequence + 1)) analyzer.discontinuity();
        generation = block.generation;
        sequence = block.sequence;
        haveBlock = true;
        analyzer.feed(std::span(block.samples.data(), block.count), block.rate);
        timeout = budget;
        lastAudio = block.receivedAt;
        received = true;
    }
    if (!received && time - lastAudio > timeout) analyzer.decay(dt);
    return analyzer.features();
}
}
