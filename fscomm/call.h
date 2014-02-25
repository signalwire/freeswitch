/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2014, Anthony Minessale II <anthm@freeswitch.org>
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Joao Mesquita <jmesquita@freeswitch.org>
 *
 */
#ifndef CALL_H
#define CALL_H

#include <QtCore>
#include <QString>
#include <switch.h>
#include "channel.h"

typedef enum {
    FSCOMM_CALL_STATE_RINGING = 0,
    FSCOMM_CALL_STATE_TRYING  = 1,
    FSCOMM_CALL_STATE_ANSWERED = 2,
    FSCOMM_CALL_STATE_FAILED = 3,
    FSCOMM_CALL_STATE_TRANSFER = 4
} fscomm_call_state_t;

typedef enum {
        FSCOMM_CALL_DIRECTION_INBOUND = 0,
        FSCOMM_CALL_DIRECTION_OUTBOUND = 1
} fscomm_call_direction_t;

class Call {
public:
    Call();
    /* Needs rework */
    QString getCidName(void) { return (_direction == FSCOMM_CALL_DIRECTION_INBOUND) ? _otherLegChannel.data()->getCidName() : _channel.data()->getCidName(); }
    QString getCidNumber(void) { return (_direction == FSCOMM_CALL_DIRECTION_INBOUND) ? _otherLegChannel.data()->getCidNumber() : _channel.data()->getCidNumber(); }
    QString getDestinationNumber(void) { return _otherLegChannel.data()->getDestinationNumber(); }

    void setChannel(QSharedPointer<Channel> channel) { _channel = channel; }
    QSharedPointer<Channel> getChannel() { return _channel; }
    void setOtherLegChannel(QSharedPointer<Channel> channel) { _otherLegChannel = channel; }
    QSharedPointer<Channel> getOtherLegChannel() { return _otherLegChannel; }

    QString getUuid(void) { return _channel.data()->getUuid(); }
    QString getOtherLegUuid(void) { return _otherLegChannel.data()->getUuid(); }
    void setCallDirection(fscomm_call_direction_t dir) { _direction = dir; }
    int getCallID(void) { return _channel.data()->getPaCallId(); }
    fscomm_call_direction_t getDirection() { return _direction; }
    fscomm_call_state_t getState() { return _state; }
    void setState(fscomm_call_state_t state) { _state = state; }
    void setCause(QString cause) { _cause = cause; qDebug()<<cause; }
    QString getCause() { return _cause; qDebug() << _cause; }
    void setActive(bool isActive) { _isActive = isActive; }
    bool isActive() { return _isActive == true; }
    switch_status_t toggleRecord(bool);
    switch_status_t toggleHold(bool);
    void sendDTMF(QString digit);
    void setAnsweredEpoch(qulonglong time) { _answeredEpoch = time/1000000; }
    QTime getCurrentStateTime();

    /*bool transfer();*/

private:
    QSharedPointer<Channel> _channel; /* This should be our portaudio channel */
    QSharedPointer<Channel> _otherLegChannel;

    QString _cause;
    fscomm_call_direction_t _direction;

    bool _isActive;
    QString _recording_filename;
    fscomm_call_state_t _state;
    qulonglong _answeredEpoch;
};

Q_DECLARE_METATYPE(Call)

#endif // CALL_H
