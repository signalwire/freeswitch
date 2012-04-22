#ifndef ACCOUNT_H
#define ACCOUNT_H

#include <QString>
//#include "fscomm.h" Why does this break AccountManager?

class Account {
public:
    explicit Account(QString name);
    void setName(QString name) { _name = name; }
    QString getName() { return _name; }
    void setState(int state) { _state = state; }
    int getState() { return _state; }
    QString getStateName();
    QString getUUID() { return _uuid; }
    void setStausCode(QString code) { _statusCode = code; }
    void setStatusPhrase(QString phrase) { _statusPhrase = phrase; }

private:
    QString _name;
    int _state;
    QString _uuid;
    QString _statusCode;
    QString _statusPhrase;
};

#endif // ACCOUNT_H
