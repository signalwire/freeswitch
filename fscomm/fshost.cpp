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

#include <QtGui>
#include "fshost.h"
#include "mod_qsettings/mod_qsettings.h"

/* Declare it globally */
FSHost g_FSHost;

FSHost::FSHost(QObject *parent) :
    QThread(parent)
{
    /* Initialize libs & globals */
    qDebug() << "Initializing globals..." << endl;
    switch_core_setrlimits();
    switch_core_set_globals();

    qRegisterMetaType<QSharedPointer<Call> >("QSharedPointer<Call>");
    qRegisterMetaType<QSharedPointer<Call> >("QSharedPointer<Channel>");
    qRegisterMetaType<QSharedPointer<Account> >("QSharedPointer<Account>");

    connect(this, SIGNAL(loadedModule(QString,QString)), this, SLOT(minimalModuleLoaded(QString,QString)));

}

QBool FSHost::isModuleLoaded(QString modName)
{
    return _loadedModules.contains(modName);
}

void FSHost::createFolders()
{
    /* Create directory structure for softphone with default configs */
    QDir conf_dir = QDir::home();
    if (!conf_dir.exists(".fscomm"))
        conf_dir.mkpath(".fscomm");
    if (!conf_dir.exists(".fscomm/recordings"))
        conf_dir.mkpath(".fscomm/recordings");
    if (!conf_dir.exists(".fscomm/sounds")) {
        conf_dir.mkpath(".fscomm/sounds");
        QFile::copy(":/sounds/test.wav", QString("%1/.fscomm/sounds/test.wav").arg(QDir::homePath()));
    }
    if(!QFile::exists(QString("%1/.fscomm/conf/freeswitch.xml").arg(conf_dir.absolutePath()))) {
        conf_dir.mkdir(".fscomm/conf");
        QFile rootXML(":/confs/freeswitch.xml");
        QString dest = QString("%1/.fscomm/conf/freeswitch.xml").arg(conf_dir.absolutePath());
        rootXML.copy(dest);
    }

    /* Set all directories to the home user directory */
    if (conf_dir.cd(".fscomm"))
    {
        SWITCH_GLOBAL_dirs.conf_dir = (char *) malloc(strlen(QString("%1/conf").arg(conf_dir.absolutePath()).toAscii().constData()) + 1);
        if (!SWITCH_GLOBAL_dirs.conf_dir) {
            emit coreLoadingError("Cannot allocate memory for conf_dir.");
        }
        strcpy(SWITCH_GLOBAL_dirs.conf_dir, QString("%1/conf").arg(conf_dir.absolutePath()).toAscii().constData());

        SWITCH_GLOBAL_dirs.log_dir = (char *) malloc(strlen(QString("%1/log").arg(conf_dir.absolutePath()).toAscii().constData()) + 1);
        if (!SWITCH_GLOBAL_dirs.log_dir) {
            emit coreLoadingError("Cannot allocate memory for log_dir.");
        }
        strcpy(SWITCH_GLOBAL_dirs.log_dir, QString("%1/log").arg(conf_dir.absolutePath()).toAscii().constData());

        SWITCH_GLOBAL_dirs.run_dir = (char *) malloc(strlen(QString("%1/run").arg(conf_dir.absolutePath()).toAscii().constData()) + 1);
        if (!SWITCH_GLOBAL_dirs.run_dir) {
            emit coreLoadingError("Cannot allocate memory for run_dir.");
        }
        strcpy(SWITCH_GLOBAL_dirs.run_dir, QString("%1/run").arg(conf_dir.absolutePath()).toAscii().constData());

        SWITCH_GLOBAL_dirs.db_dir = (char *) malloc(strlen(QString("%1/db").arg(conf_dir.absolutePath()).toAscii().constData()) + 1);
        if (!SWITCH_GLOBAL_dirs.db_dir) {
            emit coreLoadingError("Cannot allocate memory for db_dir.");
        }
        strcpy(SWITCH_GLOBAL_dirs.db_dir, QString("%1/db").arg(conf_dir.absolutePath()).toAscii().constData());

        SWITCH_GLOBAL_dirs.script_dir = (char *) malloc(strlen(QString("%1/script").arg(conf_dir.absolutePath()).toAscii().constData()) + 1);
        if (!SWITCH_GLOBAL_dirs.script_dir) {
            emit coreLoadingError("Cannot allocate memory for script_dir.");
        }
        strcpy(SWITCH_GLOBAL_dirs.script_dir, QString("%1/script").arg(conf_dir.absolutePath()).toAscii().constData());

        SWITCH_GLOBAL_dirs.htdocs_dir = (char *) malloc(strlen(QString("%1/htdocs").arg(conf_dir.absolutePath()).toAscii().constData()) + 1);
        if (!SWITCH_GLOBAL_dirs.htdocs_dir) {
            emit coreLoadingError("Cannot allocate memory for htdocs_dir.");
        }
        strcpy(SWITCH_GLOBAL_dirs.htdocs_dir, QString("%1/htdocs").arg(conf_dir.absolutePath()).toAscii().constData());
    }
}

