#include "settingsio.h"

#include <KCoreConfigSkeleton>
#include <QColor>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QUrl>

#include <cmath>
#include <limits>

namespace
{
const QString Format = QStringLiteral("kde-wallpaper-slideshow-settings");
constexpr int Version = 1;

double bound(const QVariant &limit, double fallback)
{
    return limit.isValid() ? limit.toDouble() : fallback;
}

// value converted to the type of item's setting, or invalid when it doesn't fit.
QVariant fromJson(const QJsonValue &value, const KConfigSkeletonItem *item, int enumCount)
{
    switch (item->property().metaType().id()) {
    case QMetaType::Bool:
        return value.isBool() ? QVariant(value.toBool()) : QVariant();
    case QMetaType::Int: {
        const double d = value.toDouble(std::nan(""));
        if (!value.isDouble() || d != std::trunc(d)) {
            return {};
        }
        if (enumCount >= 0) {
            return d >= 0 && d < enumCount ? QVariant(int(d)) : QVariant();
        }
        const double min = bound(item->minValue(), std::numeric_limits<int>::min());
        const double max = bound(item->maxValue(), std::numeric_limits<int>::max());
        return int(std::clamp(d, min, max));
    }
    case QMetaType::Double: {
        if (!value.isDouble()) {
            return {};
        }
        const double min = bound(item->minValue(), -std::numeric_limits<double>::max());
        const double max = bound(item->maxValue(), std::numeric_limits<double>::max());
        return std::clamp(value.toDouble(), min, max);
    }
    case QMetaType::QString:
        return value.isString() ? QVariant(value.toString()) : QVariant();
    case QMetaType::QStringList: {
        if (!value.isArray()) {
            return {};
        }
        QStringList list;
        for (const QJsonValue &v : value.toArray()) {
            if (!v.isString()) {
                return {};
            }
            list.append(v.toString());
        }
        return list;
    }
    case QMetaType::QColor: {
        const QColor color = QColor::fromString(value.toString());
        return value.isString() && color.isValid() ? QVariant(color) : QVariant();
    }
    }
    return {};
}
}

QJsonObject SettingsIO::toJson(const KCoreConfigSkeleton *skeleton)
{
    QJsonObject values;
    for (const KConfigSkeletonItem *item : skeleton->items()) {
        const QVariant value = item->property();
        if (value.metaType().id() == QMetaType::QColor) {
            const QColor color = value.value<QColor>();
            values.insert(item->name(), color.name(color.alpha() == 255 ? QColor::HexRgb : QColor::HexArgb));
        } else {
            values.insert(item->name(), QJsonValue::fromVariant(value));
        }
    }
    return values;
}

int SettingsIO::apply(KCoreConfigSkeleton *skeleton, const QJsonObject &values, const QHash<QString, int> &enumCounts)
{
    int ignored = 0;
    for (auto it = values.begin(); it != values.end(); ++it) {
        KConfigSkeletonItem *item = skeleton->findItem(it.key());
        const QVariant value = item ? fromJson(it.value(), item, enumCounts.value(it.key(), -1)) : QVariant();
        if (value.isValid()) {
            item->setProperty(value);
        } else {
            ++ignored;
        }
    }
    skeleton->save();
    return ignored;
}

QString SettingsIO::ignoredMessage(int ignored)
{
    if (ignored == 0) {
        return QString();
    }
    return ignored == 1 ? QStringLiteral("1 entry in the file was unknown or invalid and was left out.")
                        : QStringLiteral("%1 entries in the file were unknown or invalid and were left out.").arg(ignored);
}

QString SettingsIO::read(const QUrl &file, QJsonObject &root)
{
    QFile in(file.toLocalFile());
    if (!in.open(QIODevice::ReadOnly)) {
        return QStringLiteral("Cannot open %1: %2").arg(in.fileName(), in.errorString());
    }
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(in.readAll(), &error);
    if (!doc.isObject()) {
        return QStringLiteral("%1 is not a JSON settings file: %2").arg(in.fileName(), error.errorString());
    }
    root = doc.object();
    if (root.value(QLatin1String("format")).toString() != Format) {
        return QStringLiteral("%1 is not a Wallpaper Slideshow settings file.").arg(in.fileName());
    }
    if (root.value(QLatin1String("version")).toInt() > Version) {
        return QStringLiteral("%1 was made by a newer version of Wallpaper Slideshow.").arg(in.fileName());
    }
    return QString();
}

QString SettingsIO::write(const QUrl &file, QJsonObject root)
{
    root.insert(QLatin1String("format"), Format);
    root.insert(QLatin1String("version"), Version);
    QSaveFile out(file.toLocalFile());
    if (!out.open(QIODevice::WriteOnly)) {
        return QStringLiteral("Cannot write %1: %2").arg(out.fileName(), out.errorString());
    }
    out.write(QJsonDocument(root).toJson());
    if (!out.commit()) {
        return QStringLiteral("Cannot write %1: %2").arg(out.fileName(), out.errorString());
    }
    return QString();
}
