#ifndef PREFPORTAUDIO_H
#define PREFPORTAUDIO_H

#include <QObject>
#include <QDomDocument>
#include "ui_prefdialog.h"

class QSettings;

class PrefPortaudio : public QObject
{
Q_OBJECT
public:
    explicit PrefPortaudio(Ui::PrefDialog *ui, QObject *parent = 0);
    void writeConfig();
    void readConfig();

private:
    void getPaDevlist(void);
    QSettings *_settings;
    Ui::PrefDialog *_ui;
    QDomDocument _xmlPaDevList;
};

#endif // PREFPORTAUDIO_H
