#include <QtGui>
#include "prefportaudio.h"

PrefPortaudio::PrefPortaudio(Ui::PrefDialog *ui, QObject *parent) :
        QObject(parent),
        _ui(ui)
{
    connect(_ui->PaRingFileBtn, SIGNAL(clicked()), this, SLOT(ringFileChoose()));
    connect(_ui->PaHoldFileBtn, SIGNAL(clicked()), this, SLOT(holdFileChoose()));
    connect(_ui->PaIndevCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(indevChangeDev(int)));
    connect(_ui->PaOutdevCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(outdevChangeDev(int)));
    connect(_ui->PaRingdevCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(ringdevChangeDev(int)));
    connect(_ui->PaRingdevTestBtn, SIGNAL(clicked()), this, SLOT(ringdevTest()));
    connect(_ui->PaLoopTestBtn, SIGNAL(clicked()), this, SLOT(loopTest()));
    connect(_ui->PaRefreshDevListBtn, SIGNAL(clicked()), this, SLOT(refreshDevList()));
    connect(_ui->btnApplyPreprocessor, SIGNAL(toggled(bool)), this, SLOT(applyPreprocessors(bool)));
}

void PrefPortaudio::applyPreprocessors(bool state)
{
    QStringList cmds;
    if (!state)
    {
        cmds.append("stop");
    }
    else
    {
        if (_ui->checkAECRead->isChecked()) cmds.append(QString("recho_cancel=%1").arg(_ui->spinAECTail->value()));
        if (_ui->checkAECWrite->isChecked()) cmds.append(QString("wecho_cancel=%1").arg(_ui->spinAECTail->value()));
        if (_ui->checkESRead->isChecked()) cmds.append(QString("recho_suppress=%1").arg(_ui->spinESDb->value()));
        if (_ui->checkESWrite->isChecked()) cmds.append(QString("wecho_suppress=%1").arg(_ui->spinESDb->value()));
        if (_ui->checkNSRead->isChecked()) cmds.append(QString("rnoise_suppress=%1").arg(_ui->spinNSDb->value()));
        if (_ui->checkNSWrite->isChecked()) cmds.append(QString("wnoise_suppress=%1").arg(_ui->spinNSDb->value()));
        if (_ui->checkAGCRead->isChecked()) cmds.append(QString("ragc=%1").arg(_ui->spinAGC->value()));
        if (_ui->checkAGCWrite->isChecked()) cmds.append(QString("wagc=%1").arg(_ui->spinAGC->value()));
    }
    emit preprocessorsApplied(cmds);
}

void PrefPortaudio::ringdevTest()
{
    QString result;
    if (g_FSHost->sendCmd("pa", QString("play %1/.fscomm/sounds/test.wav 0").arg(QDir::homePath()).toAscii().constData(), &result) != SWITCH_STATUS_SUCCESS)
    {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error testing ringdev on mod_portaudio! %s\n",
                          result.toAscii().constData());
    }
}

void PrefPortaudio::loopTest()
{
    QString result;
    _ui->PaLoopTestBtn->setEnabled(false);
    if (g_FSHost->sendCmd("pa", "looptest", &result) != SWITCH_STATUS_SUCCESS)
    {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error running looptest on mod_portaudio! %s\n",
                          result.toAscii().constData());
    }
    _ui->PaLoopTestBtn->setEnabled(true);
}

void PrefPortaudio::refreshDevList()
{
    QString result;
    if (g_FSHost->sendCmd("pa", "rescan", &result) != SWITCH_STATUS_SUCCESS)
    {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error rescaning sound device on mod_portaudio! %s\n",
                          result.toAscii().constData());
    }
    //clear combox
    _ui->PaIndevCombo->clear();
    _ui->PaOutdevCombo->clear();
    _ui->PaRingdevCombo->clear();
    getPaDevlist();
}

void PrefPortaudio::indevChangeDev(int index)
{
    QString result;
    int dev = _ui->PaIndevCombo->itemData(index, Qt::UserRole).toInt();
    if (g_FSHost->sendCmd("pa", QString("indev #%1").arg(dev).toAscii().constData(), &result) != SWITCH_STATUS_SUCCESS)
    {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error setting ringdev to #%d on mod_portaudio!\n", dev);
        QMessageBox::critical(0, tr("Unable to change device."),
                              tr("There was an error changing the ringdev.\nPlease report this bug."),
                              QMessageBox::Ok);

    }
}

