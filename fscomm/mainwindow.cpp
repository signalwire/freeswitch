/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2009, Anthony Minessale II <anthm@freeswitch.org>
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Joao Mesquita <jmesquita@freeswitch.org>
 *
 */

#include <QInputDialog>
#include <QMessageBox>
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <switch_version.h>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    preferences(NULL)
{
    ui->setupUi(this);

    dialpadMapper = new QSignalMapper(this);
    connect(ui->dtmf0Btn, SIGNAL(clicked()), dialpadMapper, SLOT(map()));
    connect(ui->dtmf1Btn, SIGNAL(clicked()), dialpadMapper, SLOT(map()));
    connect(ui->dtmf2Btn, SIGNAL(clicked()), dialpadMapper, SLOT(map()));
    connect(ui->dtmf3Btn, SIGNAL(clicked()), dialpadMapper, SLOT(map()));
    connect(ui->dtmf4Btn, SIGNAL(clicked()), dialpadMapper, SLOT(map()));
    connect(ui->dtmf5Btn, SIGNAL(clicked()), dialpadMapper, SLOT(map()));
    connect(ui->dtmf6Btn, SIGNAL(clicked()), dialpadMapper, SLOT(map()));
    connect(ui->dtmf7Btn, SIGNAL(clicked()), dialpadMapper, SLOT(map()));
    connect(ui->dtmf8Btn, SIGNAL(clicked()), dialpadMapper, SLOT(map()));
    connect(ui->dtmf9Btn, SIGNAL(clicked()), dialpadMapper, SLOT(map()));
    connect(ui->dtmfABtn, SIGNAL(clicked()), dialpadMapper, SLOT(map()));
    connect(ui->dtmfBBtn, SIGNAL(clicked()), dialpadMapper, SLOT(map()));
    connect(ui->dtmfCBtn, SIGNAL(clicked()), dialpadMapper, SLOT(map()));
    connect(ui->dtmfDBtn, SIGNAL(clicked()), dialpadMapper, SLOT(map()));
    connect(ui->dtmfAstBtn, SIGNAL(clicked()), dialpadMapper, SLOT(map()));
    connect(ui->dtmfPoundBtn, SIGNAL(clicked()), dialpadMapper, SLOT(map()));
    dialpadMapper->setMapping(ui->dtmf0Btn, QString("0"));
    dialpadMapper->setMapping(ui->dtmf1Btn, QString("1"));
    dialpadMapper->setMapping(ui->dtmf2Btn, QString("2"));
    dialpadMapper->setMapping(ui->dtmf3Btn, QString("3"));
    dialpadMapper->setMapping(ui->dtmf4Btn, QString("4"));
    dialpadMapper->setMapping(ui->dtmf5Btn, QString("5"));
    dialpadMapper->setMapping(ui->dtmf6Btn, QString("6"));
    dialpadMapper->setMapping(ui->dtmf7Btn, QString("7"));
    dialpadMapper->setMapping(ui->dtmf8Btn, QString("8"));
    dialpadMapper->setMapping(ui->dtmf9Btn, QString("9"));
    dialpadMapper->setMapping(ui->dtmfABtn, QString("A"));
    dialpadMapper->setMapping(ui->dtmfBBtn, QString("B"));
    dialpadMapper->setMapping(ui->dtmfCBtn, QString("C"));
    dialpadMapper->setMapping(ui->dtmfDBtn, QString("D"));
    dialpadMapper->setMapping(ui->dtmfAstBtn, QString("*"));
    dialpadMapper->setMapping(ui->dtmfPoundBtn, QString("#"));
    connect(dialpadMapper, SIGNAL(mapped(QString)), this, SLOT(dialDTMF(QString)));

    connect(&g_FSHost, SIGNAL(ready()),this, SLOT(fshostReady()));
    connect(&g_FSHost, SIGNAL(ringing(QString)), this, SLOT(ringing(QString)));
    connect(&g_FSHost, SIGNAL(answered(QString)), this, SLOT(answered(QString)));
    connect(&g_FSHost, SIGNAL(hungup(Call*)), this, SLOT(hungup(Call*)));
    connect(&g_FSHost, SIGNAL(newOutgoingCall(QString)), this, SLOT(newOutgoingCall(QString)));
    connect(&g_FSHost, SIGNAL(gwStateChange(QString,int)), this, SLOT(gwStateChanged(QString,int)));
    /*connect(&g_FSHost, SIGNAL(coreLoadingError(QString)), this, SLOT(coreLoadingError(QString)));*/

    connect(ui->newCallBtn, SIGNAL(clicked()), this, SLOT(makeCall()));
    connect(ui->answerBtn, SIGNAL(clicked()), this, SLOT(paAnswer()));
    connect(ui->hangupBtn, SIGNAL(clicked()), this, SLOT(paHangup()));
    connect(ui->listCalls, SIGNAL(itemDoubleClicked(QListWidgetItem*)), this, SLOT(callListDoubleClick(QListWidgetItem*)));
    connect(ui->action_Preferences, SIGNAL(triggered()), this, SLOT(prefTriggered()));
    connect(ui->action_Exit, SIGNAL(triggered()), this, SLOT(close()));
    connect(ui->actionAbout, SIGNAL(triggered()), this, SLOT(showAbout()));
}

