// SPDX-License-Identifier: GPL-3.0-or-later
#include "AudioState.h"
#include <Plasma/Applet>
#include <KPluginFactory>
#include <QSizeF>

class LumaApplet : public Plasma::Applet {
    Q_OBJECT
    Q_PROPERTY(QObject *audio READ audio CONSTANT)
    Q_PROPERTY(int appearanceRevision READ appearanceRevision CONSTANT)
    Q_PROPERTY(QSizeF previewSize READ previewSize WRITE setPreviewSize NOTIFY previewSizeChanged)
public:
    LumaApplet(QObject *parent, const KPluginMetaData &data, const QVariantList &args)
        : Plasma::Applet(parent, data, args), state(new AudioState(this)) {}
    QObject *audio() const { return state; }
    int appearanceRevision() const { return 3; } // Hue rotation and the multicolor palette.
    QSizeF previewSize() const { return m_previewSize; }
    void setPreviewSize(QSizeF size) {
        if (size.isEmpty() || size == m_previewSize) return;
        m_previewSize = size;
        Q_EMIT previewSizeChanged();
    }
Q_SIGNALS:
    void previewSizeChanged();
private:
    AudioState *state;
    QSizeF m_previewSize{120, 40};
};
K_PLUGIN_CLASS_WITH_JSON(LumaApplet, "native-metadata.json")
#include "LumaApplet.moc"
