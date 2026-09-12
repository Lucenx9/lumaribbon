// SPDX-License-Identifier: GPL-3.0-or-later
#include "AudioState.h"
#include "AudioIngress.h"
#include <QCoreApplication>
#include <QSignalSpy>
#include <QTest>
#include <atomic>
#include <chrono>
#include <cmath>
#include <thread>

namespace {
std::atomic<bool> playTone{false};
std::atomic<unsigned> captures{0};
std::atomic<bool> captureUnavailable{false}, overflowNextPoll{false}, expireNextPoll{false};
using Clock = std::chrono::steady_clock;
}

// This test replaces only capture. Publication, notification coalescing,
// shared ownership and the QML-facing sample interface are production code.
namespace Luma {
struct PipeWireCapture::Impl {
    AudioIngress ingress;
    std::array<StereoFrame, 4800> frames{};
    explicit Impl(AudioRing &ring) : ingress(ring) { ++captures; }
    ~Impl() { --captures; }
};
PipeWireCapture::PipeWireCapture(AudioRing &ring) : impl(std::make_unique<Impl>(ring)) {}
PipeWireCapture::~PipeWireCapture() = default;
CaptureStatus PipeWireCapture::poll() {
    if (captureUnavailable.load())
        return {QStringLiteral("Test capture unavailable"), {}, true, 0, 0, QStringLiteral("Permission denied (13)")};
    const bool playing = playTone.load();
    // The format change resets the analyzer, allowing repeated audible/silent
    // transitions while the GUI is deliberately blocked for this queue test.
    const unsigned rate = playing ? 48000 : 8000;
    for (unsigned n = 0; n < rate / 10; ++n) {
        const float sample = playing ? 0.2f * std::sin(6.283185307179586 * 100 * n / rate) : 0;
        impl->frames[n] = {sample, -sample};
    }
    const double receivedAt = std::chrono::duration<double>(Clock::now().time_since_epoch()).count()
        - (expireNextPoll.exchange(false) ? 5.0 : 0.0);
    const unsigned batches = overflowNextPoll.exchange(false) ? 4 : 1;
    for (unsigned n = 0; n < batches; ++n)
        impl->ingress.write(reinterpret_cast<const char *>(impl->frames.data()), rate / 10,
            sizeof(StereoFrame), rate, playing ? 1 : 2, receivedAt);
    return {QStringLiteral("Test capture"), QStringLiteral("Synthetic"), false, rate, 2};
}
}

