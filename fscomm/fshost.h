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
#ifndef FSHOST_H
#define FSHOST_H

#include <QThread>
#include <QColor>
#include <QHash>
#include <QSharedPointer>
#include <switch.h>
#include "call.h"
#include "account.h"

class FSHost : public QThread
{
Q_OBJECT
public:
    explicit FSHost(QObject *parent = 0);
    switch_status_t sendCmd(const char *cmd, const char *args, QString *res);
    void generalEventHandler(switch_event_t *event);
    QSharedPointer<Call> getCallByUUID(QString uuid) { return _active_calls.value(uuid); }
    QSharedPointer<Call> getCurrentActiveCall();
    QList<QSharedPointer<Account> > getAccounts() { return _accounts.values(); }
    QSharedPointer<Account> getAccountByUUID(QString uuid);
    QSharedPointer<Account> getCurrentDefaultAccount();
    QSharedPointer<Account> getAccountByName(QString accStr);
    void accountReloadCmd(QSharedPointer<Account> acc);
    QBool isModuleLoaded(QString);

protected:
    void run(void);

signals:
    void coreLoadingError(QString);
    void loadingModules(QString, int, QColor);
    void loadedModule(QString, QString, QString);
    void ready(void);
    void ringing(QSharedPointer<Call>);
    void answered(QSharedPointer<Call>);
    void newOutgoingCall(QSharedPointer<Call>);
    void callFailed(QSharedPointer<Call>);
    void hungup(QSharedPointer<Call>);
    void accountStateChange(QSharedPointer<Account>);
    void newAccount(QSharedPointer<Account>);
    void delAccount(QSharedPointer<Account>);

private slots:
    /* We need to wait for the gateway deletion before reloading it */
    void accountReloadSlot(QSharedPointer<Account>);
    void minimalModuleLoaded(QString, QString, QString);

private:
    switch_status_t processBlegEvent(switch_event_t *, QString);
    switch_status_t processAlegEvent(switch_event_t *, QString);
    void createFolders();
    void printEventHeaders(switch_event_t *event);
    QHash<QString, QSharedPointer<Call> > _active_calls;
    QHash<QString, QSharedPointer<Account> > _accounts;
    QHash<QString, QString> _bleg_uuids;
    QList<QString> _reloading_Accounts;
    QList<QString> _loadedModules;
};

extern FSHost g_FSHost;

/*
   Used to match callback from fs core. We dup the event and call the class
   method callback to make use of the signal/slot infrastructure.
*/
static void eventHandlerCallback(switch_event_t *event)
{
    switch_event_t *clone = NULL;
    if (switch_event_dup(&clone, event) == SWITCH_STATUS_SUCCESS) {
        g_FSHost.generalEventHandler(clone);
    }
    switch_safe_free(clone);
}

#endif // FSHOST_H