void PrefPortaudio::ringdevChangeDev(int index)
{
    QString result;
    int dev = _ui->PaRingdevCombo->itemData(index, Qt::UserRole).toInt();
    if (g_FSHost->sendCmd("pa", QString("ringdev #%1").arg(dev).toAscii().constData(), &result) != SWITCH_STATUS_SUCCESS)
    {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error setting ringdev to #%d on mod_portaudio!\n", dev);
        QMessageBox::critical(0, tr("Unable to change device."),
                              tr("There was an error changing the ringdev.\nPlease report this bug."),
                              QMessageBox::Ok);

    }
}

void PrefPortaudio::outdevChangeDev(int index)
{
    QString result;
    int dev = _ui->PaRingdevCombo->itemData(index, Qt::UserRole).toInt();
    if (g_FSHost->sendCmd("pa", QString("outdev #%1").arg(dev).toAscii().constData(), &result) != SWITCH_STATUS_SUCCESS)
    {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error setting outdev to #%d on mod_portaudio!\n", dev);
        QMessageBox::critical(0, tr("Unable to change device."),
                              tr("There was an error changing the outdev.\nPlease report this bug."),
                              QMessageBox::Ok);
    }
}

void PrefPortaudio::holdFileChoose()
{
    QString fileName = QFileDialog::getOpenFileName(0,
                                                    tr("Select file"),
                                                    QDir::homePath(),
                                                    NULL);
    if (!fileName.isNull())
        _ui->PaHoldFileEdit->setText(fileName);
}

void PrefPortaudio::ringFileChoose()
{
    QString fileName = QFileDialog::getOpenFileName(0,
                                                    tr("Select file"),
                                                    QDir::homePath(),
                                                    NULL);
    if (!fileName.isNull())
        _ui->PaRingFileEdit->setText(fileName);
}

void PrefPortaudio::writeConfig()
{
    /* We can do better error control here, can't we? */
    ISettings *_settings = new ISettings();
    QDomElement cfg = _settings->getConfigNode("portaudio.conf");
    QDomNodeList nl = cfg.elementsByTagName("param");
    for (int i = 0; i < nl.count(); i++) {
        QDomAttr var = nl.at(i).toElement().attributeNode("name");
        QDomAttr val = nl.at(i).toElement().attributeNode("value");
        if (var.value() == "indev") {
            val.setValue(QString::number(_ui->PaIndevCombo->itemData(_ui->PaIndevCombo->currentIndex(), Qt::UserRole).toInt()));
        }
        if (var.value() == "outdev") {
            val.setValue(QString::number(_ui->PaOutdevCombo->itemData(_ui->PaOutdevCombo->currentIndex(), Qt::UserRole).toInt()));
        }
        if (var.value() == "ringdev") {
            val.setValue(QString::number(_ui->PaRingdevCombo->itemData(_ui->PaRingdevCombo->currentIndex(), Qt::UserRole).toInt()));
        }
        if (var.value() == "ring-file") {
            val.setValue(_ui->PaRingFileEdit->text());
        }
        if (var.value() == "ring-interval") {
            val.setValue(QString::number(_ui->PaRingIntervalSpin->value()));
        }
        if (var.value() == "hold-file") {
            val.setValue(_ui->PaHoldFileEdit->text());
        }
        if (var.value() == "cid-name") {
            val.setValue(_ui->PaCallerIdNameEdit->text());
        }
        if (var.value() == "cid-num") {
            val.setValue(_ui->PaCallerIdNumEdit->text());
        }
        if (var.value() == "sample-rate") {
            val.setValue(_ui->PaSampleRateEdit->text());
        }
        if (var.value() == "codec-ms") {
            val.setValue(_ui->PaCodecMSEdit->text());
        }
        /* Not used currently
        if (var.value() == "dialplan") {
            val.setValue();
        }
        if (var.value() == "timer-name") {
            val.setValue();
        }*/
    }
    /* Save the config to the file */
    _settings->setConfigNode(cfg, "portaudio.conf");
}

