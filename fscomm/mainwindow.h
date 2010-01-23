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


#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTableWidgetItem>
#include <QSignalMapper>
#include <QSystemTrayIcon>
#include <switch.h>
#include <fshost.h>
#include <call.h>
#include <account.h>
#include "preferences/prefdialog.h"

namespace Ui {
    class MainWindow;
}

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(QWidget *parent = 0);
    ~MainWindow();

protected:
    void changeEvent(QEvent *e);

signals:
    void dtmfDialed(QString);

private slots:
    void showAbout();
    void prefTriggered();
    void coreLoadingError(QString);
    void sendDTMF(QString);
    void callTableDoubleClick(QTableWidgetItem*);
    void makeCall();
    void fshostReady();
    void paAnswer();
    void paCall(QString);
    void paHangup();
    void newOutgoingCall(QSharedPointer<Call>);
    void ringing(QSharedPointer<Call>);
    void answered(QSharedPointer<Call>);
    void hungup(QSharedPointer<Call>);
    void callFailed(QSharedPointer<Call>);
    void recordCall(bool);
    void setDefaultAccount();
    void accountAdd(QSharedPointer<Account>);
    void accountDel(QSharedPointer<Account>);
    void accountStateChanged(QSharedPointer<Account>);
    void sysTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void updateCallTimers();

private:
    Ui::MainWindow *ui;
    QSignalMapper *dialpadMapper;
    PrefDialog *preferences;
    QSystemTrayIcon *sysTray;
    QTimer *callTimer;
};

#endif // MAINWINDOW_H