void FSHost::run(void)
{
    switch_core_flag_t flags = SCF_USE_SQL | SCF_USE_AUTO_NAT;
    const char *err = NULL;
    switch_bool_t console = SWITCH_FALSE;
    switch_status_t destroy_status;

    createFolders();

    /* If you need to override configuration directories, you need to change them in the SWITCH_GLOBAL_dirs global structure */
    qDebug() << "Initializing core...";
    /* Initialize the core and load modules, that will startup FS completely */
    if (switch_core_init(flags, console, &err) != SWITCH_STATUS_SUCCESS) {
        fprintf(stderr, "Failed to initialize FreeSWITCH's core: %s\n", err);
        emit coreLoadingError(err);
    }

    qDebug() << "Everything OK, Entering runtime loop ...";

    if (switch_event_bind("FSHost", SWITCH_EVENT_ALL, SWITCH_EVENT_SUBCLASS_ANY, eventHandlerCallback, NULL) != SWITCH_STATUS_SUCCESS) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind!\n");
    }

    /* Load our QSettings module */
    if (mod_qsettings_load() != SWITCH_STATUS_SUCCESS)
    {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't load mod_qsettings\n");
    }

    emit loadingModules("Loading modules...", Qt::AlignRight|Qt::AlignBottom, Qt::blue);
    if (switch_core_init_and_modload(flags, console, &err) != SWITCH_STATUS_SUCCESS) {
        fprintf(stderr, "Failed to initialize FreeSWITCH's core: %s\n", err);
        emit coreLoadingError(err);
    }

    emit ready();

    /* Go into the runtime loop. If the argument is true, this basically sets runtime.running = 1 and loops while that is set
     * If its false, it initializes the libedit for the console, then does the same thing
     */
    switch_core_runtime_loop(!console);
    fflush(stdout);


    switch_event_unbind_callback(eventHandlerCallback);
    /* When the runtime loop exits, its time to shutdown */
    destroy_status = switch_core_destroy();
    if (destroy_status == SWITCH_STATUS_SUCCESS)
    {
        qDebug() << "We have properly shutdown the core.";
    }
}