void PrefPortaudio::postWriteConfig() {
    QString result;
    if (g_FSHost->sendCmd("reload", "mod_portaudio", &result) != SWITCH_STATUS_SUCCESS)
    {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error while issuing reload command to mod_portaudio!\n");
        QMessageBox::critical(0, tr("Unable to save settings"),
                              tr("There was an error saving your settings.\nPlease report this bug."),
                              QMessageBox::Ok);
    }
}

void PrefPortaudio::readConfig()
{
    getPaDevlist(); /* To populate the combo */

    ISettings *_settings = new ISettings();
    QDomElement cfg = _settings->getConfigNode("portaudio.conf");
    QDomNodeList nl = cfg.elementsByTagName("param");
    for (int i = 0; i < nl.count(); i++) {
        QDomAttr var = nl.at(i).toElement().attributeNode("name");
        QDomAttr val = nl.at(i).toElement().attributeNode("value");
        /* Set when getting the device list */
        if (var.value() == "indev") {
        }
        if (var.value() == "outdev") {
        }
        if (var.value() == "ringdev") {
        }
        if (var.value() == "ring-file") {
            _ui->PaRingFileEdit->setText(val.value());
        }
        if (var.value() == "ring-interval") {
            _ui->PaRingIntervalSpin->setValue(val.value().toInt());
        }
        if (var.value() == "hold-file") {
            _ui->PaHoldFileEdit->setText(val.value());
        }
        /* Not yet used.
        if (var.value() == "dialplan") {
        }
        if (var.value() == "timer-name") {
        }
        */
        if (var.value() == "cid-name") {
            _ui->PaCallerIdNameEdit->setText(val.value());
        }
        if (var.value() == "cid-num") {
            _ui->PaCallerIdNumEdit->setText(val.value());
        }
        if (var.value() == "sample-rate") {
            _ui->PaSampleRateEdit->setText(val.value());
        }
        if (var.value() == "codec-ms") {
            _ui->PaCodecMSEdit->setText(val.value());
        }
    }
}

void PrefPortaudio::getPaDevlist()
{
    QString result;
    int errorLine, errorColumn;
    QString errorMsg;

    if (g_FSHost->sendCmd("pa", "devlist xml", &result) != SWITCH_STATUS_SUCCESS)
    {
        QMessageBox::critical(0, tr("PortAudio error" ),
                              tr("Error querying audio devices."),
                              QMessageBox::Ok);
        return;
    }
    _ui->PaOutdevCombo->clear();
    _ui->PaIndevCombo->clear();
    _ui->PaRingdevCombo->clear();

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
                _ui->PaIndevCombo->addItem(name,id.toInt());
            if (outputs.toInt() != 0)
            {
                _ui->PaOutdevCombo->addItem(name,id.toInt());
                _ui->PaRingdevCombo->addItem(name,id.toInt());
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
        {
            for(int itemId=0; itemId<_ui->PaRingdevCombo->count(); itemId++)
            {
                if (id == _ui->PaRingdevCombo->itemData(itemId,Qt::UserRole))
                {
                    //setCurrentIndex triggers currentIndexChanged signal, hmmm...
                    _ui->PaRingdevCombo->setCurrentIndex(itemId);
                    break;
                }
            }
        }
        else if (child.tagName() == "input")
        {
            for(int itemId=0; itemId<_ui->PaRingdevCombo->count(); itemId++)
            {
                if (id == _ui->PaIndevCombo->itemData(itemId,Qt::UserRole))
                {
                    _ui->PaIndevCombo->setCurrentIndex(itemId);
                    break;
                }
            }
        }
        else if (child.tagName() == "output")
            for(int itemId=0; itemId<_ui->PaOutdevCombo->count(); itemId++)
            {
                if (id == _ui->PaOutdevCombo->itemData(itemId,Qt::UserRole))
                {
                    _ui->PaOutdevCombo->setCurrentIndex(itemId);
                    break;
                }
            }

        child = child.nextSiblingElement();
    }

}