MainWindow::~MainWindow()
{
    delete ui;
    QString res;
    g_FSHost.sendCmd("fsctl", "shutdown", &res);
    g_FSHost.wait();
}

void MainWindow::prefTriggered()
{
    if (!preferences)
        preferences = new PrefDialog();

    preferences->raise();
    preferences->show();
    preferences->activateWindow();
}

void MainWindow::coreLoadingError(QString err)
{
    QMessageBox::warning(this, "Error Loading Core...", err, QMessageBox::Ok);
    QApplication::exit(255);
}

void MainWindow::gwStateChanged(QString gw, int state)
{
    ui->statusBar->showMessage(tr("Account %1 is %2").arg(gw, g_FSHost.getGwStateName(state)));

    /* TODO: This should be placed somewhere else when the config handler is here... */
    QList<QTableWidgetItem *> match = ui->tableAccounts->findItems(gw, Qt::MatchExactly);
    if (match.isEmpty())
    {
        /* Create the damn thing */
        ui->tableAccounts->setRowCount(ui->tableAccounts->rowCount()+1);
        QTableWidgetItem *gwField = new QTableWidgetItem(gw);
        QTableWidgetItem *stField = new QTableWidgetItem(g_FSHost.getGwStateName(state));
        ui->tableAccounts->setItem(0,0,gwField);
        ui->tableAccounts->setItem(0,1,stField);
        ui->tableAccounts->resizeColumnsToContents();
        return;
    }

    QTableWidgetItem *gwField = match.at(0);
    QTableWidgetItem *stField = ui->tableAccounts->item(gwField->row(),1);
    stField->setText(g_FSHost.getGwStateName(state));
    ui->tableAccounts->resizeColumnsToContents();

}

void MainWindow::dialDTMF(QString dtmf)
{
    QString result;
    QString dtmf_string = QString("dtmf %1").arg(dtmf);
    if (g_FSHost.sendCmd("pa", dtmf_string.toAscii(), &result) == SWITCH_STATUS_FALSE) {
        ui->textEdit->setText("Error sending that command");
    }
}

void MainWindow::callListDoubleClick(QListWidgetItem *item)
{
    Call *call = g_FSHost.getCallByUUID(item->data(Qt::UserRole).toString());
    QString switch_str = QString("switch %1").arg(call->getCallID());
    QString result;
    if (g_FSHost.sendCmd("pa", switch_str.toAscii(), &result) == SWITCH_STATUS_FALSE) {
        ui->textEdit->setText(QString("Error switching to call %1").arg(call->getCallID()));
        return;
    }
    ui->hangupBtn->setEnabled(true);
}

void MainWindow::makeCall()
{
    bool ok;
    QString dialstring = QInputDialog::getText(this, tr("Make new call"),
                                         tr("Number to dial:"), QLineEdit::Normal, NULL,&ok);

    if (ok && !dialstring.isEmpty())
    {
        paCall(dialstring);
    }
}

void MainWindow::fshostReady()
{
    ui->statusBar->showMessage("Ready");
    ui->newCallBtn->setEnabled(true);
    ui->textEdit->setEnabled(true);
    ui->textEdit->setText("Ready to dial and receive calls!");
}

void MainWindow::paAnswer()
{
    QString result;
    if (g_FSHost.sendCmd("pa", "answer", &result) == SWITCH_STATUS_FALSE) {
        ui->textEdit->setText("Error sending that command");
    }

    ui->textEdit->setText("Talking...");
    ui->hangupBtn->setEnabled(true);
    ui->answerBtn->setEnabled(false);
}

void MainWindow::paCall(QString dialstring)
{
    QString result;

    QString callstring = QString("call %1").arg(dialstring);

    if (g_FSHost.sendCmd("pa", callstring.toAscii(), &result) == SWITCH_STATUS_FALSE) {
        ui->textEdit->setText("Error sending that command");
    }

    ui->hangupBtn->setEnabled(true);
}

void MainWindow::paHangup()
{
    QString result;
    if (g_FSHost.sendCmd("pa", "hangup", &result) == SWITCH_STATUS_FALSE) {
        ui->textEdit->setText("Error sending that command");
    }

    ui->textEdit->setText("Click to dial number...");
    ui->statusBar->showMessage("Call hungup");
    ui->hangupBtn->setEnabled(false);
}

void MainWindow::newOutgoingCall(QString uuid)
{
    Call *call = g_FSHost.getCallByUUID(uuid);
    ui->textEdit->setText(QString("Calling %1 (%2)").arg(call->getCidName(), call->getCidNumber()));
    QListWidgetItem *item = new QListWidgetItem(tr("%1 (%2) - Calling").arg(call->getCidName(), call->getCidNumber()));
    item->setData(Qt::UserRole, uuid);
    ui->listCalls->addItem(item);
    ui->hangupBtn->setEnabled(true);
}

