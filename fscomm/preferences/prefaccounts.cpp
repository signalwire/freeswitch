#include <QtGui>
#include "prefaccounts.h"
#include "accountdialog.h"
#include "fshost.h"

PrefAccounts::PrefAccounts(Ui::PrefDialog *ui) :
        _ui(ui)
{
    _settings = new QSettings();
    _accDlg = NULL;
    connect(_ui->sofiaGwAddBtn, SIGNAL(clicked()), this, SLOT(addAccountBtnClicked()));
    connect(_ui->sofiaGwRemBtn, SIGNAL(clicked()), this, SLOT(remAccountBtnClicked()));
    connect(_ui->sofiaGwEditBtn, SIGNAL(clicked()), this, SLOT(editAccountBtnClicked()));

    _ui->accountsTable->horizontalHeader()->setStretchLastSection(true);
}

void PrefAccounts::addAccountBtnClicked()
{
    if (!_accDlg)
    {
        QString uuid;
        if (g_FSHost.sendCmd("create_uuid", "", &uuid) != SWITCH_STATUS_SUCCESS)
        {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not create UUID for account. Reason: %s\n", uuid.toAscii().constData());
            return;
        }
        _accDlg = new AccountDialog(uuid);
        connect(_accDlg, SIGNAL(gwAdded(QString)), this, SLOT(readConfig()));
    }
    else
    {
        QString uuid;
        if (g_FSHost.sendCmd("create_uuid", "", &uuid) != SWITCH_STATUS_SUCCESS)
        {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not create UUID for account. Reason: %s\n", uuid.toAscii().constData());
            return;
        }
        _accDlg->setAccId(uuid);
        _accDlg->clear();
    }

    _accDlg->show();
    _accDlg->raise();
    _accDlg->activateWindow();
}

void PrefAccounts::editAccountBtnClicked()
{
    QList<QTableWidgetSelectionRange> selList = _ui->accountsTable->selectedRanges();

    if (selList.isEmpty())
        return;
    QTableWidgetSelectionRange range = selList[0];

    QString uuid = _ui->accountsTable->item(range.topRow(),0)->data(Qt::UserRole).toString();

    if (!_accDlg)
    {
        _accDlg = new AccountDialog(uuid);
        connect(_accDlg, SIGNAL(gwAdded(QString)), this, SLOT(readConfig()));
    }
    else
    {
        _accDlg->setAccId(uuid);
    }

    _accDlg->readConfig();

    _accDlg->show();
    _accDlg->raise();
    _accDlg->activateWindow();
}

void PrefAccounts::remAccountBtnClicked()
{
    QList<QTableWidgetSelectionRange> sel = _ui->accountsTable->selectedRanges();
    int offset =0;

    foreach(QTableWidgetSelectionRange range, sel)
    {
        for(int row = range.topRow(); row<=range.bottomRow(); row++)
        {
            QTableWidgetItem *item = _ui->accountsTable->item(row-offset,0);

            _settings->beginGroup("FreeSWITCH/conf/sofia.conf/profiles/profile/gateways");
            _settings->remove(item->data(Qt::UserRole).toString());
            _settings->endGroup();
            /* Fire event to remove account */
            switch_event_t *event;
            if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, FSCOMM_EVENT_ACC_REMOVED) == SWITCH_STATUS_SUCCESS) {
                QSharedPointer<Account> acc = g_FSHost.getAccountByUUID(item->data(Qt::UserRole).toString());
                if (!acc.isNull())
                {
                    QString res;
                    QString arg = QString("profile softphone killgw %1").arg(acc.data()->getName());

                    if (g_FSHost.sendCmd("sofia", arg.toAscii().data() , &res) != SWITCH_STATUS_SUCCESS)
                    {
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not killgw %s from profile softphone.\n",
                                          acc.data()->getName().toAscii().data());
                    }
                }
            }
            _ui->accountsTable->removeRow(row-offset);
            offset++;
        }
    }

    if (offset > 0)
        readConfig(false);
}

void PrefAccounts::writeConfig()
{
    return;
}

void PrefAccounts::readConfig(bool reload)
{

    _ui->accountsTable->clearContents();
    _ui->accountsTable->setRowCount(0);

    _settings->beginGroup("FreeSWITCH/conf/sofia.conf/profiles/profile/gateways");
    
    foreach(QString accId, _settings->childGroups())
    {
        _settings->beginGroup(accId);
        _settings->beginGroup("gateway/attrs");
        QTableWidgetItem *item0 = new QTableWidgetItem(_settings->value("name").toString());
        item0->setData(Qt::UserRole, accId);
        _settings->endGroup();
        _settings->beginGroup("gateway/params");
        QTableWidgetItem *item1 = new QTableWidgetItem(_settings->value("username").toString());
        item1->setData(Qt::UserRole, accId);
        _settings->endGroup();
        _settings->endGroup();
        _ui->accountsTable->setRowCount(_ui->accountsTable->rowCount()+1);
        _ui->accountsTable->setItem(_ui->accountsTable->rowCount()-1, 0, item0);
        _ui->accountsTable->setItem(_ui->accountsTable->rowCount()-1, 1, item1);
    }
    _ui->accountsTable->resizeRowsToContents();
    _ui->accountsTable->resizeColumnsToContents();
    _ui->accountsTable->horizontalHeader()->setStretchLastSection(true);

    _settings->endGroup();

    if (reload)
    {
        QString res;
        if (g_FSHost.sendCmd("sofia", "profile softphone rescan", &res) != SWITCH_STATUS_SUCCESS)
        {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not rescan the softphone profile.\n");
            return;
        }
    }

    if (_ui->accountsTable->rowCount() == 1)
    {
        QString default_gateway = _settings->value(QString("/FreeSWITCH/conf/sofia.conf/profiles/profile/gateways/%1/gateway/attrs/name").arg(_ui->accountsTable->item(0,0)->data(Qt::UserRole).toString())).toString();
        _settings->beginGroup("FreeSWITCH/conf/globals");
        _settings->setValue("default_gateway", default_gateway);
        _settings->endGroup();
        switch_core_set_variable("default_gateway", default_gateway.toAscii().data());
    }

}