void FSHost::generalEventHandler(QSharedPointer<switch_event_t>event)
{
    QString uuid = switch_event_get_header_nil(event.data(), "Unique-ID");

    switch(event.data()->event_id) {
    case SWITCH_EVENT_CHANNEL_CREATE: /*1A - 17B*/
        {
            eventChannelCreate(event, uuid);
            break;
        }
    case SWITCH_EVENT_CHANNEL_ANSWER: /*2A - 31B*/
        {
            eventChannelAnswer(event, uuid);
            break;
        }
    case SWITCH_EVENT_CODEC:/*3/4A - 24/25B*/
        {
            eventCodec(event, uuid);
            break;
        }
    case SWITCH_EVENT_CHANNEL_STATE:/*6/7/8/37/44/46A - 20/21/22/28/38/40/42B*/
        {
            eventChannelState(event, uuid);
            break;
        }
    case SWITCH_EVENT_CHANNEL_EXECUTE:/*9/11/13/15A*/
        {
            eventChannelExecute(event, uuid);
            break;
        }
    case SWITCH_EVENT_CHANNEL_EXECUTE_COMPLETE:/*10/12/14/16/35A*/
        {
            eventChannelExecuteComplete(event, uuid);
            break;
        }
    case SWITCH_EVENT_CHANNEL_OUTGOING:/*18B*/
        {
            eventChannelOutgoing(event, uuid);
            break;
        }
    case SWITCH_EVENT_CHANNEL_ORIGINATE:/*19B*/
        {
            eventChannelOriginate(event, uuid);
            break;
        }
    case SWITCH_EVENT_CALL_UPDATE:/*23/29/30B*/
        {
            eventCallUpdate(event, uuid);
            break;
        }
    case SWITCH_EVENT_CHANNEL_PROGRESS:
        {
            eventChannelProgress(event, uuid);
            break;
        }
    case SWITCH_EVENT_CHANNEL_PROGRESS_MEDIA:/*26B*/
        {
            eventChannelProgressMedia(event, uuid);
            break;
        }
    case SWITCH_EVENT_CHANNEL_BRIDGE:/*27A*/
        {
            eventChannelBridge(event, uuid);
            break;
        }
    /*32?*/
    /*case SWITCH_EVENT_RECV_INFO:
        {
            eventRecvInfo(event, uuid);
            break;
        }*/
    case SWITCH_EVENT_CHANNEL_HANGUP:/*36A-33B*/
        {
            eventChannelHangup(event, uuid);
            break;
        }
    case SWITCH_EVENT_CHANNEL_UNBRIDGE:/*34A*/
        {
            eventChannelUnbridge(event, uuid);
            break;
        }
    case SWITCH_EVENT_CHANNEL_HANGUP_COMPLETE:/*39/43B*/
        {
            eventChannelHangupComplete(event, uuid);
            break;
        }
    case SWITCH_EVENT_CHANNEL_DESTROY:/*45A-41B*/
        {
            eventChannelDestroy(event, uuid);
            break;
        }
    case SWITCH_EVENT_CUSTOM:/*5A*/
        {
            if (strcmp(event.data()->subclass_name, "sofia::gateway_state") == 0)
            {
                QString state = switch_event_get_header_nil(event.data(), "State");
                QString gw = switch_event_get_header_nil(event.data(), "Gateway");
                QSharedPointer<Account> acc = _accounts.value(gw);
                if (acc.isNull())
                    return;

                if (state == "TRYING") {
                    acc.data()->setStatusPhrase(switch_event_get_header_nil(event.data(), "Phrase"));
                    acc.data()->setState(FSCOMM_GW_STATE_TRYING);
                    emit accountStateChange(acc);
                } else if (state == "REGISTER") {
                    acc.data()->setStatusPhrase(switch_event_get_header_nil(event.data(), "Phrase"));
                    acc.data()->setState(FSCOMM_GW_STATE_REGISTER);
                    emit accountStateChange(acc);
                } else if (state == "REGED") {
                    acc.data()->setStatusPhrase(switch_event_get_header_nil(event.data(), "Phrase"));
                    acc.data()->setState(FSCOMM_GW_STATE_REGED);
                    emit accountStateChange(acc);
                } else if (state == "UNREGED") {
                    acc.data()->setStatusPhrase(switch_event_get_header_nil(event.data(), "Phrase"));
                    acc.data()->setState(FSCOMM_GW_STATE_UNREGED);
                    emit accountStateChange(acc);
                } else if (state == "UNREGISTER") {
                    acc.data()->setStatusPhrase(switch_event_get_header_nil(event.data(), "Phrase"));
                    acc.data()->setState(FSCOMM_GW_STATE_UNREGISTER);
                    emit accountStateChange(acc);
                } else if (state =="FAILED") {
                    acc.data()->setStatusPhrase(switch_event_get_header_nil(event.data(), "Phrase"));
                    acc.data()->setState(FSCOMM_GW_STATE_FAILED);
                    emit accountStateChange(acc);
                } else if (state == "FAIL_WAIT") {
                    acc.data()->setState(FSCOMM_GW_STATE_FAIL_WAIT);
                    emit accountStateChange(acc);
                } else if (state == "EXPIRED") {
                    acc.data()->setStatusPhrase(switch_event_get_header_nil(event.data(), "Phrase"));
                    acc.data()->setState(FSCOMM_GW_STATE_EXPIRED);
                    emit accountStateChange(acc);
                } else if (state == "NOREG") {
                    acc.data()->setStatusPhrase(switch_event_get_header_nil(event.data(), "Phrase"));
                    acc.data()->setState(FSCOMM_GW_STATE_NOREG);
                    emit accountStateChange(acc);
                }
            }
            else if (strcmp(event.data()->subclass_name, "sofia::gateway_add") == 0)
            {
                QString gw = switch_event_get_header_nil(event.data(), "Gateway");
                Account * accPtr = new Account(gw);
                QSharedPointer<Account> acc = QSharedPointer<Account>(accPtr);
                acc.data()->setState(FSCOMM_GW_STATE_NOAVAIL);
                _accounts.insert(gw, acc);
                emit newAccount(acc);
            }
            else if (strcmp(event.data()->subclass_name, "sofia::gateway_delete") == 0)
            {
                QSharedPointer<Account> acc = _accounts.take(switch_event_get_header_nil(event.data(), "Gateway"));
                if (!acc.isNull())
                    emit delAccount(acc);
            }
            else
            {
                //printEventHeaders(event);
            }
            break;
        }
    case SWITCH_EVENT_MODULE_LOAD:
        {
            QString modType = switch_event_get_header_nil(event.data(), "type");
            QString modKey = switch_event_get_header_nil(event.data(), "key");
            emit loadedModule(modType, modKey);
            break;
        }
    default:
        break;
    }
}

