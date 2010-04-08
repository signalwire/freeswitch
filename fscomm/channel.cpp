#include "channel.h"

Channel::Channel(QString uuid):
        _uuid(uuid)
{
    _progressEpoch = 0;
    _progressMediaEpoch = 0;
    _createdEpoch = 0;
}
