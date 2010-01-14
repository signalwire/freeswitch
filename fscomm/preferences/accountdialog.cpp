#include <QSettings>
#include <QtGui>
#include "accountdialog.h"
#include "ui_accountdialog.h"
#include "fshost.h"

AccountDialog::AccountDialog(int accId, QWidget *parent) :
    QDialog(parent),
    _accId(accId),
    ui(new Ui::AccountDialog)
{
    ui->setupUi(this);
    _settings = new QSettings;
    connect(this, SIGNAL(accepted()), this, SLOT(writeConfig()));
    connect(ui->sofiaExtraParamAddBtn, SIGNAL(clicked()), this, SLOT(addExtraParam()));
    connect(ui->sofiaExtraParamRemBtn, SIGNAL(clicked()), this, SLOT(remExtraParam()));
}

AccountDialog::~AccountDialog()
{
    delete ui;
}

void AccountDialog::remExtraParam()
{
    QList<QTableWidgetSelectionRange> sel = ui->sofiaExtraParamTable->selectedRanges();

    foreach(QTableWidgetSelectionRange range, sel)
    {
        int offset =0;
        for(int row = range.topRow(); row<=range.bottomRow(); row++)
        {
            ui->sofiaExtraParamTable->removeRow(row-offset);
            offset++;
        }
    }
}

void AccountDialog::addExtraParam()
{
    bool ok;
    QString paramName = QInputDialog::getText(this, tr("Add parameter."),
                                         tr("New parameter name:"), QLineEdit::Normal,
                                         NULL, &ok);
    if (!ok)
        return;
    QString paramVal = QInputDialog::getText(this, tr("Add parameter."),
                                         tr("New parameter value:"), QLineEdit::Normal,
                                         NULL, &ok);
    if (!ok)
        return;

    QTableWidgetItem* paramNameItem = new QTableWidgetItem(paramName);
    QTableWidgetItem* paramValItem = new QTableWidgetItem(paramVal);
    ui->sofiaExtraParamTable->setRowCount(ui->sofiaExtraParamTable->rowCount()+1);
    ui->sofiaExtraParamTable->setItem(ui->sofiaExtraParamTable->rowCount()-1,0,paramNameItem);
    ui->sofiaExtraParamTable->setItem(ui->sofiaExtraParamTable->rowCount()-1,1,paramValItem);
}

void AccountDialog::writeConfig()
{
    _settings->beginGroup("FreeSWITCH/conf/sofia.conf/profiles/profile/gateways");

    _settings->beginGroup(QString::number(_accId));
    
    _settings->beginGroup("gateway/attrs");
    _settings->setValue("name", ui->sofiaGwNameEdit->text());
    _settings->endGroup();


    _settings->beginGroup("gateway/params");
    _settings->setValue("username", ui->sofiaGwUsernameEdit->text());
    _settings->setValue("realm", ui->sofiaGwRealmEdit->text());
    _settings->setValue("password", ui->sofiaGwPasswordEdit->text());
    _settings->setValue("extension", ui->sofiaGwExtensionEdit->text());
    _settings->setValue("expire-seconds", ui->sofiaGwExpireSecondsSpin->value());
    _settings->setValue("register", ui->sofiaGwRegisterCombo->currentText());
    _settings->setValue("register-transport", ui->sofiaGwRegisterTransportCombo->currentText());
    _settings->setValue("retry-seconds", ui->sofiaGwRetrySecondsSpin->value());    
    for (int i = 0; i< ui->sofiaExtraParamTable->rowCount(); i++)
    {
        _settings->setValue(ui->sofiaExtraParamTable->item(i, 0)->text(),
                            ui->sofiaExtraParamTable->item(i, 1)->text());
    }
    _settings->endGroup();

    _settings->endGroup();

    _settings->endGroup();

    QString res;
    if (g_FSHost.sendCmd("sofia", "profile softphone rescan", &res) != SWITCH_STATUS_SUCCESS)
    {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not rescan the softphone profile.\n");
        return;
    }
    emit gwAdded();
}

void AccountDialog::changeEvent(QEvent *e)
{
    QDialog::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        break;
    default:
        break;
    }
}