void FSHost::eventChannelCreate(QSharedPointer<switch_event_t>event, QString uuid)
{
    Channel *channelPtr = new Channel(uuid);
    QSharedPointer<Channel>channel(channelPtr);
    _channels.insert(uuid, channel);
}
void FSHost::eventChannelAnswer(QSharedPointer<switch_event_t>event, QString uuid)
{
    _channels.value(uuid).data()->setDestinatinonNumber(switch_event_get_header_nil(event.data(), "Caller-Destination-Number"));
    if (_active_calls.contains(uuid))
    {
        _active_calls.value(uuid).data()->setAnsweredEpoch(QString(switch_event_get_header_nil(event.data(), "Caller-Channel-Answered-Time")).toULongLong());
        _active_calls.value(uuid).data()->setState(FSCOMM_CALL_STATE_ANSWERED);
        emit answered(_active_calls.value(uuid));
    }
}
void FSHost::eventChannelState(QSharedPointer<switch_event_t>event, QString uuid)
{}
void FSHost::eventChannelExecute(QSharedPointer<switch_event_t>event, QString uuid)
{}
void FSHost::eventChannelExecuteComplete(QSharedPointer<switch_event_t>event, QString uuid)
{
    _channels.value(uuid).data()->setPaCallId(atoi(switch_event_get_header_nil(event.data(), "variable_pa_call_id")));
}
void FSHost::eventChannelOutgoing(QSharedPointer<switch_event_t>event, QString uuid)
{
    /* Checks if this is an inbound or outbound call */
    /** Outbound call */
    if ( strcmp(switch_event_get_header_nil(event.data(), "Caller-Source"), "mod_portaudio") == 0 )
    {
        Call *callPtr = new Call();

        callPtr->setCallDirection(FSCOMM_CALL_DIRECTION_OUTBOUND);
        callPtr->setChannel(_channels.value(uuid));
        callPtr->setOtherLegChannel(_channels.value(switch_event_get_header_nil(event.data(), "Other-Leg-Unique-ID")));
        QSharedPointer<Call> call(callPtr);
        _active_calls.insert(uuid, call);
        call.data()->setState(FSCOMM_CALL_STATE_TRYING);
        emit newOutgoingCall(call);
    }
    /** Inbound call */
    else
    {
        Call *callPtr = new Call();

        callPtr->setCallDirection(FSCOMM_CALL_DIRECTION_INBOUND);
        callPtr->setChannel(_channels.value(switch_event_get_header_nil(event.data(), "Other-Leg-Unique-ID")));
        callPtr->setOtherLegChannel(_channels.value(uuid));
        QSharedPointer<Call> call(callPtr);
        _active_calls.insert(switch_event_get_header_nil(event.data(), "Other-Leg-Unique-ID"), call);
        call.data()->setState(FSCOMM_CALL_STATE_RINGING);
        emit ringing(call);
    }
}
void FSHost::eventChannelOriginate(QSharedPointer<switch_event_t>event, QString uuid)
{}
void FSHost::eventChannelProgress(QSharedPointer<switch_event_t>event, QString uuid)
{}
void FSHost::eventChannelProgressMedia(QSharedPointer<switch_event_t>event, QString uuid)
{
    _channels.value(uuid).data()->setProgressEpoch(QString(switch_event_get_header_nil(event.data(), "Caller-Channel-Progress-Time")).toULongLong());
    if (_active_calls.contains(uuid))
    {
        _active_calls.value(uuid).data()->setState(FSCOMM_CALL_STATE_RINGING);
        emit ringing(_active_calls.value(uuid));
    }
}
void FSHost::eventChannelBridge(QSharedPointer<switch_event_t>event, QString uuid)
{
    QString time;
    time = switch_event_get_header_nil(event.data(), "Caller-Channel-Progress-Time");
    if (time.toULongLong() > 0) _channels.value(uuid).data()->setProgressEpoch(time.toULongLong());
    time = switch_event_get_header_nil(event.data(), "Caller-Channel-Progress-Media-Time");
    if (time.toULongLong() > 0) _channels.value(uuid).data()->setProgressMediaEpoch(time.toULongLong());
}
void FSHost::eventChannelHangup(QSharedPointer<switch_event_t>event, QString uuid)
{
    if (_active_calls.contains(uuid))
    {
        emit hungup(_active_calls.take(uuid));
    }
}
void FSHost::eventChannelUnbridge(QSharedPointer<switch_event_t>event, QString uuid)
{}
void FSHost::eventChannelHangupComplete(QSharedPointer<switch_event_t>event, QString uuid)
{}
void FSHost::eventChannelDestroy(QSharedPointer<switch_event_t>event, QString uuid)
{
    _channels.take(uuid);
}
void FSHost::eventCodec(QSharedPointer<switch_event_t>event, QString uuid)
{
    _channels.value(uuid).data()->setCidName(switch_event_get_header_nil(event.data(), "Caller-Caller-ID-Name"));
    _channels.value(uuid).data()->setCidNumber(switch_event_get_header_nil(event.data(), "Caller-Caller-ID-Number"));
}
void FSHost::eventCallUpdate(QSharedPointer<switch_event_t>event, QString uuid)
{}
void FSHost::eventRecvInfo(QSharedPointer<switch_event_t>event, QString uuid)
{}

