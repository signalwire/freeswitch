#ifndef ACCOUNT_H
#define ACCOUNT_H

#include <QString>

#define FSCOMM_GW_STATE_TRYING 0
#define FSCOMM_GW_STATE_REGISTER 1
#define FSCOMM_GW_STATE_REGED 2
#define FSCOMM_GW_STATE_UNREGED 3
#define FSCOMM_GW_STATE_UNREGISTER 4
#define FSCOMM_GW_STATE_FAILED 5
#define FSCOMM_GW_STATE_FAIL_WAIT 6
#define FSCOMM_GW_STATE_EXPIRED 7
#define FSCOMM_GW_STATE_NOREG 8


static QString fscomm_gw_state_names[] = {
    QString("Trying"),
    QString("Registering"),
    QString("Registered"),
    QString("Un-Registered"),
    QString("Un-Registering"),
    QString("Failed"),
    QString("Failed"),
    QString("Expired"),
    QString("Not applicable")
};

class Account {
public:
    explicit Account(QString name);
    void setName(QString name) { _name = name; }
    QString getName() { return _name; }
    void setState(int state) { _state = state; }
    int getState() { return _state; }
    QString getStateName() { return fscomm_gw_state_names[_state]; }
    QString getUUID() { return _uuid; }

private:
    QString _name;
    int _state;
    QString _uuid;
};

#endif // ACCOUNT_H