void MainWindow::ringing(QString uuid)
{

    Call *call = g_FSHost.getCallByUUID(uuid);
    for (int i=0; i<ui->listCalls->count(); i++)
    {
        QListWidgetItem *item = ui->listCalls->item(i);
        if (item->data(Qt::UserRole).toString() == uuid)
        {
            item->setText(tr("%1 - Ringing").arg(call->getCidNumber()));
            ui->textEdit->setText(QString("Call from %1 (%2)").arg(call->getCidName(), call->getCidNumber()));
            return;
        }
    }

    ui->textEdit->setText(QString("Call from %1 (%2)").arg(call->getCidName(), call->getCidNumber()));
    QListWidgetItem *item = new QListWidgetItem(tr("%1 (%2) - Ringing").arg(call->getCidName(), call->getCidNumber()));
    item->setData(Qt::UserRole, uuid);
    ui->listCalls->addItem(item);
    ui->answerBtn->setEnabled(true);
}

void MainWindow::answered(QString uuid)
{
    Call *call = g_FSHost.getCallByUUID(uuid);
    for (int i=0; i<ui->listCalls->count(); i++)
    {
        QListWidgetItem *item = ui->listCalls->item(i);
        if (item->data(Qt::UserRole).toString() == uuid)
        {
            if (call->getDirection() == FSCOMM_CALL_DIRECTION_INBOUND)
            {
                item->setText(tr("%1 (%2) - Active").arg(call->getCidName(), call->getCidNumber()));
                break;
            }
            else
            {
                item->setText(tr("%1 - Active").arg(call->getCidNumber()));
                break;
            }
        }
    }
    ui->dtmf0Btn->setEnabled(true);
    ui->dtmf1Btn->setEnabled(true);
    ui->dtmf2Btn->setEnabled(true);
    ui->dtmf3Btn->setEnabled(true);
    ui->dtmf4Btn->setEnabled(true);
    ui->dtmf5Btn->setEnabled(true);
    ui->dtmf6Btn->setEnabled(true);
    ui->dtmf7Btn->setEnabled(true);
    ui->dtmf8Btn->setEnabled(true);
    ui->dtmf9Btn->setEnabled(true);
    ui->dtmfABtn->setEnabled(true);
    ui->dtmfBBtn->setEnabled(true);
    ui->dtmfCBtn->setEnabled(true);
    ui->dtmfDBtn->setEnabled(true);
    ui->dtmfAstBtn->setEnabled(true);
    ui->dtmfPoundBtn->setEnabled(true);
}

void MainWindow::hungup(Call* call)
{
    for (int i=0; i<ui->listCalls->count(); i++)
    {
        QListWidgetItem *item = ui->listCalls->item(i);
        if (item->data(Qt::UserRole).toString() == call->getUUID())
        {
            delete ui->listCalls->takeItem(i);
            break;
        }
    }
    ui->textEdit->setText(tr("Call with %1 (%2) hungup.").arg(call->getCidName(), call->getCidNumber()));
    /* TODO: Will cause problems if 2 calls are received at the same time */
    ui->answerBtn->setEnabled(false);
    ui->hangupBtn->setEnabled(false);
    ui->dtmf0Btn->setEnabled(false);
    ui->dtmf1Btn->setEnabled(false);
    ui->dtmf2Btn->setEnabled(false);
    ui->dtmf3Btn->setEnabled(false);
    ui->dtmf4Btn->setEnabled(false);
    ui->dtmf5Btn->setEnabled(false);
    ui->dtmf6Btn->setEnabled(false);
    ui->dtmf7Btn->setEnabled(false);
    ui->dtmf8Btn->setEnabled(false);
    ui->dtmf9Btn->setEnabled(false);
    ui->dtmfABtn->setEnabled(false);
    ui->dtmfBBtn->setEnabled(false);
    ui->dtmfCBtn->setEnabled(false);
    ui->dtmfDBtn->setEnabled(false);
    ui->dtmfAstBtn->setEnabled(false);
    ui->dtmfPoundBtn->setEnabled(false);
    delete call;
}

void MainWindow::changeEvent(QEvent *e)
{
    QMainWindow::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        break;
    default:
        break;
    }
}

void MainWindow::showAbout()
{
    QString result;
    g_FSHost.sendCmd("version", "", &result);

    QMessageBox::about(this, tr("About FSComm"),
                       tr("<h2>FSComm</h2>"
                          "<p>Author: Jo&atilde;o Mesquita &lt;jmesquita@freeswitch.org>"
                          "<p>FsComm is a softphone based on libfreeswitch."
                          "<p>The FreeSWITCH&trade; images and name are trademark of"
                          " Anthony Minessale II, primary author of FreeSWITCH&trade;."
                          "<p>Compiled FSComm version: %1"
                          "<p>%2").arg(SWITCH_VERSION_FULL, result));
}
