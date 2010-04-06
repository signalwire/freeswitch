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

    int getPaCallId() { return _paCallId; }
    void setPaCallId(int paCallId) { _paCallId = paCallId;}

    void setProgressEpoch(qulonglong time) { _progressEpoch = time/1000000; }
    qulonglong getProgressEpoch() { return _progressEpoch; }
    void setProgressMediaEpoch(qulonglong time) { _progressMediaEpoch = time/1000000; }
    qulonglong getProgressMediaEpoch() { return _progressMediaEpoch; }

private:
    QString _uuid;
    QString _cidName;
    QString _cidNumber;
    QString _destinationNumber;
    int _paCallId;
    qulonglong _progressEpoch;
    qulonglong _progressMediaEpoch;
};

Q_DECLARE_METATYPE(Channel)

#endif // CHANNEL_H
