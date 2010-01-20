#include <QtGui>
#include "account.h"

Account::Account(QString name) :
    _name(name)
{
    QSettings settings;
    settings.beginGroup("FreeSWITCH/conf/sofia.conf/profiles/profile/gateways");
    foreach(QString g, settings.childGroups())
    {
        settings.beginGroup(g);
        if(settings.value("gateway/attrs/name").toString() == name)
        {
            _uuid = g;
            break;
        }
    }
}
