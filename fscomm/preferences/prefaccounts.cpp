#include <QtGui>
#include "prefaccounts.h"
#include "accountdialog.h"

PrefAccounts::PrefAccounts(Ui::PrefDialog *ui) :
        _ui(ui)
{
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
        _accDlg = new AccountDialog();
        connect(_accDlg, SIGNAL(gwAdded(QString)), this, SLOT(readConfig()));
    }
    else
    {
        /* Needs to be set to empty because we are not editing */
        _accDlg->setName(QString());
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

    /* Get the selected item */
    QString gwName = _ui->accountsTable->item(range.topRow(),0)->text();

    if (!_accDlg)
    {
        /* TODO: We need a way to read this sucker... Might as well just already pass the profile name */
        _accDlg = new AccountDialog();
        connect(_accDlg, SIGNAL(gwAdded(QString)), this, SLOT(readConfig()));
    }

    /* TODO: Should pass the profile name someday */
    _accDlg->setName(gwName);
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

            ISettings settings(this);
            QDomElement cfg = settings.getConfigNode("sofia.conf");
            QDomNodeList gws = cfg.elementsByTagName("gateway");
            for (int i = 0; i < gws.count(); i++) {
                QDomElement gw = gws.at(i).toElement();
                if ( gw.attributeNode("name").value() == item->text()) {
                    cfg.elementsByTagName("gateways").at(0).removeChild(gw);
                    break;
                }
            }
            settings.setConfigNode(cfg, "sofia.conf");

            /* Mark the account to be deleted */
            _toDelete.append(item->text());

            _ui->accountsTable->removeRow(row-offset);
            offset++;
        }
    }

    if (offset)
        readConfig();
}

void PrefAccounts::writeConfig()
{
    return;
}

void PrefAccounts::postWriteConfig() {

    QString res;
    if (g_FSHost->sendCmd("sofia", "profile softphone rescan", &res) != SWITCH_STATUS_SUCCESS)
    {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not rescan the softphone profile.\n");
    }

    foreach (QString gw, _toDelete) {
        if (g_FSHost->sendCmd("sofia", QString("profile softphone killgw %1").arg(gw).toAscii().constData(), &res) != SWITCH_STATUS_SUCCESS)
        {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not remove gateway from profile [%s].\n", gw.toAscii().constData());
        }
    }
}

void PrefAccounts::readConfig()
{

    _ui->accountsTable->clearContents();
    _ui->accountsTable->setRowCount(0);

    ISettings settings(this);

    QDomElement cfg = settings.getConfigNode("sofia.conf");

    if ( cfg.elementsByTagName("gateways").count() == 0 ) {
        QDomElement profile = cfg.elementsByTagName("profile").at(0).toElement();
        QDomDocument d = profile.toDocument();
        QDomElement gws = d.createElement("gateways");
        profile.insertBefore(gws, QDomNode()); /* To make it look nicer */
        settings.setConfigNode(cfg, "sofia.conf");
        return;
    }

    QDomNodeList l = cfg.elementsByTagName("gateway");

    for (int i = 0; i < l.count(); i++) {
        QDomElement gw = l.at(i).toElement();
        QTableWidgetItem *item0 = new QTableWidgetItem(gw.attribute("name"));
        QTableWidgetItem *item1 = NULL;
        /* Iterate until we find what we need */
        QDomNodeList params = gw.elementsByTagName("param");
        for(int j = 0; i < params.count(); j++) {
            QDomElement e = params.at(j).toElement();
            QString var = e.attributeNode("name").value();
            if (var == "username" ) {
                item1 = new QTableWidgetItem(e.attributeNode("value").value());
                break; /* We found, so stop looping */
            }
        }
        _ui->accountsTable->setRowCount(_ui->accountsTable->rowCount()+1);
        _ui->accountsTable->setItem(_ui->accountsTable->rowCount()-1, 0, item0);
        _ui->accountsTable->setItem(_ui->accountsTable->rowCount()-1, 1, item1);
    }

    _ui->accountsTable->resizeRowsToContents();
    _ui->accountsTable->resizeColumnsToContents();
    _ui->accountsTable->horizontalHeader()->setStretchLastSection(true);

    /*
    TODO: We have to figure out what to do with the default account stuff!
    if (_ui->accountsTable->rowCount() == 1)
    {
        QString default_gateway = _settings->value(QString("/FreeSWITCH/conf/sofia.conf/profiles/profile/gateways/%1/gateway/attrs/name").arg(_ui->accountsTable->item(0,0)->data(Qt::UserRole).toString())).toString();
        _settings->beginGroup("FreeSWITCH/conf/globals");
        _settings->setValue("default_gateway", default_gateway);
        _settings->endGroup();
        switch_core_set_variable("default_gateway", default_gateway.toAscii().data());
    }*/

}
