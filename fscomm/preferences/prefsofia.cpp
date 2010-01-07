#include <QtGui>
#include <fshost.h>
#include "prefsofia.h"

PrefSofia::PrefSofia(Ui::PrefDialog *ui, QObject *parent) :
        QObject(parent),
        _ui(ui)
{
    _settings = new QSettings();
}

void PrefSofia::readConfig()
{
}

void PrefSofia::writeConfig()
{
    _settings->beginGroup("FreeSWITCH/conf");
    _settings->beginGroup("sofia.conf");



    _settings->endGroup();
    _settings->endGroup();
}
