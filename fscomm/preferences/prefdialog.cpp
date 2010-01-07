#include <QtGui>
#include "prefdialog.h"
#include "ui_prefdialog.h"
#include "prefportaudio.h"
#include "prefsofia.h"
#include "accountdialog.h"

PrefDialog::PrefDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::PrefDialog)
{
    ui->setupUi(this);
    _settings = new QSettings();
    connect(this, SIGNAL(accepted()), this, SLOT(writeConfig()));
    connect(ui->sofiaGwAddBtn, SIGNAL(clicked()), this, SLOT(addAccountBtnClicked()));

    _accDlg = NULL;
    _mod_portaudio = new PrefPortaudio(ui, this);
    _mod_sofia = new PrefSofia(ui, this);
    readConfig();
}

PrefDialog::~PrefDialog()
{
    delete ui;
}

void PrefDialog::addAccountBtnClicked()
{
    if (!_accDlg)
        _accDlg = new AccountDialog();

    _accDlg->show();
    _accDlg->raise();
    _accDlg->activateWindow();
}

void PrefDialog::writeConfig()
{    
    _mod_portaudio->writeConfig();
    /*_mod_sofia->writeConfig();*/
}

void PrefDialog::changeEvent(QEvent *e)
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

void PrefDialog::readConfig()
{
    _mod_portaudio->readConfig();
    /*_mod_sofia->readConfig();*/
}
