#pragma once

#include <QtGlobal>

#include <algorithm>

// How long an item stays on screen. timerMs is the base timer T, maxMs the
// maximum M, and loopMs the animation or video length L (negative when unknown
// or when the item is a still image).
inline qint64 effectiveDurationMs(qint64 timerMs, qint64 maxMs, qint64 loopMs, bool smart, bool roundUpToLoops)
{
    if (!smart || loopMs <= 0) {
        return timerMs;
    }
    const qint64 max = std::max(maxMs, timerMs);
    if (loopMs <= timerMs) {
        if (!roundUpToLoops) {
            return timerMs;
        }
        return std::min(max, (timerMs + loopMs - 1) / loopMs * loopMs);
    }
    return std::min(loopMs, max);
}
