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

    QString cid_name = _settings->value("cid-name").toString();
    QString ncid_name = _ui->PaCallerIdNameEdit->text();

    QString cid_num = _settings->value("cid-num").toString();
    QString ncid_num = _ui->PaCallerIdNumEdit->text();

    QString hold_file = _settings->value("hold-file").toString();
    QString nhold_file =  _ui->PaHoldFileEdit->text();

    QString ring_file = _settings->value("ring-file").toString();
    QString nring_file = _ui->PaRingFileEdit->text();

    int ring_interval = _settings->value("ring-interval").toInt();
    int nring_interval = _ui->PaRingIntervalSpin->value();

    QString result;

    if (cid_name != ncid_name ||
        cid_num != ncid_num ||
        hold_file != nhold_file ||
        ring_file != nring_file ||
        ring_interval != nring_interval)
    {
        if (g_FSHost.sendCmd("reload", "mod_portaudio", &result) == SWITCH_STATUS_SUCCESS)
        {
            _settings->setValue("cid-name", ncid_name);
            _settings->setValue("cid-num", ncid_num);
            _settings->setValue("ring-file", nring_file);
            _settings->setValue("ring-interval", nring_interval);
            _settings->setValue("hold-file", nhold_file);
        }
        else
        {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error while issuing reload command to mod_portaudio!\n");
            QMessageBox::critical(0, tr("Unable to save settings"),
                                  tr("There was an error saving your settings.\nPlease report this bug."),
                                  QMessageBox::Ok);
        }
    }

    int nindev = _ui->PaIndevCombo->currentIndex();
    int indev = _settings->value("indev").toInt();
    int noutdev = _ui->PaOutdevCombo->currentIndex();
    int outdev = _settings->value("outdev").toInt();
    int nringdev = _ui->PaRingdevCombo->currentIndex();
    int ringdev = _settings->value("ringdev").toInt();

    if (nindev != indev)
    {
        if (g_FSHost.sendCmd("pa", QString("indev #%1").arg(nindev).toAscii().constData(), &result) == SWITCH_STATUS_SUCCESS)
        {
            _settings->setValue("indev", nindev);
        }
        else
        {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error setting indev from #%d to #%d on mod_portaudio!\n",
                              indev, nindev);
            QMessageBox::critical(0, tr("Unable to save settings"),
                                  tr("There was an error changing the indev.\nPlease report this bug."),
                                  QMessageBox::Ok);
        }
    }

    if (noutdev!= outdev)
    {
        if (g_FSHost.sendCmd("pa", QString("outdev #%1").arg(noutdev).toAscii().constData(), &result) == SWITCH_STATUS_SUCCESS)
        {
            _settings->setValue("outdev", noutdev);
        }
        else
        {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error setting outdev from #%d to #%d on mod_portaudio!\n",
                              outdev, noutdev);
            QMessageBox::critical(0, tr("Unable to save settings"),
                                  tr("There was an error changing the outdev.\nPlease report this bug."),
                                  QMessageBox::Ok);
        }
    }
    if (nringdev != ringdev)
    {
        if (g_FSHost.sendCmd("pa", QString("ringdev #%1").arg(nringdev).toAscii().constData(), &result) == SWITCH_STATUS_SUCCESS)
        {
            _settings->setValue("ringdev", nringdev);
        }
        else
        {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error setting ringdev from #%d to #%d on mod_portaudio!\n",
                              nringdev, nringdev);
            QMessageBox::critical(0, tr("Unable to save settings"),
                                  tr("There was an error changing the ringdev.\nPlease report this bug."),
                                  QMessageBox::Ok);
        }
    }

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
