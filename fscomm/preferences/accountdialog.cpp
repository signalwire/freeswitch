#include <QSettings>
#include "accountdialog.h"
#include "ui_accountdialog.h"

AccountDialog::AccountDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AccountDialog)
{
    ui->setupUi(this);
    _settings = new QSettings;
    connect(this, SIGNAL(accepted()), this, SLOT(writeConfig()));
}

AccountDialog::~AccountDialog()
{
    delete ui;
}

void AccountDialog::writeConfig()
{
    _settings->beginGroup("FreeSWITCH/conf/accounts");

    _settings->beginGroup(ui->sofiaGwNameEdit->text());
    _settings->setValue("username", ui->sofiaGwUsernameEdit->text());
    _settings->setValue("realm", ui->sofiaGwRealmEdit->text());
    _settings->setValue("from-user", ui->sofiaGwFromUserEdit->text());
    _settings->setValue("from-domain", ui->sofiaGwFromDomainEdit->text());
    _settings->setValue("password", ui->sofiaGwPasswordEdit->text());
    _settings->setValue("extension", ui->sofiaGwExtensionEdit->text());
    _settings->setValue("proxy", ui->sofiaGwProxyEdit->text());
    _settings->setValue("register-proxy", ui->sofiaGwRegisterProxyEdit->text());
    _settings->setValue("expire-seconds", ui->sofiaGwExpireSecondsSpin->value());
    _settings->setValue("register", ui->sofiaGwRegisterCombo->currentText());
    _settings->setValue("register-transport", ui->sofiaGwRegisterTransportCombo->currentText());
    _settings->setValue("retry-seconds", ui->sofiaGwRetrySecondsSpin->value());
    _settings->setValue("caller-id-in-from", ui->sofiaGwCallerIdInFromCombo->currentText());
    _settings->setValue("contact-params", ui->sofiaGwContactParamsEdit->text());
    _settings->setValue("ping", ui->sofiaGwPingSpin->value());
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
