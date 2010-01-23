#ifndef PREFACCOUNTS_H
#define PREFACCOUNTS_H

#include <QObject>
#include "ui_prefdialog.h"

#define FSCOMM_EVENT_ACC_REMOVED "fscomm::acc_removed"

class QSettings;
class AccountDialog;

class PrefAccounts : public QObject {
    Q_OBJECT
public:
    explicit PrefAccounts(Ui::PrefDialog *ui);
    void writeConfig();

public slots:
    void readConfig(bool reload=true);

private slots:
    void addAccountBtnClicked();
    void editAccountBtnClicked();
    void remAccountBtnClicked();

private:
    Ui::PrefDialog *_ui;
    AccountDialog *_accDlg;
    QSettings *_settings;
};

#endif // PREFACCOUNTS_H
