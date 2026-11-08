#pragma once

#include <QHash>
#include <QJsonObject>
#include <QString>

class KCoreConfigSkeleton;
class QUrl;

// Settings as JSON for export and import. A settings file is
//   {"format": "kde-wallpaper-slideshow-settings", "version": 1,
//    "general": {<key>: <value>, ...}, "profiles": {"<profile id>": {...}, ...}}
// where either section may be missing.
namespace SettingsIO
{
// The skeleton's values by key. Enums are choice indexes, colors "#rrggbb" or "#aarrggbb".
QJsonObject toJson(const KCoreConfigSkeleton *skeleton);
// Sets each entry of values that names a setting and has the right type, clamped to
// the setting's range, then saves. enumCounts holds the number of choices of each enum
// setting. Returns the number of entries left out.
int apply(KCoreConfigSkeleton *skeleton, const QJsonObject &values, const QHash<QString, int> &enumCounts = {});
// Reports entries apply() left out; empty when there were none.
QString ignoredMessage(int ignored);

// Read and write a settings file. Return an error message, empty on success.
QString read(const QUrl &file, QJsonObject &root);
QString write(const QUrl &file, QJsonObject root);
}
