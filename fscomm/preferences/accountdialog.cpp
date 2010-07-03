#include <QtGui>
#include "accountdialog.h"
#include "ui_accountdialog.h"

AccountDialog::AccountDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AccountDialog)
{
    ui->setupUi(this);
    connect(this, SIGNAL(accepted()), this, SLOT(writeConfig()));
    connect(ui->sofiaExtraParamAddBtn, SIGNAL(clicked()), this, SLOT(addExtraParam()));
    connect(ui->sofiaExtraParamRemBtn, SIGNAL(clicked()), this, SLOT(remExtraParam()));
    connect(ui->clidSettingsCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(clidSettingsComboChanged(int)));
    connect(ui->codecSettingsCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(codecSettingsComboChanged(int)));

    ui->sofiaExtraParamTable->horizontalHeader()->setStretchLastSection(true);
    ui->tabWidget->removeTab(ui->tabWidget->indexOf(ui->codecPage));
    ui->tabWidget->removeTab(ui->tabWidget->indexOf(ui->clidPage));
}

AccountDialog::~AccountDialog()
{
    delete ui;
}

void AccountDialog::codecSettingsComboChanged(int index)
{
    if (index == 0)
        ui->tabWidget->removeTab(ui->tabWidget->indexOf(ui->codecPage));
    else
        ui->tabWidget->insertTab(1,ui->codecPage,tr("Codecs"));

}

void AccountDialog::clidSettingsComboChanged(int index)
{
    if (index == 0)
        ui->tabWidget->removeTab(ui->tabWidget->indexOf(ui->clidPage));
    else
        ui->tabWidget->insertTab(1,ui->clidPage,tr("Caller ID"));
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
    ui->sofiaExtraParamTable->resizeColumnsToContents();
    ui->sofiaExtraParamTable->resizeRowsToContents();
    ui->sofiaExtraParamTable->horizontalHeader()->setStretchLastSection(true);
}

/* TODO: We need to figure out the callerID thing... */
void AccountDialog::readConfig()
{

    /* We already know the name of the gateway, so... */
    ui->sofiaGwNameEdit->setText(_name);

    ISettings settings(this);
    QDomElement cfg = settings.getConfigNode("sofia.conf");

    QDomNodeList nl = cfg.elementsByTagName("gateway");

    for (int i = 0; i < nl.count(); i++) {
        QDomElement gw = nl.at(i).toElement();
        if (gw.attributeNode("name").value() == _name) {
            /* Iterate the params and set the values */
            QDomNodeList params = gw.elementsByTagName("param");
            int row = 0; /* Used for extra params */
            ui->sofiaExtraParamTable->clearContents();
            for (int j = 0; j < params.count(); j++) {
                QDomElement param = params.at(j).toElement();
                QString var = param.attributeNode("name").value();
                QString val = param.attributeNode("value").value();
                if ( var == "username" ) {
                    ui->sofiaGwUsernameEdit->setText(val);
                } else if ( var == "realm" ) {
                    ui->sofiaGwRealmEdit->setText(val);
                } else if ( var == "password" ) {
                    ui->sofiaGwPasswordEdit->setText(val);
                } else if ( var == "expire-seconds" ) {
                    ui->sofiaGwExpireSecondsSpin->setValue(val.toInt());
                } else if ( var == "register" ) {
                    ui->sofiaGwRegisterCombo->setCurrentIndex(ui->sofiaGwRegisterCombo->findText(val, Qt::MatchExactly));
                } else if ( var == "register-transport" ) {
                    ui->sofiaGwRegisterTransportCombo->setCurrentIndex(ui->sofiaGwRegisterTransportCombo->findText(val, Qt::MatchExactly));
                } else if ( var == "retry-seconds" ) {
                    ui->sofiaGwRetrySecondsSpin->setValue(val.toInt());
                } else {
                    /* Set custom parameters */
                    row++;
                    ui->sofiaExtraParamTable->setRowCount(row);
                    QTableWidgetItem *varName = new QTableWidgetItem(var);
                    QTableWidgetItem *varVal = new QTableWidgetItem(val);
                    ui->sofiaExtraParamTable->setItem(row-1, 0,varName);
                    ui->sofiaExtraParamTable->setItem(row-1, 1,varVal);
                }
            }
            /* Stop processing the gateway list */
            break;
        }
    }

    ui->sofiaExtraParamTable->resizeColumnsToContents();
    ui->sofiaExtraParamTable->resizeRowsToContents();
    ui->sofiaExtraParamTable->horizontalHeader()->setStretchLastSection(true);
}

