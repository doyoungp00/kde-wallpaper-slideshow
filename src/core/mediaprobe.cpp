#include "mediaprobe.h"

#include <QFile>

namespace
{
bool skipSubBlocks(QIODevice &d)
{
    for (;;) {
        char size;
        if (!d.getChar(&size)) {
            return false;
        }
        if (size == 0) {
            return true;
        }
        const qint64 n = quint8(size);
        if (d.skip(n) != n) {
            return false;
        }
    }
}

quint32 le24(const char *p)
{
    return quint8(p[0]) | quint8(p[1]) << 8 | quint8(p[2]) << 16;
}

quint32 le32(const char *p)
{
    return le24(p) | quint32(quint8(p[3])) << 24;
}
}

namespace MediaProbe
{
std::optional<qint64> gifLoopMs(QIODevice &d)
{
    const QByteArray header = d.read(13);
    if (header.size() < 13 || !(header.startsWith("GIF87a") || header.startsWith("GIF89a"))) {
        return std::nullopt;
    }
    const quint8 screenFlags = header[10];
    if (screenFlags & 0x80) {
        d.skip(3 * (1 << ((screenFlags & 7) + 1)));
    }

    int frames = 0;
    qint64 totalCs = 0;
    int delayCs = 0;
    for (;;) {
        char introducer;
        if (!d.getChar(&introducer)) {
            break; // Truncated; count the frames seen so far.
        }
        if (quint8(introducer) == 0x21) {
            char label;
            if (!d.getChar(&label)) {
                break;
            }
            if (quint8(label) == 0xF9) {
                char size;
                if (!d.getChar(&size)) {
                    break;
                }
                const QByteArray gce = d.read(quint8(size));
                if (gce.size() >= 3) {
                    delayCs = quint8(gce[1]) | quint8(gce[2]) << 8;
                }
            }
            if (!skipSubBlocks(d)) {
                break;
            }
        } else if (quint8(introducer) == 0x2C) {
            const QByteArray desc = d.read(9);
            if (desc.size() < 9) {
                break;
            }
            const quint8 flags = desc[8];
            if (flags & 0x80) {
                d.skip(3 * (1 << ((flags & 7) + 1)));
            }
            d.skip(1); // LZW minimum code size
            ++frames;
            // Browsers and Qt treat 0 and 1 cs as 10 cs.
            totalCs += delayCs <= 1 ? 10 : delayCs;
            delayCs = 0;
            if (!skipSubBlocks(d)) {
                break;
            }
        } else {
            break; // 0x3B trailer, or garbage
        }
    }
    if (frames == 0) {
        return std::nullopt;
    }
    return frames > 1 ? totalCs * 10 : Still;
}

std::optional<qint64> webpLoopMs(QIODevice &d)
{
    const QByteArray header = d.read(12);
    if (header.size() < 12 || !header.startsWith("RIFF") || header.mid(8, 4) != "WEBP") {
        return std::nullopt;
    }
    int frames = 0;
    qint64 totalMs = 0;
    for (;;) {
        const QByteArray chunk = d.read(8);
        if (chunk.size() < 8) {
            break;
        }
        const qint64 size = le32(chunk.constData() + 4);
        const qint64 padded = size + (size & 1);
        if (chunk.startsWith("ANMF") && size >= 16) {
            const QByteArray frame = d.read(16);
            if (frame.size() < 16) {
                break;
            }
            ++frames;
            totalMs += le24(frame.constData() + 12);
            if (d.skip(padded - 16) != padded - 16) {
                break;
            }
        } else if (d.skip(padded) != padded) {
            break;
        }
    }
    return frames > 1 ? totalMs : Still;
}

std::optional<qint64> loopLengthMs(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    if (filePath.endsWith(QLatin1String(".gif"), Qt::CaseInsensitive)) {
        return gifLoopMs(file);
    }
    if (filePath.endsWith(QLatin1String(".webp"), Qt::CaseInsensitive)) {
        return webpLoopMs(file);
    }
    return Still;
}
}
