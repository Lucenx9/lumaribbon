// SPDX-License-Identifier: GPL-3.0-or-later
#include "PipeWireCapture.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <iostream>

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir runtime;
    if (!runtime.isValid()) return 1;
    qputenv("PIPEWIRE_RUNTIME_DIR", runtime.path().toUtf8());
    qputenv("PIPEWIRE_REMOTE", "pipewire-0");
    Luma::AudioRing ring;
    Luma::PipeWireCapture capture(ring);
    const auto status = capture.poll();
    if (!status.error || status.detail.isEmpty() || status.rate != 0) {
        std::cerr << "An unavailable server must expose its connection error\n";
        return 1;
    }
    const auto retry = capture.poll();
    if (retry.detail != status.detail || !retry.error) {
        std::cerr << "The error detail must survive the retry backoff\n";
        return 1;
    }
    std::cout << "PASS connection error detail and retry persistence\n";
}
