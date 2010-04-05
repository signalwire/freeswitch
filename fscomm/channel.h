#ifndef CHANNEL_H
#define CHANNEL_H

#include <QtCore>

class Channel
{
public:
    Channel() {}
    Channel(QString uuid);
    QString getUuid() { return _uuid; }
    void setCidName(QString cidName) { _cidName = cidName; }
    QString getCidName() { return _cidName; }
    void setCidNumber(QString cidNumber) { _cidNumber = cidNumber; }
    QString getCidNumber() { return _cidNumber; }
    void setDestinatinonNumber(QString destinationNumber) { _destinationNumber = destinationNumber; }
    QString getDestinationNumber() { return _destinationNumber; }

    int getPaCallId() { return _pa_call_id; }

private:
    QString _uuid;
    QString _cidName;
    QString _cidNumber;
    QString _destinationNumber;
    int _pa_call_id;
};

Q_DECLARE_METATYPE(Channel)

#endif // CHANNEL_H
