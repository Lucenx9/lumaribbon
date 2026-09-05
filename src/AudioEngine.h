// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "AudioTypes.h"
#include "RibbonMotion.h"
#include "PipeWireCapture.h"
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>
#include <QObject>

namespace Luma {
struct AudioSnapshot {
    Features features;
    RibbonShape shape;
    CaptureStatus status;
    double phase = 0;
    double time = 0;
    uint32_t dropped = 0;
    uint32_t expired = 0;
};
class AudioEngine : public QObject {
    Q_OBJECT
public:
    static std::shared_ptr<AudioEngine> acquire();
    ~AudioEngine();
    AudioSnapshot snapshot() const;
Q_SIGNALS:
    void audioAvailable();
private:
    AudioEngine();
    void run(std::stop_token stop);
    mutable std::mutex snapshotMutex;
    AudioSnapshot latest;
    std::atomic<bool> wakePending{false};
    std::jthread worker;
};
}
