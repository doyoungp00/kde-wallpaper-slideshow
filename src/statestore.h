#pragma once

#include <KSharedConfig>
#include <QObject>
#include <QTimer>

#include <functional>

// Runtime state in ~/.local/state, kept apart from the user's settings. Writes are
// debounced; KConfig replaces the file atomically on each sync.
class StateStore : public QObject
{
    Q_OBJECT
public:
    using Writer = std::function<void(KSharedConfig &)>;

    explicit StateStore(QObject *parent = nullptr);

    KSharedConfig &config()
    {
        return *m_config;
    }
    bool isFirstLaunch() const
    {
        return m_firstLaunch;
    }
    void setWriter(Writer writer)
    {
        m_writer = std::move(writer);
    }

    void markDirty();
    void flush();

private:
    KSharedConfig::Ptr m_config;
    Writer m_writer;
    QTimer m_debounce;
    bool m_firstLaunch;
};