/* TODO: Figure out the callerID thing... */
void AccountDialog::writeConfig()
{
    /* TODO: This is where we need to figure out the caller ID
    if (ui->clidSettingsCombo->currentIndex() == 0)
    {
        settings->remove("caller_id_name");
        settings->remove("caller_id_num");
    } else {
        settings->setValue("caller_id_name", ui->sofiaCallerIDName->text());
        settings->setValue("caller_id_num", ui->sofiaCallerIDNum->text());
    }
    */
    ISettings settings(this);
    QDomElement cfg = settings.getConfigNode("sofia.conf");

    /* First check to see if we are editing */
    if (!_name.isEmpty()) {

        /* Find our gateway */
        QDomElement gw;
        QDomNodeList gws = cfg.elementsByTagName("gateway");
        for (int i = 0; i < gws.count(); i++) {
            if ( gws.at(i).toElement().attributeNode("name").value() == _name) {
                gw = gws.at(i).toElement();
                /* Set the new gateway name */
                if ( _name != ui->sofiaGwNameEdit->text() ) {
                    _name = ui->sofiaGwNameEdit->text();
                    gws.at(i).toElement().attributeNode("name").setValue(ui->sofiaGwNameEdit->text());
                }
                break;
            }
        }
        if ( gw.isNull() ) {
            qDebug() << "Hey, there is no gateway!";
            return;
        }

        /* Found the gateway, now iterate the parameters */
        QDomNodeList params = gw.elementsByTagName("param");
        for (int i = 0; i < params.count(); i++) {
            QDomElement param = params.at(i).toElement();
            QString var = param.attributeNode("name").value();
            QDomAttr val = param.attributeNode("value");
            if ( var == "username" ) {
                val.setValue(ui->sofiaGwUsernameEdit->text());
            } else if ( var == "realm" ) {
                val.setValue(ui->sofiaGwRealmEdit->text());
            } else if ( var == "password" ) {
                val.setValue(ui->sofiaGwPasswordEdit->text());
            } else if ( var == "expire-seconds" ) {
                val.setValue(QString::number(ui->sofiaGwExpireSecondsSpin->value()));
            } else if ( var == "register" ) {
                val.setValue(ui->sofiaGwRegisterCombo->currentText());
            } else if ( var == "register-transport" ) {
                val.setValue(ui->sofiaGwRegisterTransportCombo->currentText());
            } else if ( var == "retry-seconds" ) {
                val.setValue(QString::number(ui->sofiaGwRetrySecondsSpin->value()));
            }
        }
        /* Set extra parameters */
        QDomDocument d = gw.toDocument();
        for (int i = 0; i< ui->sofiaExtraParamTable->rowCount(); i++)
        {
            QDomElement ePar = d.createElement("param");
            QDomAttr var = d.createAttribute(ui->sofiaExtraParamTable->item(i, 0)->text());
            ePar.appendChild(var);
            QDomAttr val = d.createAttribute(ui->sofiaExtraParamTable->item(i, 1)->text());
            ePar.appendChild(val);
            gw.appendChild(ePar);
        }
    } else {
        QDomElement gws = cfg.elementsByTagName("gateways").at(0).toElement();
        QDomDocument d = gws.toDocument();
        QDomElement nGw = d.createElement("gateway");
        gws.insertAfter(nGw, QDomNode());
        nGw.setAttribute("name",ui->sofiaGwNameEdit->text());

        /* Set each one of the parameters */
        setParam(nGw, "username", ui->sofiaGwUsernameEdit->text());
        setParam(nGw, "password", ui->sofiaGwPasswordEdit->text());
        setParam(nGw, "register", ui->sofiaGwRegisterCombo->currentText());
        setParam(nGw, "realm", ui->sofiaGwRealmEdit->text());
        setParam(nGw, "expire-seconds", QString::number(ui->sofiaGwExpireSecondsSpin->value()));
        setParam(nGw, "register-transport", ui->sofiaGwRegisterTransportCombo->currentText());
        setParam(nGw, "retry-seconds", QString::number(ui->sofiaGwRetrySecondsSpin->value()));
        for (int i = 0; i< ui->sofiaExtraParamTable->rowCount(); i++)
        {
            setParam(nGw, ui->sofiaExtraParamTable->item(i, 0)->text(), ui->sofiaExtraParamTable->item(i, 1)->text());
        }
    }

    settings.setConfigNode(cfg, "sofia.conf");
    emit gwAdded(_name);
}

void AccountDialog::setParam(QDomElement &parent, QString name, QString value) {
    QDomDocument d = parent.toDocument();
    QDomElement e = d.createElement("param");
    e.setAttribute("name", name);
    e.setAttribute("value", value);
    parent.appendChild(e);
}

void AccountDialog::clear()
{
    ui->sofiaExtraParamTable->clearContents();
    ui->sofiaExtraParamTable->setRowCount(0);

    ui->sofiaGwNameEdit->clear();
    ui->sofiaGwUsernameEdit->clear();
    ui->sofiaGwRealmEdit->clear();
    ui->sofiaGwPasswordEdit->clear();
    ui->sofiaGwExpireSecondsSpin->setValue(60);
    ui->sofiaGwRegisterCombo->setCurrentIndex(0);
    ui->sofiaGwRegisterTransportCombo->setCurrentIndex(0);
    ui->sofiaGwRetrySecondsSpin->setValue(30);
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
