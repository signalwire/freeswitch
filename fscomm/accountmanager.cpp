#include "accountmanager.h"


QList<QSharedPointer<Account> > AccountManager::_accounts;

AccountManager::AccountManager(QObject *parent) :
    QObject(parent)
{
    connect(g_FSHost, SIGNAL(newEvent(QSharedPointer<switch_event_t>)), this, SLOT(newEventSlot(QSharedPointer<switch_event_t>)));
}

void AccountManager::newEventSlot(QSharedPointer<switch_event_t> e) {
    QString eName = switch_event_get_header_nil(e.data(), "Event-Name");
    QString eSub = e.data()->subclass_name;
    qDebug() << eName;
    switch(e.data()->event_id) {
    case SWITCH_EVENT_CUSTOM:
        {
            qDebug() << eName << eSub;
            break;
        }
    case SWITCH_EVENT_API:
        {
            /* Might not be necessary anymore */
            break;
        }
    default:
        {
            break;
        }
    }
}
