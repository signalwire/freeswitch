#ifndef PREFACCOUNTS_H
#define PREFACCOUNTS_H

#include <QObject>
#include "ui_prefdialog.h"

class QSettings;

class PrefAccounts
{
public:
    explicit PrefAccounts(Ui::PrefDialog *ui);
    void readConfig();
    void writeConfig();

private:
    Ui::PrefDialog *_ui;
    QSettings *_settings;
};

#endif // PREFACCOUNTS_H
