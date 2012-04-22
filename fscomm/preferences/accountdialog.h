#ifndef ACCOUNTDIALOG_H
#define ACCOUNTDIALOG_H

#include <QDialog>
#include "fscomm.h"

namespace Ui {
    class AccountDialog;
}

class AccountDialog : public QDialog {
    Q_OBJECT
public:
    AccountDialog(QWidget *parent = 0);
    ~AccountDialog();
    void clear();
    void readConfig();
    void setName(QString name) { _name = name; }

signals:
    void gwAdded(QString);

private slots:
    void writeConfig();
    void addExtraParam();
    void remExtraParam();
    void codecSettingsComboChanged(int);
    void clidSettingsComboChanged(int);

protected:
    void changeEvent(QEvent *e);

private:
    void setParam(QDomElement &parent, QString name, QString value);
    /* Might need the profile as well someday */
    QString _name; /* Needs to be empty when not editing */
    Ui::AccountDialog *ui;
};

#endif // ACCOUNTDIALOG_H
