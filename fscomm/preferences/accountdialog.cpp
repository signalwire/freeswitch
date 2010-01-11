#include <QSettings>
#include <QtGui>
#include "accountdialog.h"
#include "ui_accountdialog.h"

AccountDialog::AccountDialog(QWidget *parent) :
    QDialog(parent),
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
    _settings->beginGroup("FreeSWITCH/conf/accounts");

    _settings->beginGroup(ui->sofiaGwNameEdit->text());
    _settings->setValue("username", ui->sofiaGwUsernameEdit->text());
    _settings->setValue("realm", ui->sofiaGwRealmEdit->text());
    _settings->setValue("password", ui->sofiaGwPasswordEdit->text());
    _settings->setValue("extension", ui->sofiaGwExtensionEdit->text());
    _settings->setValue("expire-seconds", ui->sofiaGwExpireSecondsSpin->value());
    _settings->setValue("register", ui->sofiaGwRegisterCombo->currentText());
    _settings->setValue("register-transport", ui->sofiaGwRegisterTransportCombo->currentText());
    _settings->setValue("retry-seconds", ui->sofiaGwRetrySecondsSpin->value());

    _settings->beginGroup("customParams");
    for (int i = 0; i< ui->sofiaExtraParamTable->rowCount(); i++)
    {
        _settings->setValue(ui->sofiaExtraParamTable->item(i, 0)->text(),
                            ui->sofiaExtraParamTable->item(i, 1)->text());
    }
    _settings->endGroup();

    _settings->endGroup();

    _settings->endGroup();
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
