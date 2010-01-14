#include <QtGui>
#include "prefaccounts.h"
#include "accountdialog.h"

PrefAccounts::PrefAccounts(Ui::PrefDialog *ui) :
        _ui(ui)
{
    _settings = new QSettings();
    _accDlg = NULL;
    connect(_ui->sofiaGwAddBtn, SIGNAL(clicked()), this, SLOT(addAccountBtnClicked()));
}

void PrefAccounts::addAccountBtnClicked()
{
    if (!_accDlg)
    {
        _accDlg = new AccountDialog(_ui->accountsTable->rowCount());
        connect(_accDlg, SIGNAL(gwAdded()), this, SLOT(readConfig()));
    }

    _accDlg->show();
    _accDlg->raise();
    _accDlg->activateWindow();
}

void PrefAccounts::writeConfig()
{
    return;
}

void PrefAccounts::readConfig()
{

    _ui->accountsTable->clearContents();
    _ui->accountsTable->setRowCount(0);

    _settings->beginGroup("FreeSWITCH/conf/sofia.conf/profiles/profile/gateways");
    
    foreach(QString accId, _settings->childGroups())
    {
        _settings->beginGroup(accId);
        _settings->beginGroup("gateway/attrs");
        QTableWidgetItem *item0 = new QTableWidgetItem(_settings->value("name").toString());
        _settings->endGroup();
        _settings->beginGroup("gateway/params");
        QTableWidgetItem *item1 = new QTableWidgetItem(_settings->value("username").toString());
        _settings->endGroup();
        _settings->endGroup();
        _ui->accountsTable->setRowCount(_ui->accountsTable->rowCount()+1);
        _ui->accountsTable->setItem(_ui->accountsTable->rowCount()-1, 0, item0);
        _ui->accountsTable->setItem(_ui->accountsTable->rowCount()-1, 1, item1);
    }

    _settings->endGroup();
}
