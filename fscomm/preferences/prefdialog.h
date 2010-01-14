#ifndef PREFDIALOG_H
#define PREFDIALOG_H

#include <QDialog>
#include <QDomDocument>
#include <QSettings>
#include <fshost.h>

class PrefPortaudio;
class PrefSofia;
class PrefAccounts;

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

private:
    void readConfig();
    QSettings *_settings;
    PrefAccounts *_pref_accounts;
    Ui::PrefDialog *ui;
    PrefPortaudio *_mod_portaudio;
    PrefSofia *_mod_sofia;
};


#endif // PREFDIALOG_H
