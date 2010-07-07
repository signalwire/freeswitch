#ifndef PREFACCOUNTS_H
#define PREFACCOUNTS_H

#include <QObject>
#include "ui_prefdialog.h"
#include "fscomm.h"

class AccountDialog;

class PrefAccounts : public QObject {
    Q_OBJECT
public:
    explicit PrefAccounts(Ui::PrefDialog *ui);
    void writeConfig();
    void postWriteConfig();

public slots:
    void readConfig();

private slots:
    void addAccountBtnClicked();
    void editAccountBtnClicked();
    void remAccountBtnClicked();

private:
    void markAccountToDelete(QString gwName); /* TODO: Might be interesting to pass the account instead */
    Ui::PrefDialog *_ui;
    AccountDialog *_accDlg;
    QList<QString> _toDelete;
};

#endif // PREFACCOUNTS_H
