#include "dbusinterface.h"
#include "app.h"
#include "playbackcontroller.h"

#include <QDBusConnection>

DBusInterface::DBusInterface(App *app)
    : QObject(app)
    , m_app(app)
{
    QDBusConnection::sessionBus().registerObject(QStringLiteral("/Slideshow"), this, QDBusConnection::ExportScriptableSlots);
}

void DBusInterface::next()
{
    if (PlaybackController *c = m_app->primaryController()) {
        c->next();
    }
}

void DBusInterface::previous()
{
    if (PlaybackController *c = m_app->primaryController()) {
        c->previous();
    }
}

void DBusInterface::first()
{
    if (PlaybackController *c = m_app->primaryController()) {
        c->first();
    }
}

void DBusInterface::last()
{
    if (PlaybackController *c = m_app->primaryController()) {
        c->last();
    }
}

void DBusInterface::togglePause()
{
    m_app->togglePause();
}

void DBusInterface::toggleHidden()
{
    m_app->toggleHidden();
}

void DBusInterface::jumpTo(int n)
{
    if (PlaybackController *c = m_app->primaryController()) {
        c->jumpTo(n - 1);
    }
}

void DBusInterface::showSettings()
{
    m_app->showSettings();
}

void DBusInterface::quit()
{
    m_app->quit();
}

void DBusInterface::setFullscreenActive(bool active)
{
    m_app->setFullscreenActive(active);
}
