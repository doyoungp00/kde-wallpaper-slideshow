#pragma once

#include <QQuickAsyncImageProvider>
#include <QThreadPool>

// Loads still images for the slideshow as image://media/<mode>/<encoded path>,
// where mode is the scale mode (0 cover, 1 fit, 2 stretch). Files load in
// parallel and are decoded at the size the mode needs on screen; a load that is
// no longer wanted stops reading, which matters on slow network shares.
class MediaImageProvider : public QQuickAsyncImageProvider
{
public:
    MediaImageProvider();

    QQuickImageResponse *requestImageResponse(const QString &id, const QSize &requestedSize) override;

private:
    QThreadPool m_pool;
};
