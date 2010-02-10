#ifndef PREFSOFIA_H
#define PREFSOFIA_H

#include <QObject>
#include "ui_prefdialog.h"

class PrefSofia : public QObject
{
Q_OBJECT
public:
    explicit PrefSofia(Ui::PrefDialog *ui, QObject *parent = 0);
    void writeConfig();
    void readConfig();

private:
    Ui::PrefDialog *_ui;

};

#endif // PREFSOFIA_H
