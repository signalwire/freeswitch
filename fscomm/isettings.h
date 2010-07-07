#ifndef ISETTINGS_H
#define ISETTINGS_H

#include <QObject>
#include <QtXml>
#include "fscomm.h"

class ISettings : public QObject {
    Q_OBJECT
public:
    ISettings(QObject *parent = 0);
    QDomElement getConfigNode(QString module);
    void setConfigNode(QDomElement node, QString module);
    void saveToFile();
private:
    static QDomDocument *xml;
    static QMutex *mutex;
};

#endif // ISETTINGS_H
