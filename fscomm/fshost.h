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
#include <QHash>
#include <QSharedPointer>
#include <switch.h>
#include "call.h"

#define FSCOMM_GW_STATE_TRYING 0
#define FSCOMM_GW_STATE_REGISTER 1
#define FSCOMM_GW_STATE_REGED 2
#define FSCOMM_GW_STATE_UNREGED 3
#define FSCOMM_GW_STATE_UNREGISTER 4
#define FSCOMM_GW_STATE_FAILED 5
#define FSCOMM_GW_STATE_FAIL_WAIT 6
#define FSCOMM_GW_STATE_EXPIRED 7
#define FSCOMM_GW_STATE_NOREG 8


static const char *fscomm_gw_state_names[] = {
    "TRYING",
    "REGISTER",
    "REGED",
    "UNREGED",
    "UNREGISTER",
    "FAILED",
    "FAIL_WAIT",
    "EXPIRED",
    "NOREG"
};

class FSHost : public QThread
{
Q_OBJECT
public:
    explicit FSHost(QObject *parent = 0);
    switch_status_t sendCmd(const char *cmd, const char *args, QString *res);
    void generalEventHandler(switch_event_t *event);
    QSharedPointer<Call> getCallByUUID(QString uuid) { return _active_calls.value(uuid); }
    QString getGwStateName(int id) { return fscomm_gw_state_names[id]; }

protected:
    void run(void);

signals:
    void coreLoadingError(QString);
    void ready(void);
    void ringing(QSharedPointer<Call>);
    void answered(QSharedPointer<Call>);
    void newOutgoingCall(QSharedPointer<Call>);
    void callFailed(QSharedPointer<Call>);
    void hungup(QSharedPointer<Call>);
    void gwStateChange(QString, int);

private:
    switch_status_t processBlegEvent(switch_event_t *, QString);
    switch_status_t processAlegEvent(switch_event_t *, QString);
    void createFolders();
    void printEventHeaders(switch_event_t *event);
    QHash<QString, QSharedPointer<Call> > _active_calls;
    QHash<QString, QString> _bleg_uuids;
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
