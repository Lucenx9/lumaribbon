// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "AudioRing.h"
#include <QString>
#include <memory>

namespace Luma {
struct CaptureStatus {
    QString message;
    QString device;
    bool error = false;
    uint32_t rate = 0;
    uint32_t channels = 0;
    QString detail{};
};
// Construct, maintain, and destroy on the analysis thread. No Qt event loop needed.
class PipeWireCapture {
public:
    explicit PipeWireCapture(AudioRing &ring);
    ~PipeWireCapture();
    CaptureStatus poll();
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
}
