#ifndef PREFPORTAUDIO_H
#define PREFPORTAUDIO_H

#include <QObject>
#include <QDomDocument>
#include "ui_prefdialog.h"
#include "fscomm.h"

class PrefPortaudio : public QObject
{
Q_OBJECT
public:
    explicit PrefPortaudio(Ui::PrefDialog *ui, QObject *parent = 0);
    void writeConfig();
    void postWriteConfig();
    void readConfig();

private slots:
    void ringFileChoose();
    void holdFileChoose();
    void indevChangeDev(int);
    void outdevChangeDev(int);
    void ringdevChangeDev(int);
    void ringdevTest();
    void loopTest();
    void refreshDevList();
    void applyPreprocessors(bool);

signals:
    void preprocessorsApplied(QStringList);

private:
    void getPaDevlist(void);
    Ui::PrefDialog *_ui;
    QDomDocument _xmlPaDevList;
};

#endif // PREFPORTAUDIO_H
