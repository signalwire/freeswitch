#ifndef PREFACCOUNTS_H
#define PREFACCOUNTS_H

#include <QObject>
#include "ui_prefdialog.h"

class QSettings;
class AccountDialog;

class PrefAccounts : public QObject {
    Q_OBJECT
public:
    explicit PrefAccounts(Ui::PrefDialog *ui);
    void writeConfig();

public slots:
    void readConfig();

private slots:
    void addAccountBtnClicked();

private:
    Ui::PrefDialog *_ui;
    AccountDialog *_accDlg;
    QSettings *_settings;
};

#endif // PREFACCOUNTS_H
