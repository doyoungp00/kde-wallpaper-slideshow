#pragma once

#include <KCoreConfigSkeleton>
#include <QMetaMethod>
#include <QTimer>

// Saves skeleton shortly after any of its properties changes, so settings apply
// and persist without an Apply button.
inline void saveOnChange(KCoreConfigSkeleton *skeleton)
{
    auto *timer = new QTimer(skeleton);
    timer->setSingleShot(true);
    timer->setInterval(300);
    QObject::connect(timer, &QTimer::timeout, skeleton, &KCoreConfigSkeleton::save);
    const QMetaMethod start = timer->metaObject()->method(timer->metaObject()->indexOfSlot("start()"));
    for (int i = 0; i < skeleton->metaObject()->propertyCount(); ++i) {
        const QMetaProperty property = skeleton->metaObject()->property(i);
        if (property.hasNotifySignal()) {
            QObject::connect(skeleton, property.notifySignal(), timer, start);
        }
    }
}
