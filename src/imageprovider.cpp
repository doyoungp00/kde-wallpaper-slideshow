#include "imageprovider.h"
#include "core/iogate.h"

#include <QBuffer>
#include <QFile>
#include <QImageReader>
#include <QRunnable>
#include <QUrl>

#include <atomic>

namespace
{
class MediaImageResponse : public QQuickImageResponse, public QRunnable
{
public:
    MediaImageResponse(const QString &path, int mode, const QSize &target)
        : m_path(path)
        , m_mode(mode)
        , m_target(target)
    {
        setAutoDelete(false);
    }

    void run() override
    {
        load();
        Q_EMIT finished(); // may be emitted from any thread
    }

    void cancel() override
    {
        m_canceled = true;
    }

    QQuickTextureFactory *textureFactory() const override
    {
        return QQuickTextureFactory::textureFactoryForImage(m_image);
    }

    QString errorString() const override
    {
        return m_error;
    }

private:
    void load()
    {
        QFile file(m_path);
        bool opened;
        {
            IoGate::Scope io;
            opened = file.open(QIODevice::ReadOnly);
        }
        if (!opened) {
            m_error = file.errorString();
            return;
        }
        // Read in chunks so a skipped item stops tying up the share, and so the
        // system can sleep between two of them.
        QByteArray data;
        data.reserve(file.size());
        while (!file.atEnd()) {
            if (m_canceled) {
                m_error = QStringLiteral("canceled");
                return;
            }
            QByteArray chunk;
            {
                IoGate::Scope io;
                chunk = file.read(1 << 20);
            }
            if (chunk.isEmpty()) {
                m_error = file.errorString();
                return;
            }
            data += chunk;
        }

        QBuffer buffer(&data);
        QImageReader reader(&buffer);
        reader.setAutoTransform(true);
        QSize size = reader.size();
        if (reader.transformation() & QImageIOHandler::TransformationRotate90) {
            size.transpose();
        }
        if (size.isValid() && m_target.isValid() && !m_target.isEmpty()) {
            // Cover and stretch need every target pixel covered; fit needs the whole image inside.
            const qreal sx = qreal(m_target.width()) / size.width();
            const qreal sy = qreal(m_target.height()) / size.height();
            const qreal scale = m_mode == 1 ? std::min(sx, sy) : std::max(sx, sy);
            if (scale < 1) {
                QSize scaled = (QSizeF(size) * scale).toSize().expandedTo({1, 1});
                if (reader.transformation() & QImageIOHandler::TransformationRotate90) {
                    scaled.transpose(); // the reader scales before rotating
                }
                reader.setScaledSize(scaled);
            }
        }
        if (!reader.read(&m_image)) {
            m_error = reader.errorString();
        }
    }

    const QString m_path;
    const int m_mode;
    const QSize m_target;
    std::atomic_bool m_canceled = false;
    QImage m_image;
    QString m_error;
};
}

MediaImageProvider::MediaImageProvider()
{
    m_pool.setMaxThreadCount(4);
}

QQuickImageResponse *MediaImageProvider::requestImageResponse(const QString &id, const QSize &requestedSize)
{
    const qsizetype slash = id.indexOf(QLatin1Char('/'));
    const int mode = id.left(slash).toInt();
    const QString path = QUrl::fromPercentEncoding(id.mid(slash + 1).toUtf8());
    auto *response = new MediaImageResponse(path, mode, requestedSize);
    m_pool.start(response);
    return response;
}
