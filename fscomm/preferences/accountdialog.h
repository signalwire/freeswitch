#ifndef ACCOUNTDIALOG_H
#define ACCOUNTDIALOG_H

#include <QDialog>

namespace Ui {
    class AccountDialog;
}

class QSettings;

class AccountDialog : public QDialog {
    Q_OBJECT
public:
    AccountDialog(QString accId, QWidget *parent = 0);
    ~AccountDialog();
    void clear();
    void setAccId(QString);
    void readConfig();

signals:
    void gwAdded();

private slots:
    void writeConfig();
    void addExtraParam();
    void remExtraParam();

protected:
    void changeEvent(QEvent *e);

private:
    QString _accId;
    Ui::AccountDialog *ui;
    QSettings *_settings;
};

#endif // ACCOUNTDIALOG_H
