#include <QtGui>
#include "prefaccounts.h"

PrefAccounts::PrefAccounts(Ui::PrefDialog *ui) :
        _ui(ui)
{
    _settings = new QSettings();
}

void PrefAccounts::writeConfig()
{
    return;
}

void PrefAccounts::readConfig()
{
    _settings->beginGroup("FreeSWITCH/conf/accounts");
    foreach(QString accountName, _settings->childGroups())
    {
        _settings->beginGroup(accountName);
        QTableWidgetItem *item0 = new QTableWidgetItem(accountName);
        QTableWidgetItem *item1 = new QTableWidgetItem(_settings->value("username").toString());
        _settings->endGroup();
        _ui->accountsTable->setRowCount(_ui->accountsTable->rowCount()+1);
        _ui->accountsTable->setItem(_ui->accountsTable->rowCount()-1, 0, item0);
        _ui->accountsTable->setItem(_ui->accountsTable->rowCount()-1, 1, item1);
    }

    _settings->endGroup();
}
