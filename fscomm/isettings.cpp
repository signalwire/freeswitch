#include "isettings.h"
#include <QtGui>

QMutex *ISettings::mutex = new QMutex();
QDomDocument *ISettings::xml = 0;

ISettings::ISettings(QObject *parent) :
    QObject(parent)
{
    ISettings::mutex->lock();
    if (!(ISettings::xml)) {
        QFile *f = new QFile(QString("%1%2%3").arg(SWITCH_GLOBAL_dirs.conf_dir, SWITCH_PATH_SEPARATOR ,"freeswitch.xml"));
        if ( !f->open(QIODevice::ReadOnly | QIODevice::Text ) ) {
            /* TODO: Let the user know */
            qDebug() << "Could not read from file.";
            return;
        }
        QString errMsg;
        int errLine = 0, errCol = 0;
        ISettings::xml = new QDomDocument();
        if ( !ISettings::xml->setContent(f, &errMsg, &errLine, &errCol) ) {
            /* TODO: Let the user know */
            qDebug() << "Could not set content";
        }
        f->close();
        delete(f);
    }
    ISettings::mutex->unlock();
}

QDomElement ISettings::getConfigNode(QString module) {
    /* We don't need to lock since we are just reading (true?) */
    QDomElement e = ISettings::xml->documentElement();
    QDomNodeList nl = e.elementsByTagName("configuration");
    for(int i = 0; i < nl.count(); i++) {
        QDomElement el = nl.at(i).toElement();
        if ( el.attribute("name") == module ) {
            return el;
        }
    }
    return QDomElement();
}

void ISettings::setConfigNode(QDomElement node, QString module) {
    ISettings::mutex->lock();
    QDomElement e = ISettings::xml->documentElement();
    QDomNodeList l = e.elementsByTagName("configuration");
    for (int i = 0; i < l.count(); i++) {
        QDomElement el = l.at(i).toElement();
        if ( el.attribute("name") == module ) {
            /* Found the proper module to replace */
            el.parentNode().replaceChild(node.toDocumentFragment(),el);
        }
    }
    ISettings::mutex->unlock();
}

void ISettings::saveToFile() {
    ISettings::mutex->lock();
    if (ISettings::xml) {
        QFile *f = new QFile(QString("%1%2%3").arg(SWITCH_GLOBAL_dirs.conf_dir, SWITCH_PATH_SEPARATOR ,"freeswitch.xml"));
        if ( !f->open(QFile::WriteOnly | QFile::Truncate) ) {
            /* TODO: Let the user know */
            qDebug() << "Could not open from file.";
            return;
        }
        QTextStream out(f);
        ISettings::xml->save(out, 2);
        f->close();
        if ( !f->open(QFile::ReadOnly) ) {
            /* TODO: Let the user know */
            qDebug() << "Could not open from file.";
            return;
        }
        QString errMsg;
        int errLine = 0, errCol = 0;
        if ( !ISettings::xml->setContent(f, &errMsg, &errLine, &errCol) ) {
            /* TODO: Let the user know */
            qDebug() << "Could not set content";
        }
        f->close();
        delete(f);
    }
    ISettings::mutex->unlock();
}
