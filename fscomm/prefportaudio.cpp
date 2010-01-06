#include <QtGui>
#include <fshost.h>
#include "prefportaudio.h"

PrefPortaudio::PrefPortaudio(Ui::PrefDialog *ui, QObject *parent) :
    QObject(parent),
    _ui(ui)
{
    _settings = new QSettings();
}

void PrefPortaudio::writeConfig()
{
    _settings->beginGroup("FreeSWITCH/conf");
    _settings->beginGroup("portaudio.conf");
    _settings->setValue("cid-name", _ui->PaCallerIdNameEdit->text());
    _settings->setValue("cid-num", _ui->PaCallerIdNumEdit->text());
    _settings->setValue("indev", _ui->PaIndevCombo->currentIndex());
    _settings->setValue("outdev", _ui->PaOutdevCombo->currentIndex());
    _settings->setValue("ringdev", _ui->PaRingdevCombo->currentIndex());
    _settings->setValue("ring-file", _ui->PaRingFileEdit->text());
    _settings->setValue("ring-interval", _ui->PaRingIntervalSpin->value());
    _settings->setValue("hold-file", _ui->PaHoldFileEdit->text());
    _settings->endGroup();
    _settings->endGroup();
}

void PrefPortaudio::readConfig()
{
    getPaDevlist();
    _settings->beginGroup("FreeSWITCH/conf");

    _settings->beginGroup("portaudio.conf");
    _ui->PaCallerIdNameEdit->setText(_settings->value("cid-name").toString());
    _ui->PaCallerIdNumEdit->setText(_settings->value("cid-num").toString());
    _ui->PaHoldFileEdit->setText(_settings->value("hold-file").toString());
    _ui->PaRingFileEdit->setText(_settings->value("ring-file").toString());
    _ui->PaRingIntervalSpin->setValue(_settings->value("ring-interval").toInt());
    _settings->endGroup();

    _settings->endGroup();
}

void PrefPortaudio::getPaDevlist()
{
    QString result;
    int errorLine, errorColumn;
    QString errorMsg;

    if (g_FSHost.sendCmd("pa", "devlist xml", &result) != SWITCH_STATUS_SUCCESS)
    {
        QMessageBox::critical(0, tr("PortAudio error" ),
                             tr("Error querying audio devices."),
                             QMessageBox::Ok);
        return;
    }

    if (!_xmlPaDevList.setContent(result, &errorMsg, &errorLine, &errorColumn))
    {
        QMessageBox::critical(0, tr("PortAudio error" ),
                              tr("Error parsing output xml from pa devlist.\n%1 (Line:%2, Col:%3).").arg(errorMsg,
                                                                                                         errorLine,
                                                                                                         errorColumn),
                             QMessageBox::Ok);
        return;
    }
    QDomElement root = _xmlPaDevList.documentElement();
    if (root.tagName() != "xml")
    {
        QMessageBox::critical(0, tr("PortAudio error" ),
                              tr("Error parsing output xml from pa devlist. Root tag is not <xml>."),
                             QMessageBox::Ok);
        return;
    }
    QDomElement devices = root.firstChildElement("devices");
    if (devices.isNull())
    {
        QMessageBox::critical(0, tr("PortAudio error" ),
                              tr("Error parsing output xml from pa devlist. There is no <devices> tag."),
                             QMessageBox::Ok);
        return;
    }

    QDomElement child = devices.firstChildElement();
    if (child.isNull())
    {
        QMessageBox::critical(0, tr("PortAudio error" ),
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
                _ui->PaIndevCombo->addItem(name,inputs.toInt());
            if (outputs.toInt() != 0)
            {
                _ui->PaOutdevCombo->addItem(name,inputs.toInt());
                _ui->PaRingdevCombo->addItem(name,inputs.toInt());
            }
        }
        child = child.nextSiblingElement();
    }

    QDomElement bindings = root.firstChildElement("bindings");
    if (bindings.isNull())
    {
        QMessageBox::critical(0, tr("PortAudio error" ),
                              tr("Error parsing output xml from pa devlist. There is no <bindings> tag."),
                             QMessageBox::Ok);
        return;
    }

    child = bindings.firstChildElement();
    if (child.isNull())
    {
        QMessageBox::critical(0, tr("PortAudio error" ),
                              tr("Error parsing output xml from pa devlist. There are no bindings."),
                             QMessageBox::Ok);
        return;
    }

    while (!child.isNull())
    {
        QString id = child.attribute("device","-1");
        if (child.tagName() == "ring")
            _ui->PaRingdevCombo->setCurrentIndex(id.toInt());
        else if (child.tagName() == "input")
            _ui->PaIndevCombo->setCurrentIndex(id.toInt());
        else if (child.tagName() == "output")
            _ui->PaOutdevCombo->setCurrentIndex(id.toInt());

        child = child.nextSiblingElement();
    }

}
