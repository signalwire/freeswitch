#ifndef PREFDIALOG_H
#define PREFDIALOG_H

#include <QDialog>
#include <QDomDocument>
#include "fscomm.h"

class PrefPortaudio;
class PrefSofia;
class PrefAccounts;
class QAbstractButton;

namespace Ui {
    class PrefDialog;
}

class PrefDialog : public QDialog {
    Q_OBJECT
public:
    PrefDialog(QWidget *parent = 0);
    ~PrefDialog();

protected:
    void changeEvent(QEvent *e);

private slots:
    void writeConfig();
    void clicked(QAbstractButton*);

signals:
    void preprocessorsApplied(QStringList);

private:
    void readConfig();
    PrefAccounts *_pref_accounts;
    Ui::PrefDialog *ui;
    PrefPortaudio *_mod_portaudio;
    PrefSofia *_mod_sofia;
};


#endif // PREFDIALOG_H
