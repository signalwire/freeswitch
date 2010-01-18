/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2009, Anthony Minessale II <anthm@freeswitch.org>
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

#include "call.h"
#include <fshost.h>

Call::Call()
{
}

Call::Call(int call_id, QString cid_name, QString cid_number, fscomm_call_direction_t direction, QString uuid) :
        _call_id(call_id),
        _cid_name(cid_name),
        _cid_number(cid_number),
        _direction(direction),
        _uuid (uuid)
{
    _isActive = false;
}

switch_status_t Call::toggleRecord(bool startRecord)
{
    QDir conf_dir = QDir::home();
    QString result;
    switch_status_t status;

    if (startRecord)
    {
        _recording_filename = QString("%1/.fscomm/recordings/%2_%3.wav").arg(
                                         conf_dir.absolutePath(),
                                         QDateTime::currentDateTime().toString("yyyyMMddhhmmss"),
                                         _cid_number);
        status = g_FSHost.sendCmd("uuid_record", QString("%1 start %2").arg(_uuid, _recording_filename).toAscii().data(),&result);
    }
    else
    {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Stopping call recording on call [%s]\n",
                          _uuid.toAscii().data());
        status = g_FSHost.sendCmd("uuid_record", QString("%1 stop %2").arg(_uuid, _recording_filename).toAscii().data(),&result);
    }

    return status;
}
