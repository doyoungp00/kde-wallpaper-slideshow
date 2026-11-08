#pragma once

#include <QIODevice>
#include <QString>

#include <optional>

// Header-only parsing of animation loop lengths. No frame data is decoded.
namespace MediaProbe
{
// Returned when the file holds a single frame.
constexpr qint64 Still = -1;

// Loop length in ms, Still, or nullopt when the data is not a valid GIF/WebP.
std::optional<qint64> gifLoopMs(QIODevice &device);
std::optional<qint64> webpLoopMs(QIODevice &device);

// Dispatches on the file extension (gif or webp).
std::optional<qint64> loopLengthMs(const QString &filePath);
}
