// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "AudioEngine.h"
#include <QObject>
#include <QTimer>
#include <QVariantMap>

class AudioState : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusChanged)
    Q_PROPERTY(QString deviceName READ deviceName NOTIFY statusChanged)
    Q_PROPERTY(bool hasError READ hasError NOTIFY statusChanged)
    Q_PROPERTY(QString formatDescription READ formatDescription NOTIFY statusChanged)
    Q_PROPERTY(QString renderingStatus READ renderingStatus NOTIFY renderingStatusChanged)
    Q_PROPERTY(QString diagnostics READ diagnostics NOTIFY diagnosticsChanged)
public:
    explicit AudioState(QObject *parent = nullptr);
    Q_INVOKABLE QVariantMap sample(double sensitivity = 1.0) const;
    Q_INVOKABLE void reportRendering(bool simple);
    QString statusMessage() const { return status.message; }
    QString deviceName() const { return status.device; }
    bool hasError() const { return status.error; }
    QString formatDescription() const;
    QString renderingStatus() const { return rendererStatus; }
    QString diagnostics() const;
Q_SIGNALS:
    void audioAvailable();
    void statusChanged();
    void renderingStatusChanged();
    void diagnosticsChanged();
private:
    std::shared_ptr<Luma::AudioEngine> engine;
    Luma::CaptureStatus status;
    QTimer statusTimer;
    QString rendererStatus;
    uint32_t dropped = 0, expired = 0;
};