class AudioStateTests : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void unavailableRecoveryAndDiagnostics() {
        captureUnavailable.store(true);
        playTone.store(false);
        auto state = std::make_unique<AudioState>();
        auto engine = Luma::AudioEngine::acquire();
        QSignalSpy changes(state.get(), &AudioState::diagnosticsChanged);
        QTRY_VERIFY(state->hasError());
        QVERIFY(state->diagnostics().contains(QStringLiteral("Permission denied (13)")));
        QVERIFY(state->diagnostics().contains(QStringLiteral("Dropped audio blocks: 0")));
        QCOMPARE(state->sample()["energy"].toDouble(), 0.0);
        QSignalSpy wake(state.get(), &AudioState::audioAvailable);
        captureUnavailable.store(false);
        playTone.store(true);
        QTRY_VERIFY_WITH_TIMEOUT(state->sample()["energy"].toDouble() > 0.1, 500);
        QTRY_COMPARE(wake.count(), 1);
        QTRY_VERIFY(!state->hasError());
        QVERIFY(!state->diagnostics().contains(QStringLiteral("Permission denied")));
        QVERIFY(state->diagnostics().contains(QStringLiteral("48000 Hz")));
        state->reportRendering(true);
        QVERIFY(state->diagnostics().contains(QStringLiteral("Simple rendering")));
        overflowNextPoll.store(true);
        QTRY_VERIFY(engine->snapshot().dropped > 0);
        QTRY_VERIFY(!state->diagnostics().contains(QStringLiteral("Dropped audio blocks: 0")));
        expireNextPoll.store(true);
        QTRY_VERIFY(engine->snapshot().expired > 0);
        QTRY_VERIFY(!state->diagnostics().contains(QStringLiteral("Expired audio blocks: 0")));
        QVERIFY(changes.count() >= 4);
        captureUnavailable.store(true);
        playTone.store(false);
        QTRY_VERIFY(state->hasError());
        QTRY_VERIFY_WITH_TIMEOUT(state->sample()["energy"].toDouble() < 0.0005, 5000);
        const auto removal = Clock::now();
        state.reset();
        engine.reset();
        QVERIFY(Clock::now() - removal < std::chrono::milliseconds(500));
        QCOMPARE(captures.load(), 0u);
        captureUnavailable.store(false);
    }
    void coalescedWakeAndRemoval() {
        auto first = std::make_unique<AudioState>();
        auto second = std::make_unique<AudioState>();
        auto engine = Luma::AudioEngine::acquire();
        QSignalSpy firstWake(first.get(), &AudioState::audioAvailable);
        QSignalSpy secondWake(second.get(), &AudioState::audioAvailable);
        const auto awaitWithoutGui = [&](bool audible) {
            playTone.store(audible);
            const auto deadline = Clock::now() + std::chrono::seconds(2);
            while (Clock::now() < deadline) {
                const auto s = engine->snapshot();
                if (s.status.rate == (audible ? 48000u : 8000u)
                    && (audible ? s.features.energy > 0.1f : s.features.energy == 0)) return true;
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            return false;
        };
        QVERIFY(awaitWithoutGui(false));
        QCOMPARE(captures.load(), 1u);
        for (unsigned n = 0; n < 4; ++n) {
            QVERIFY(awaitWithoutGui(true));
            QVERIFY(awaitWithoutGui(false));
        }
        QVERIFY(awaitWithoutGui(true));
        QCOMPARE(firstWake.count(), 0); // The blocked GUI has not received anything.
        QCoreApplication::processEvents();
        QCOMPARE(firstWake.count(), 1); // Five starts produce at most one queued wake-up.
        QCOMPARE(secondWake.count(), 1);
        const auto quiet = first->sample(0.5), loud = second->sample(2);
        QVERIFY(quiet["energy"].toDouble() > 0);
        QVERIFY(loud["energy"].toDouble() >= quiet["energy"].toDouble());
        QCOMPARE(quiet["rippleOrigin"], loud["rippleOrigin"]);
        for (const char *key : {"arch", "counterBend", "bias", "opening", "lift", "lean"}) {
            QVERIFY(quiet.contains(key));
            QVERIFY(std::isfinite(quiet[key].toDouble()));
            QVERIFY(std::abs(quiet[key].toDouble() - loud[key].toDouble()) < 0.06);
        }
        // A newly created popup/instance reads a mature shared trajectory even
        // with different sensitivity. It must not start its own spring at zero.
        QTest::qWait(1600);
        {
            AudioState late;
            QCOMPARE(captures.load(), 1u);
            const auto current = first->sample(0.5), joined = late.sample(2);
            QVERIFY(current["arch"].toDouble() > 0.5);
            for (const char *key : {"arch", "counterBend", "bias", "opening", "lift", "lean"})
                QVERIFY(std::abs(current[key].toDouble() - joined[key].toDouble()) < 0.003);
        }
        QVERIFY(awaitWithoutGui(false));
        QVERIFY(awaitWithoutGui(true));
        QCoreApplication::processEvents();
        QCOMPARE(firstWake.count(), 2); // Delivery rearms the shared notifier.
        QCOMPARE(secondWake.count(), 2);
        first.reset();
        QCOMPARE(captures.load(), 1u);
        QVERIFY(awaitWithoutGui(false));
        QVERIFY(awaitWithoutGui(true)); // Leave a notification queued during final removal.
        second.reset();
        const auto removal = Clock::now();
        engine.reset();
        QVERIFY(Clock::now() - removal < std::chrono::milliseconds(500));
        QCOMPARE(captures.load(), 0u);
        QCoreApplication::processEvents(); // Must not invoke the destroyed engine or states.
    }
};
QTEST_GUILESS_MAIN(AudioStateTests)
#include "test_audio_state.moc"
