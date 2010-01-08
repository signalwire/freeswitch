#ifndef ACCOUNTDIALOG_H
#define ACCOUNTDIALOG_H

#include <QDialog>

namespace Ui {
    class AccountDialog;
}

class AccountDialog : public QDialog {
    Q_OBJECT
public:
    AccountDialog(QWidget *parent = 0);
    ~AccountDialog();

private slots:
    void writeConfig();

protected:
    void changeEvent(QEvent *e);

private:
    Ui::AccountDialog *ui;
};

#endif // ACCOUNTDIALOG_H
