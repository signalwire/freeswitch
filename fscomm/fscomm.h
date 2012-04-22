#ifndef FSCOMM_H
#define FSCOMM_H

#include "account.h"
#include "isettings.h"
#include "fshost.h"
#include "accountmanager.h"

#define FSCOMM_GW_STATE_TRYING 0
#define FSCOMM_GW_STATE_REGISTER 1
#define FSCOMM_GW_STATE_REGED 2
#define FSCOMM_GW_STATE_UNREGED 3
#define FSCOMM_GW_STATE_UNREGISTER 4
#define FSCOMM_GW_STATE_FAILED 5
#define FSCOMM_GW_STATE_FAIL_WAIT 6
#define FSCOMM_GW_STATE_EXPIRED 7
#define FSCOMM_GW_STATE_NOREG 8
#define FSCOMM_GW_STATE_NOAVAIL 9


static QString fscomm_gw_state_names[] = {
    QString("Trying"),
    QString("Registering"),
    QString("Registered"),
    QString("Un-Registered"),
    QString("Un-Registering"),
    QString("Failed"),
    QString("Failed"),
    QString("Expired"),
    QString("Not applicable"),
    QString("Not available")
};

#endif // FSCOMM_H
