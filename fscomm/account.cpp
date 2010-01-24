#include <QtGui>
#include "account.h"

Account::Account(QString name) :
    _name(name)
{
    _statusCode = QString();
    _statusPhrase = QString();

    QSettings settings;
    settings.beginGroup("FreeSWITCH/conf/sofia.conf/profiles/profile/gateways");
    foreach(QString g, settings.childGroups())
    {
        settings.beginGroup(g);
        if(settings.value("gateway/attrs/name").toString() == name)
        {
            _uuid = g;
            settings.endGroup();
            break;
        }
        settings.endGroup();
    }
}

QString Account::getStateName()
{
    if (_statusPhrase.isEmpty())
        return fscomm_gw_state_names[_state];

    return QString("%1 - %2").arg(fscomm_gw_state_names[_state], _statusPhrase);
}
