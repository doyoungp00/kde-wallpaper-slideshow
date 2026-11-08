#include "statestore.h"

#include <KConfigGroup>

StateStore::StateStore(QObject *parent)
    : QObject(parent)
    , m_config(KSharedConfig::openStateConfig())
{
    m_firstLaunch = !m_config->hasGroup(QStringLiteral("General"));
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(300);
    connect(&m_debounce, &QTimer::timeout, this, &StateStore::flush);
}

void StateStore::markDirty()
{
    if (!m_debounce.isActive()) {
        m_debounce.start();
    }
}

void StateStore::flush()
{
    m_debounce.stop();
    if (m_writer) {
        m_writer(*m_config);
        m_config->sync();
    }
}
