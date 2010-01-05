#include <QtGui>
#include "prefdialog.h"
#include "ui_prefdialog.h"

PrefDialog::PrefDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::PrefDialog)
{
    ui->setupUi(this);
    _settings = new QSettings();
    connect(this, SIGNAL(accepted()), this, SLOT(configAccepted()));
    getPaDevlist();
}

PrefDialog::~PrefDialog()
{
    delete ui;
}

void PrefDialog::configAccepted()
{
    _settings->beginGroup("FreeSWITCH/conf");

    _settings->beginGroup("portaudio.conf");
    _settings->setValue("cid-name", ui->PaCallerIdNameEdit->text());
    _settings->setValue("cid-num", ui->PaCallerIdNumEdit->text());
    _settings->setValue("indev", ui->PaIndevCombo->currentIndex());
    _settings->setValue("outdev", ui->PaOutdevCombo->currentIndex());
    _settings->setValue("ringdev", ui->PaRingdevCombo->currentIndex());
    _settings->setValue("ring-file", ui->PaRingFileEdit->text());
    _settings->setValue("ring-interval", ui->PaRingIntervalSpin->value());
    _settings->setValue("hold-file", ui->PaHoldFileEdit->text());
    _settings->endGroup();

    _settings->endGroup();

}

void PrefDialog::getPaDevlist()
{
    QString result;
    int errorLine, errorColumn;
    QString errorMsg;

    if (g_FSHost.sendCmd("pa", "devlist xml", &result) != SWITCH_STATUS_SUCCESS)
    {
        QMessageBox::critical(this, tr("PortAudio error" ),
                             tr("Error querying audio devices."),
                             QMessageBox::Ok);
        return;
    }

    if (!_xmlPaDevList.setContent(result, &errorMsg, &errorLine, &errorColumn))
    {
        QMessageBox::critical(this, tr("PortAudio error" ),
                              tr("Error parsing output xml from pa devlist.\n%1 (Line:%2, Col:%3).").arg(errorMsg,
                                                                                                         errorLine,
                                                                                                         errorColumn),
                             QMessageBox::Ok);
        return;
    }
    QDomElement root = _xmlPaDevList.documentElement();
    if (root.tagName() != "xml")
    {
        QMessageBox::critical(this, tr("PortAudio error" ),
                              tr("Error parsing output xml from pa devlist. Root tag is not <xml>."),
                             QMessageBox::Ok);
        return;
    }
    QDomElement devices = root.firstChildElement("devices");
    if (devices.isNull())
    {
        QMessageBox::critical(this, tr("PortAudio error" ),
                              tr("Error parsing output xml from pa devlist. There is no <devices> tag."),
                             QMessageBox::Ok);
        return;
    }

    QDomElement child = devices.firstChildElement();
    if (child.isNull())
    {
        QMessageBox::critical(this, tr("PortAudio error" ),
                              tr("Error parsing output xml from pa devlist. There is no <device> tag."),
                             QMessageBox::Ok);
        return;
    }

    while (!child.isNull())
    {
        if (child.tagName() == "device")
        {
            QString id, name, inputs, outputs;
            id = child.attribute("id","-1");
            name = child.attribute("name","Null");
            inputs = child.attribute("inputs","0");
            outputs = child.attribute("outputs","0");
            if (inputs.toInt() != 0)
                ui->PaIndevCombo->addItem(name,inputs.toInt());
            if (outputs.toInt() != 0)
            {
                ui->PaOutdevCombo->addItem(name,inputs.toInt());
                ui->PaRingdevCombo->addItem(name,inputs.toInt());
            }
        }
        child = child.nextSiblingElement();
    }

    QDomElement bindings = root.firstChildElement("bindings");
    if (bindings.isNull())
    {
        QMessageBox::critical(this, tr("PortAudio error" ),
                              tr("Error parsing output xml from pa devlist. There is no <bindings> tag."),
                             QMessageBox::Ok);
        return;
    }

    child = devices.firstChildElement();
    if (child.isNull())
    {
        QMessageBox::critical(this, tr("PortAudio error" ),
                              tr("Error parsing output xml from pa devlist. There are no bindings."),
                             QMessageBox::Ok);
        return;
    }

    while (!child.isNull())
    {
        QString id;
        id = child.attribute("device","-1");

        if (child.tagName() == "ring")
            ui->PaRingdevCombo->setCurrentIndex(id.toInt());
        else if (child.tagName() == "input")
            ui->PaIndevCombo->setCurrentIndex(id.toInt());
        else if (child.tagName() == "ring")
            ui->PaOutdevCombo->setCurrentIndex(id.toInt());

        child = child.nextSiblingElement();
    }

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