void FSHost::minimalModuleLoaded(QString modType, QString modKey)
{
    if (modType == "endpoint")
    {
        _loadedModules.append(modKey);
    }
}

void FSHost::accountReloadCmd(QSharedPointer<Account> acc)
{
    QString res;
    QString arg = QString("profile softphone killgw %1").arg(acc.data()->getName());

    connect(this, SIGNAL(delAccount(QSharedPointer<Account>)), this, SLOT(accountReloadSlot(QSharedPointer<Account>)));

    if (g_FSHost.sendCmd("sofia", arg.toAscii().data() , &res) != SWITCH_STATUS_SUCCESS)
    {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not killgw %s from profile softphone.\n",
                          acc.data()->getName().toAscii().data());
    }
    _reloading_Accounts.append(acc.data()->getName());
}

void FSHost::accountReloadSlot(QSharedPointer<Account> acc)
{
    if (_reloading_Accounts.contains(acc.data()->getName()))
    {
        _reloading_Accounts.takeAt(_reloading_Accounts.indexOf(acc.data()->getName(), 0));
        QString res;
        if (g_FSHost.sendCmd("sofia", "profile softphone rescan", &res) != SWITCH_STATUS_SUCCESS)
        {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not rescan the softphone profile.\n");
            return;
        }
        if (_reloading_Accounts.isEmpty())
            disconnect(this, SLOT(accountReloadSlot(QSharedPointer<Account>)));
    }
}

switch_status_t FSHost::sendCmd(const char *cmd, const char *args, QString *res)
{
    switch_status_t status = SWITCH_STATUS_FALSE;
    switch_stream_handle_t stream = { 0 };
    SWITCH_STANDARD_STREAM(stream);
    qDebug() << "Sending command: " << cmd << args << endl;
    status = switch_api_execute(cmd, args, NULL, &stream);
    *res = switch_str_nil((char *) stream.data);
    switch_safe_free(stream.data);

    return status;
}

QSharedPointer<Account> FSHost::getAccountByUUID(QString uuid)
{
    foreach(QSharedPointer<Account> acc, _accounts.values())
    {
        if (acc.data()->getUUID() == uuid)
            return acc;
    }
    return QSharedPointer<Account>();
}

QSharedPointer<Call> FSHost::getCurrentActiveCall()
{
    foreach(QSharedPointer<Call> call, _active_calls.values())
    {
        if (call.data()->isActive())
            return call;
    }
    return QSharedPointer<Call>();
}

void FSHost::printEventHeaders(QSharedPointer<switch_event_t>event)
{
    switch_event_header_t *hp;
    qDebug() << QString("Received event: %1(%2)").arg(switch_event_name(event.data()->event_id), switch_event_get_header_nil(event.data(), "Event-Subclass"));
    for (hp = event.data()->headers; hp; hp = hp->next) {
        qDebug() << hp->name << "=" << hp->value;
    }
    qDebug() << "\n\n";
}

QSharedPointer<Account> FSHost::getAccountByName(QString accStr)
{
    foreach(QSharedPointer<Account> acc, _accounts.values())
    {
        if (acc.data()->getName() == accStr)
            return acc;
    }
    return QSharedPointer<Account>();
}

QSharedPointer<Account> FSHost::getCurrentDefaultAccount()
{
    QSettings settings;
    settings.beginGroup("FreeSWITCH/conf/globals");
    QString accString = settings.value("default_gateway").toString();
    settings.endGroup();
    return getAccountByName(accString);
}
