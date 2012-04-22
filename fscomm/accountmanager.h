#ifndef ACCOUNTMANAGER_H
#define ACCOUNTMANAGER_H

#include <QObject>
#include "fscomm.h"

class AccountManager : public QObject
{
Q_OBJECT
public:
    explicit AccountManager(QObject *parent = 0);
    void newAccount(Account &acc);

signals:
    void sigNewAccount(Account &acc);

public slots:
private slots:
    void newEventSlot(QSharedPointer<switch_event_t>);

private:
    static QList<QSharedPointer<Account> > _accounts;

};

#endif // ACCOUNTMANAGER_H
