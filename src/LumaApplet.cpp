// SPDX-License-Identifier: GPL-3.0-or-later
#include "AudioState.h"
#include <Plasma/Applet>
#include <KPluginFactory>

class LumaApplet : public Plasma::Applet {
    Q_OBJECT
    Q_PROPERTY(QObject *audio READ audio CONSTANT)
public:
    LumaApplet(QObject *parent, const KPluginMetaData &data, const QVariantList &args)
        : Plasma::Applet(parent, data, args), state(new AudioState(this)) {}
    QObject *audio() const { return state; }
private:
    AudioState *state;
};
K_PLUGIN_CLASS_WITH_JSON(LumaApplet, "native-metadata.json")
#include "LumaApplet.moc"
