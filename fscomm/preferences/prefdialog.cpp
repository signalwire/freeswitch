#include <QtGui>
#include "prefdialog.h"
#include "ui_prefdialog.h"
#include "prefportaudio.h"
#include "prefsofia.h"
#include "prefaccounts.h"

PrefDialog::PrefDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::PrefDialog)
{
    ui->setupUi(this);
    connect(this, SIGNAL(accepted()), this, SLOT(writeConfig()));
    connect(ui->buttonBox, SIGNAL(clicked(QAbstractButton*)), this, SLOT(clicked(QAbstractButton*)));

    _pref_accounts = new PrefAccounts(ui);
    _mod_portaudio = new PrefPortaudio(ui, this);
    connect(_mod_portaudio, SIGNAL(preprocessorsApplied(QStringList)), this, SIGNAL(preprocessorsApplied(QStringList)));
    _mod_sofia = new PrefSofia(ui, this);
    readConfig();
}

PrefDialog::~PrefDialog()
{
    delete ui;
}

void PrefDialog::clicked(QAbstractButton *b) {
    if (ui->buttonBox->buttonRole(b) == QDialogButtonBox::ApplyRole) {
        writeConfig();
        readConfig();
    }
    if ( ui->buttonBox->buttonRole(b) == QDialogButtonBox::RejectRole) {
        /* This doesn't really work because we need to reset the DOM as well to discard changes... */
        readConfig();
    }
}

void PrefDialog::writeConfig()
{    
    /* Ask modules to write their configs. */
    _mod_portaudio->writeConfig();
    _mod_sofia->writeConfig();
    _pref_accounts->writeConfig();

    /* Write it to file */
    ISettings settings(this);
    settings.saveToFile();

    /* Re-read the configuration to memory */
    const char *err;
    switch_xml_t xml_root;

    if ((xml_root = switch_xml_open_root(1, &err))) {
        switch_xml_free(xml_root);
    } else {
        QMessageBox::critical(0, tr("Unable to save settings"),
                              tr("There was an error saving your settings.\nPlease report this bug.\n%1").arg(err),
                              QMessageBox::Ok);
        return;
    }

    /* Tell modules new config is in memory so they get a chance */
    _mod_portaudio->postWriteConfig();
    _pref_accounts->postWriteConfig();
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
    _pref_accounts->readConfig();
    _mod_portaudio->readConfig();
    _mod_sofia->readConfig();
}
