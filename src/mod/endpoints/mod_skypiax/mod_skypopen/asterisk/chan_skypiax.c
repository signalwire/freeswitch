//indent -gnu -ts4 -br -brs -cdw -lp -ce -nbfda -npcs -nprs -npsl -nbbo -saf -sai -saw -cs -bbo -nhnl -nut -sob -l90 
#include "skypiax.h"

/* LOCKS */
/*! \brief Protect the skypiax_usecnt */
AST_MUTEX_DEFINE_STATIC(skypiax_usecnt_lock);
/*! \brief Protect the monitoring thread, so only one process can kill or start it, and not
 *    when it's doing something critical. */
AST_MUTEX_DEFINE_STATIC(skypiax_monlock);
/*! \brief Protect the interfaces list */
AST_MUTEX_DEFINE_STATIC(skypiax_iflock);

/* GLOBAL VARIABLES */
int running = 1;
int skypiax_dir_entry_extension = 1;	//FIXME one var for each interface!
char skypiax_console_active_array[50] = "";
char *skypiax_console_active = skypiax_console_active_array;
/*! \brief Count of active channels for this module */
int skypiax_usecnt = 0;
int skypiax_debug = 0;
/*! \brief This is the thread for the monitor which checks for input on the channels
 *    which are not currently in use.  */
pthread_t skypiax_monitor_thread = AST_PTHREADT_NULL;
pthread_t skypiax_monitor_audio_thread = AST_PTHREADT_NULL;

/* CONSTANTS */
/*! \brief Textual description for this module */
const char skypiax_desc[] = "Skypiax, Skype Driver";
/*! \brief Textual type for this module */
const char skypiax_type[] = "Skypiax";
/*! \brief Name of configuration file for this module */
const char skypiax_config[] = "skypiax.conf";

char skypiax_console_skypiax_usage[] =
	"       \n" "chan_skypiax commands info\n" "       \n"
	"       chan_skypiax adds to Asterisk the following CLI commands:\n" "       \n"
	"       CLI COMMANDS:\n" "          skypiax_hangup\n" "          skypiax_dial\n"
	"          skypiax_console\n" "          skypiax_playback_boost\n"
	"          skypiax_capture_boost\n" "          skypiax_skype\n"
	"          skypiax_dir_import\n" "\n" "       You can type 'help [command]' to obtain more specific info on usage.\n" "       \n";
char skypiax_console_hangup_usage[] =
	"Usage: skypiax_hangup\n"
	"       Hangs up any call currently placed on the \"current\" skypiax_console (Skypiax) channel.\n"
	"       Enter 'help skypiax_console' on how to change the \"current\" skypiax_console\n";
char skypiax_console_playback_boost_usage[] =
	"Usage: skypiax_playback_boost [value]\n"
	"       Shows or set the value of boost applied to the outgoing sound (voice). Possible values are: 0 (no boost applied), -40 to 40 (negative to positive range, in db). Without specifying a value, it just shows the current value. The value is for the  \"current\" skypiax_console (Skypiax) channel.\n"
	"       Enter 'help skypiax_console' on how to change the \"current\" skypiax_console\n";
char skypiax_console_capture_boost_usage[] =
	"Usage: skypiax_capture_boost [value]\n"
	"       Shows or set the value of boost applied to the incoming sound (voice). Possible values are: 0 (no boost applied), -40 to 40 (negative to positive range, in db). Without specifying a value, it just shows the current value. The value is for the  \"current\" skypiax_console (Skypiax) channel.\n"
	"       Enter 'help skypiax_console' on how to change the \"current\" skypiax_console\n";

char skypiax_console_dial_usage[] =
	"Usage: skypiax_dial [DTMFs]\n"
	"       Dials a given DTMF string in the call currently placed on the\n"
	"       \"current\" skypiax_console (Skypiax) channel.\n" "       Enter 'help skypiax_console' on how to change the \"current\" skypiax_console\n";

char skypiax_console_skypiax_console_usage[] =
	"Usage: skypiax_console [interface] | [show]\n"
	"       If used without a parameter, displays which interface is the \"current\"\n"
	"       skypiax_console.  If a device is specified, the \"current\" skypiax_console is changed to\n"
	"       the interface specified.\n" "       If the parameter is \"show\", the available interfaces are listed\n";

char skypiax_console_skype_usage[] =
	"Usage: skypiax_skype [command string]\n"
	"       Send the 'command string' skype_msg to the Skype client connected to the  \"current\" skypiax_console (Skypiax) channel.\n"
	"       Enter 'help skypiax_console' on how to change the \"current\" skypiax_console\n";

char skypiax_console_skypiax_dir_import_usage[] =
	"Usage: skypiax_dir_import [add | replace]\n"
	"       Write in the directoriax.conf config file all the entries found in 'Contacts' list of the Skype client connected to the \"current\" skypiax_console.\n"
	"       You can choose between 'add' to the end of the directoriax.conf file, or 'replace' the whole file with this new content.\n"
	"       Enter 'help skypiax_console' on how to change the \"current\" skypiax_console\n";

/*! \brief Definition of this channel for PBX channel registration */
const struct ast_channel_tech skypiax_tech = {
	.type = skypiax_type,
	.description = skypiax_desc,
	.capabilities = AST_FORMAT_SLINEAR,
	.requester = skypiax_request,
	.hangup = skypiax_hangup,
	.answer = skypiax_answer,
	.read = skypiax_read,
	.call = skypiax_originate_call,
	.write = skypiax_write,
	.indicate = skypiax_indicate,
	.fixup = skypiax_fixup,
	.devicestate = skypiax_devicestate,
#ifdef ASTERISK_VERSION_1_4
	.send_digit_begin = skypiax_digitsend_begin,
	.send_digit_end = skypiax_digitsend_end,
#else /* ASTERISK_VERSION_1_4 */
	.send_digit = skypiax_digitsend,
#endif /* ASTERISK_VERSION_1_4 */
};

/*! \brief fake skypiax_pvt structure values, 
 * just for logging purposes */
struct skypiax_pvt skypiax_log_struct = {
	.name = "none",
};

/*! \brief Default skypiax_pvt structure values, 
 * used by skypiax_mkif to initialize the interfaces */
struct skypiax_pvt skypiax_default = {
	.interface_state = SKYPIAX_STATE_DOWN,
	.skype_callflow = 0,
	.context = "default",
	.language = "en",
	.exten = "s",
	.next = NULL,
	.owner = NULL,
	.controldev_thread = AST_PTHREADT_NULL,
	.skypiax_sound_rate = 8000,
	.skypiax_sound_capt_fd = -1,
	.capture_boost = 0,
	.playback_boost = 0,
	.stripmsd = 0,
	.skype = 0,
	.skypiax_dir_entry_extension_prefix = 6,
};

/*! 
 * \brief PVT structure for a skypiax interface (channel), created by skypiax_mkif
 */
struct skypiax_pvt *skypiax_iflist = NULL;

#ifdef ASTERISK_VERSION_1_6
struct ast_cli_entry myclis[] = {
/* 
 * CLI do not works since some time on 1.6, they changed the CLI mechanism
 */
#if 0
	AST_CLI_DEFINE(skypiax_console_hangup, "Hangup a call on the console"),
	AST_CLI_DEFINE(skypiax_console_dial, "Dial an extension on the console"),
	AST_CLI_DEFINE(skypiax_console_playback_boost, "Sets/displays spk boost in dB"),
	AST_CLI_DEFINE(skypiax_console_capture_boost, "Sets/displays mic boost in dB"),
	AST_CLI_DEFINE(skypiax_console_set_active, "Sets/displays active console"),
	AST_CLI_DEFINE(skypiax_console_skype, "Sends a Skype command"),
	AST_CLI_DEFINE(skypiax_console_skypiax_dir_import, "imports entries from cellphone"),
	AST_CLI_DEFINE(skypiax_console_skypiax, "all things skypiax"),
#endif
};
#else
struct ast_cli_entry myclis[] = {
	{{"skypiax_hangup", NULL}, skypiax_console_hangup,
	 "Hangup a call on the skypiax_console",
	 skypiax_console_hangup_usage},
	{{"skypiax_playback_boost", NULL}, skypiax_console_playback_boost, "playback boost",
	 skypiax_console_playback_boost_usage},
	{{"skypiax_capture_boost", NULL}, skypiax_console_capture_boost, "capture boost",
	 skypiax_console_capture_boost_usage},
	{{"skypiax_usage", NULL}, skypiax_console_skypiax, "chan_skypiax commands info",
	 skypiax_console_skypiax_usage},
	{{"skypiax_skype", NULL}, skypiax_console_skype, "Skype msg",
	 skypiax_console_skype_usage},
	{{"skypiax_dial", NULL}, skypiax_console_dial,
	 "Dial an extension on the skypiax_console",
	 skypiax_console_dial_usage},
	{{"skypiax_console", NULL}, skypiax_console_set_active,
	 "Sets/displays active skypiax_console",
	 skypiax_console_skypiax_console_usage},
	{{"skypiax_dir_import", NULL}, skypiax_console_skypiax_dir_import,
	 "Write the directoriax.conf file, used by directoriax app",
	 skypiax_console_skypiax_dir_import_usage},
};
#endif

/* IMPLEMENTATION */

void skypiax_unlocka_log(void *x)
{
	ast_mutex_t *y;
	y = x;
	int i;

	for (i = 0; i < 5; i++) {	//let's be generous

		ast_log(LOG_DEBUG,
				SKYPIAX_SVN_VERSION
				"[%-7lx] I'm a dying thread, and I'm to go unlocking mutex %p for the %dth time\n", (unsigned long int) pthread_self(), y, i);

		ast_mutex_unlock(y);
	}
	ast_log(LOG_DEBUG, SKYPIAX_SVN_VERSION "[%-7lx] I'm a dying thread, I've finished unlocking mutex %p\n", (unsigned long int) pthread_self(), y);
}

int skypiax_queue_control(struct ast_channel *c, int control)
{
	struct skypiax_pvt *p = c->tech_pvt;

/* queue the frame */
	if (p)
		p->control_to_send = control;
	else {
		return 0;
	}
	DEBUGA_PBX("Queued CONTROL FRAME %d\n", SKYPIAX_P_LOG, control);

/* wait for the frame to be sent */
	while (p->control_to_send)
		usleep(1);

	return 0;
}

int skypiax_devicestate(void *data)
{
	struct skypiax_pvt *p = NULL;
	char *name = data;
	int res = AST_DEVICE_INVALID;

	if (!data) {
		ERRORA("Devicestate requested with no data\n", SKYPIAX_P_LOG);
		return res;
	}

	/* lock the interfaces' list */
	LOKKA(&skypiax_iflock);
	/* make a pointer to the first interface in the interfaces list */
	p = skypiax_iflist;
	/* Search for the requested interface and verify if is unowned */
	while (p) {
		size_t length = strlen(p->name);
		/* is this the requested interface? */
		if (strncmp(name, p->name, length) == 0) {
			/* is this interface unowned? */
			if (!p->owner) {
				res = AST_DEVICE_NOT_INUSE;
				DEBUGA_PBX("Interface is NOT OWNED by a channel\n", SKYPIAX_P_LOG);
			} else {
				/* interface owned by a channel */
				res = AST_DEVICE_INUSE;
				DEBUGA_PBX("Interface is OWNED by a channel\n", SKYPIAX_P_LOG);
			}

			/* we found the requested interface, bail out from the while loop */
			break;
		}
		/* not yet found, next please */
		p = p->next;
	}
	/* unlock the interfaces' list */
	UNLOCKA(&skypiax_iflock);

	if (res == AST_DEVICE_INVALID) {
		ERRORA("Checking device state for interface [%s] returning AST_DEVICE_INVALID\n", SKYPIAX_P_LOG, name);
	}
	return res;
}

#ifndef ASTERISK_VERSION_1_4
int skypiax_indicate(struct ast_channel *c, int cond)
#else
int skypiax_indicate(struct ast_channel *c, int cond, const void *data, size_t datalen)
#endif
{
	struct skypiax_pvt *p = c->tech_pvt;
	int res = 0;

	NOTICA("Let's INDICATE %d\n", SKYPIAX_P_LOG, cond);

	switch (cond) {
	case AST_CONTROL_BUSY:
	case AST_CONTROL_CONGESTION:
	case AST_CONTROL_RINGING:
	case -1:
		res = -1;				/* Ask for inband indications */
		break;
	case AST_CONTROL_PROGRESS:
	case AST_CONTROL_PROCEEDING:
	case AST_CONTROL_VIDUPDATE:
	case AST_CONTROL_HOLD:
	case AST_CONTROL_UNHOLD:
#ifdef ASTERISK_VERSION_1_4
	case AST_CONTROL_SRCUPDATE:
#endif /* ASTERISK_VERSION_1_4 */
		break;
	default:
		WARNINGA("Don't know how to display condition %d on %s\n", SKYPIAX_P_LOG, cond, c->name);
		/* The core will play inband indications for us if appropriate */
		res = -1;
	}

	return res;
}

/*! \brief PBX interface function -build skypiax pvt structure 
 *         skypiax calls initiated by the PBX arrive here */
struct ast_channel *skypiax_request(const char *type, int format, void *data, int *cause)
{
	struct skypiax_pvt *p = NULL;
	struct ast_channel *tmp = NULL;
	char *name = data;
	int found = 0;

	DEBUGA_PBX("Try to request type: %s, name: %s, cause: %d," " format: %d\n", SKYPIAX_P_LOG, type, name, *cause, format);

	if (!data) {
		ERRORA("Channel requested with no data\n", SKYPIAX_P_LOG);
		return NULL;
	}

	char interface[256];
	int i;
	memset(interface, '\0', sizeof(interface));

	for (i = 0; i < sizeof(interface); i++) {
		if (name[i] == '/')
			break;
		interface[i] = name[i];
	}
	/* lock the interfaces' list */
	LOKKA(&skypiax_iflock);
	/* make a pointer to the first interface in the interfaces list */
	p = skypiax_iflist;

	if (strcmp("ANY", interface) == 0) {
		/* we've been asked for the "ANY" interface, let's find the first idle interface */
		DEBUGA_SKYPE("Finding one available skype interface\n", SKYPIAX_P_LOG);
		p = find_available_skypiax_interface();
		if (p) {
			found = 1;

			/* create a new channel owning this interface */
			tmp = skypiax_new(p, SKYPIAX_STATE_DOWN, p->context);
			if (!tmp) {
				/* the channel was not created, probable memory allocation error */
				*cause = AST_CAUSE_SWITCH_CONGESTION;
			}

		}

	}

	/* Search for the requested interface and verify if is unowned and format compatible */
	while (p && !found) {
		//size_t length = strlen(p->name);
		/* is this the requested interface? */
		if (strcmp(interface, p->name) == 0) {
			/* is the requested format supported by this interface? */
			if ((format & AST_FORMAT_SLINEAR) != 0) {
				/* is this interface unowned? */
				if (!p->owner) {
					DEBUGA_PBX("Requesting: %s, name: %s, format: %d\n", SKYPIAX_P_LOG, type, name, format);
					/* create a new channel owning this interface */
					tmp = skypiax_new(p, SKYPIAX_STATE_DOWN, p->context);
					if (!tmp) {
						/* the channel was not created, probable memory allocation error */
						*cause = AST_CAUSE_SWITCH_CONGESTION;
					}
				} else {
					/* interface owned by another channel */
					WARNINGA("owned by another channel\n", SKYPIAX_P_LOG);
					*cause = AST_CAUSE_REQUESTED_CHAN_UNAVAIL;
				}
			} else {
				/* requested format not supported */
				WARNINGA("format %d not supported\n", SKYPIAX_P_LOG, format);
				*cause = AST_CAUSE_BEARERCAPABILITY_NOTAVAIL;
			}
			/* we found the requested interface, bail out from the while loop */
			break;
		}
		/* not yet found, next please */
		p = p->next;
	}
	/* unlock the interfaces' list */
	UNLOCKA(&skypiax_iflock);
	/* restart the monitor so it will watch only the remaining unowned interfaces  */
	skypiax_restart_monitor();
	if (tmp == NULL) {
		/* new channel was not created */
		WARNINGA("Unable to create new Skypiax channel %s\n", SKYPIAX_P_LOG, name);
	}
	/* return the newly created channel */
	return tmp;
}

/*! \brief  Hangup skypiax call
 * Part of PBX interface, called from ast_hangup */

int skypiax_hangup(struct ast_channel *c)
{
	struct skypiax_pvt *p;

	/* get our skypiax pvt interface from channel */
	p = c->tech_pvt;
	/* if there is not skypiax pvt why we are here ? */
	if (!p) {
		ERRORA("Asked to hangup channel not connected\n", SKYPIAX_P_LOG);
		return 0;
	}

	if (p->skype && p->interface_state != SKYPIAX_STATE_DOWN) {
		char msg_to_skype[1024];
		p->interface_state = SKYPIAX_STATE_HANGUP_REQUESTED;
		DEBUGA_SKYPE("hanging up skype call: %s\n", SKYPIAX_P_LOG, p->skype_call_id);
		//sprintf(msg_to_skype, "SET CALL %s STATUS FINISHED", p->skype_call_id);
		sprintf(msg_to_skype, "ALTER CALL %s HANGUP", p->skype_call_id);
		skypiax_signaling_write(p, msg_to_skype);
	}

	while (p->interface_state != SKYPIAX_STATE_DOWN) {
		usleep(10000);
	}
	DEBUGA_SKYPE("Now is really DOWN\n", SKYPIAX_P_LOG);
	/* shutdown the serial monitoring thread */
	if (p->controldev_thread && (p->controldev_thread != AST_PTHREADT_NULL)
		&& (p->controldev_thread != AST_PTHREADT_STOP)) {
		if (pthread_cancel(p->controldev_thread)) {
			ERRORA("controldev_thread pthread_cancel failed, maybe he killed himself?\n", SKYPIAX_P_LOG);
		}
		/* push it, maybe is stuck in a select or so */
		if (pthread_kill(p->controldev_thread, SIGURG)) {
			DEBUGA_SERIAL("controldev_thread pthread_kill failed, no problem\n", SKYPIAX_P_LOG);
		}
#ifndef __CYGWIN__				/* under cygwin, this seems to be not reliable, get stuck at times */
		/* wait for it to die */
		if (pthread_join(p->controldev_thread, NULL)) {
			ERRORA("controldev_thread pthread_join failed, BAD\n", SKYPIAX_P_LOG);
		}
#else /* __CYGWIN__ */
/* allow the serial thread to die */
		usleep(300000);			//300msecs
#endif /* __CYGWIN__ */
	}
	p->controldev_thread = AST_PTHREADT_NULL;

	p->interface_state = SKYPIAX_STATE_DOWN;
	p->skype_callflow = CALLFLOW_CALL_IDLE;

	DEBUGA_PBX("I'll send AST_CONTROL_HANGUP\n", SKYPIAX_P_LOG);
	ast_queue_control(p->owner, AST_CONTROL_HANGUP);
	DEBUGA_PBX("I've sent AST_CONTROL_HANGUP\n", SKYPIAX_P_LOG);

	/* subtract one to the usage count of Skypiax-type channels */
	LOKKA(&skypiax_usecnt_lock);
	skypiax_usecnt--;
	if (skypiax_usecnt < 0)
		ERRORA("Usecnt < 0???\n", SKYPIAX_P_LOG);
	UNLOCKA(&skypiax_usecnt_lock);
	ast_update_use_count();

	/* our skypiax pvt interface is no more part of a channel */
	p->owner = NULL;
	/* our channel has no more this skypiax pvt interface to manage */
	c->tech_pvt = NULL;
	/* set the channel state to DOWN, eg. available, not in active use */
	if (ast_setstate(c, SKYPIAX_STATE_DOWN)) {
		ERRORA("ast_setstate failed, BAD\n", SKYPIAX_P_LOG);
		return -1;
	}

	/* restart the monitor thread, so it can recheck which interfaces it have to watch during its loop (the interfaces that are not owned by channels) */
	if (skypiax_restart_monitor()) {
		ERRORA("skypiax_restart_monitor failed, BAD\n", SKYPIAX_P_LOG);
		return -1;
	}

	return 0;
}

/*! \brief  Answer incoming call,
 * Part of PBX interface */
int skypiax_answer(struct ast_channel *c)
{
	struct skypiax_pvt *p = c->tech_pvt;
	int res;

	/* whle ringing, we just wait, the skype thread will answer */
	while (p->interface_state == SKYPIAX_STATE_RING) {
		usleep(10000);			//10msec
	}
	if (p->interface_state != SKYPIAX_STATE_UP) {
		ERRORA("call answering failed, we want it to be into interface_state=%d, got %d\n", SKYPIAX_P_LOG, SKYPIAX_STATE_UP, p->interface_state);
		res = -1;
	} else {
		DEBUGA_PBX("call answered\n", SKYPIAX_P_LOG);
		res = 0;
	}
	return res;
}

#ifdef ASTERISK_VERSION_1_4
int skypiax_digitsend_begin(struct ast_channel *c, char digit)
{
	struct skypiax_pvt *p = c->tech_pvt;

	DEBUGA_PBX("DIGIT BEGIN received: %c\n", SKYPIAX_P_LOG, digit);

	return 0;
}

int skypiax_digitsend_end(struct ast_channel *c, char digit, unsigned int duration)
{
	struct skypiax_pvt *p = c->tech_pvt;
	char msg_to_skype[1024];

	NOTICA("DIGIT END received: %c %d\n", SKYPIAX_P_LOG, digit, duration);

	sprintf(msg_to_skype, "SET CALL %s DTMF %c", p->skype_call_id, digit);

	skypiax_signaling_write(p, msg_to_skype);

	return 0;
}
#else /* ASTERISK_VERSION_1_4 */
int skypiax_digitsend(struct ast_channel *c, char digit)
{
	struct skypiax_pvt *p = c->tech_pvt;
	char msg_to_skype[1024];

	NOTICA("DIGIT received: %c\n", SKYPIAX_P_LOG, digit);

	sprintf(msg_to_skype, "SET CALL %s DTMF %c", p->skype_call_id, digit);

	skypiax_signaling_write(p, msg_to_skype);

	return 0;
}

#endif /* ASTERISK_VERSION_1_4 */
//struct ast_frame *skypiax_audio_read(struct skypiax_pvt *p)
//#define SAMPLES_PER_FRAME 160
/*! \brief Read audio frames from channel */
struct ast_frame *skypiax_read(struct ast_channel *c)
{
	struct skypiax_pvt *p = c->tech_pvt;
	static struct ast_frame f;
	static short __buf[SKYPIAX_FRAME_SIZE + AST_FRIENDLY_OFFSET / 2];
	short *buf;
	int samples;

/* if there are control frames queued to be sent by skypiax_queue_control, send it the first */
//TODO maybe better a real queue?
	if (p && p->owner && p->control_to_send) {
		ast_queue_control(p->owner, p->control_to_send);
		DEBUGA_PBX("Sent CONTROL FRAME %d\n", SKYPIAX_P_LOG, p->control_to_send);
		p->control_to_send = 0;
	}

	memset(__buf, '\0', (SKYPIAX_FRAME_SIZE + AST_FRIENDLY_OFFSET / 2));

	buf = __buf + AST_FRIENDLY_OFFSET / 2;

	f.frametype = AST_FRAME_NULL;
	f.subclass = 0;
	f.samples = 0;
	f.datalen = 0;
	f.data = NULL;
	f.offset = 0;
	f.src = skypiax_type;
	f.mallocd = 0;
	f.delivery.tv_sec = 0;
	f.delivery.tv_usec = 0;

/* if the call is not active (ie: answered), do not send audio frames, they would pile up in a lag queue */
	if (p->owner && p->owner->_state != SKYPIAX_STATE_UP) {
		return &f;
	}

	if ((samples = read(p->audiopipe[0], buf, SAMPLES_PER_FRAME * sizeof(short))) != 320) {
		DEBUGA_SOUND("read=====> NOT GOOD samples=%d expected=%d\n", SKYPIAX_P_LOG, samples, SAMPLES_PER_FRAME * sizeof(short));
		usleep(100);
		//do nothing
	} else {
		//DEBUGA_SOUND("read=====> GOOD samples=%d\n", SKYPIAX_P_LOG, samples);
		/* A real frame */
		f.frametype = AST_FRAME_VOICE;
		f.subclass = AST_FORMAT_SLINEAR;
		f.samples = SKYPIAX_FRAME_SIZE;
		f.datalen = SKYPIAX_FRAME_SIZE * 2;
		f.data = buf;
		f.offset = AST_FRIENDLY_OFFSET;
		f.src = skypiax_type;
		f.mallocd = 0;

		if (p->capture_boost)
			skypiax_sound_boost(&f, p->capture_boost);
	}

	return &f;
}

/*! \brief Initiate skypiax call from PBX 
 * used from the dial() application
 */
int skypiax_originate_call(struct ast_channel *c, char *idest, int timeout)
{
	struct skypiax_pvt *p = NULL;
	p = c->tech_pvt;
	char rdest[80], *where, dstr[100] = "";
	char *stringp = NULL;
	int status;

	if ((c->_state != SKYPIAX_STATE_DOWN)
		&& (c->_state != SKYPIAX_STATE_RESERVED)) {
		ERRORA("skypiax_originate_call called on %s, neither down nor reserved\n", SKYPIAX_P_LOG, c->name);
		return -1;
	}

	DEBUGA_PBX("skypiax_originate_call to call idest: %s, timeout: %d!\n", SKYPIAX_P_LOG, idest, timeout);

	strncpy(rdest, idest, sizeof(rdest) - 1);
	stringp = rdest;
	strsep(&stringp, "/");
	where = strsep(&stringp, "/");
	if (!where) {
		ERRORA("Destination %s requires an actual destination (Skypiax/device/destination)\n", SKYPIAX_P_LOG, idest);
		return -1;
	}

	strncpy(dstr, where + p->stripmsd, sizeof(dstr) - 1);
	DEBUGA_PBX("skypiax_originate_call dialing idest: %s, timeout: %d, dstr: %s!\n", SKYPIAX_P_LOG, idest, timeout, dstr);

	strcpy(p->session_uuid_str, "dialing");
	status = skypiax_call(p, dstr, timeout);
	if (status) {
		WARNINGA("skypiax_originate_call dialing failed: %d!\n", SKYPIAX_P_LOG, status);
		return -1;
	}

	DEBUGA_PBX("skypiax_originate_call dialed idest: %s, timeout: %d, dstr: %s!\n", SKYPIAX_P_LOG, idest, timeout, dstr);

	ast_setstate(p->owner, SKYPIAX_STATE_DIALING);
	return 0;
}

int skypiax_sound_boost(struct ast_frame *f, double boost)
{
/* LUIGI RIZZO's magic */
	if (boost != 0) {			/* scale and clip values */
		int i, x;
		int16_t *ptr = (int16_t *) f->data;
		for (i = 0; i < f->samples; i++) {
			x = (ptr[i] * boost) / BOOST_SCALE;
			if (x > 32767) {
				x = 32767;
			} else if (x < -32768) {
				x = -32768;
			}
			ptr[i] = x;
		}
	}
	return 0;
}

/*! \brief Send audio frame to channel */
int skypiax_write(struct ast_channel *c, struct ast_frame *f)
{
	struct skypiax_pvt *p = c->tech_pvt;
	int sent;

	if (p->owner && p->owner->_state != SKYPIAX_STATE_UP) {
		return 0;
	}
	if (p->playback_boost)
		skypiax_sound_boost(f, p->playback_boost);

	sent = write(p->audioskypepipe[1], (short *) f->data, f->datalen);
	//skypiax_sound_write(p, f);

	return 0;
}

/*! \brief  Fix up a channel:  If a channel is consumed, this is called.
 * Basically update any ->owner links */
int skypiax_fixup(struct ast_channel *oldchan, struct ast_channel *newchan)
{
	struct skypiax_pvt *p = newchan->tech_pvt;

	if (!p) {
		ERRORA("No pvt after masquerade. Strange things may happen\n", SKYPIAX_P_LOG);
		return -1;
	}

	if (p->owner != oldchan) {
		ERRORA("old channel wasn't %p but was %p\n", SKYPIAX_P_LOG, oldchan, p->owner);
		return -1;
	}

	p->owner = newchan;
	return 0;
}

struct ast_channel *skypiax_new(struct skypiax_pvt *p, int state, char *context)
{
	struct ast_channel *tmp;

	/* alloc a generic channel struct */
#ifndef ASTERISK_VERSION_1_4
	tmp = ast_channel_alloc(1);
#else
	//tmp = ast_channel_alloc(1, state, 0, 0, "", p->exten, p->context, 0, "");
	//tmp = ast_channel_alloc(1, state, i->cid_num, i->cid_name, i->accountcode, i->exten, i->context, i->amaflags, "Skypiax/%s", p->name);
	tmp = ast_channel_alloc(1, state, 0, 0, "", p->exten, p->context, 0, "Skypiax/%s", p->name);

#endif /* ASTERISK_VERSION_1_4 */
	if (tmp) {

		/* give a name to the newly created channel */
#ifndef ASTERISK_VERSION_1_4
		snprintf(tmp->name, sizeof(tmp->name), "Skypiax/%s", p->name);
		tmp->type = skypiax_type;
#else /* ASTERISK_VERSION_1_4 */
		ast_string_field_build(tmp, name, "Skypiax/%s", p->name);
#endif /* ASTERISK_VERSION_1_4 */

		DEBUGA_PBX("new channel: name=%s requested_state=%d\n", SKYPIAX_P_LOG, tmp->name, state);

		/* fd for the channel to poll for incoming audio */
		tmp->fds[0] = p->skypiax_sound_capt_fd;

		/* audio formats managed */
		tmp->nativeformats = AST_FORMAT_SLINEAR;
		tmp->readformat = AST_FORMAT_SLINEAR;
		tmp->writeformat = AST_FORMAT_SLINEAR;
		/* the technology description (eg. the interface type) of the newly created channel is the Skypiax's one */
		tmp->tech = &skypiax_tech;
		/* the technology pvt (eg. the interface) of the newly created channel is this interface pvt */
		tmp->tech_pvt = p;

		/* copy this interface default context, extension, language to the newly created channel */
		if (strlen(p->context))
			strncpy(tmp->context, p->context, sizeof(tmp->context) - 1);
		if (strlen(p->exten))
			strncpy(tmp->exten, p->exten, sizeof(tmp->exten) - 1);
#ifndef ASTERISK_VERSION_1_4
		if (strlen(p->language))
			strncpy(tmp->language, p->language, sizeof(tmp->language) - 1);
#else
		if (strlen(p->language))
			ast_string_field_set(tmp, language, p->language);
#endif /* ASTERISK_VERSION_1_4 */
		/* copy the requested context (not necessarily the interface default) to the newly created channel */
		if (strlen(context))
			strncpy(tmp->context, context, sizeof(tmp->context) - 1);

		/* copy this interface default callerid in the newly created channel */
		ast_set_callerid(tmp, !ast_strlen_zero(p->callid_number) ? p->callid_number : NULL,
						 !ast_strlen_zero(p->callid_name) ? p->callid_name : NULL, !ast_strlen_zero(p->callid_number) ? p->callid_number : NULL);

		DEBUGA_PBX("callid_number=%s, callid_name=%s\n", SKYPIAX_P_LOG, p->callid_number, p->callid_name);

		/* the owner of this interface pvt is the newly created channel */
		p->owner = tmp;
		/* set the newly created channel state to the requested state */
		if (ast_setstate(tmp, state)) {
			ERRORA("ast_setstate failed, BAD\n", SKYPIAX_P_LOG);
			//ast_dsp_free(p->dsp);
			ast_channel_free(tmp);
			return NULL;
		}
		/* if the requested state is different from DOWN, let the pbx manage this interface (now part of the newly created channel) */
		if (state != SKYPIAX_STATE_DOWN) {
			DEBUGA_PBX("Try to start PBX on %s, state=%d\n", SKYPIAX_P_LOG, tmp->name, state);
			if (ast_pbx_start(tmp)) {
				ERRORA("Unable to start PBX on %s\n", SKYPIAX_P_LOG, tmp->name);
				ast_channel_free(tmp);
				return NULL;
			}
		}
		/* let's start the serial monitoring thread too, so we can have serial signaling */
		if (ast_pthread_create(&p->controldev_thread, NULL, skypiax_do_controldev_thread, p) < 0) {
			ERRORA("Unable to start controldev thread.\n", SKYPIAX_P_LOG);
			ast_channel_free(tmp);
			tmp = NULL;
		}
		DEBUGA_SERIAL("STARTED controldev_thread=%lu STOP=%lu NULL=%lu\n", SKYPIAX_P_LOG,
					  (unsigned long) p->controldev_thread, (unsigned long) AST_PTHREADT_STOP, (unsigned long) AST_PTHREADT_NULL);

		/* add one to the usage count of Skypiax-type channels */
		LOKKA(&skypiax_usecnt_lock);
		skypiax_usecnt++;
		UNLOCKA(&skypiax_usecnt_lock);
		ast_update_use_count();

		/* return the newly created channel */
		return tmp;
	}
	ERRORA("failed memory allocation for Skypiax channel\n", SKYPIAX_P_LOG);
	return NULL;
}

/*!
 * \brief Load the module into Asterisk and start its threads
 *
 * This function register the module into Asterisk,
 * create the interfaces for the channels, 
 * start the auxiliary threads for the interfaces,
 * then start a monitor thread. The monitor thread
 * will signal Asterisk when an interface receive a call.
 *
 *
 * \return zero on success, -1 on error.
 */
int load_module(void)
{
	int i;
	struct ast_config *cfg;
	struct skypiax_pvt *tmp;
	struct skypiax_pvt *p = NULL;
	struct skypiax_pvt *p2 = NULL;
#ifdef ASTERISK_VERSION_1_6
	struct ast_flags config_flags = { 0 };
#endif /* ASTERISK_VERSION_1_6 */

#if defined(WANT_SKYPE_X11) || defined(__CYGWIN__)
#ifndef __CYGWIN__
	if (!XInitThreads())
		ast_log(LOG_ERROR, "Not initialized XInitThreads!\n");
#endif /* __CYGWIN__ */
#if 0
	ast_register_atexit(skypiax_disconnect);
	ast_register_application(skype2skypiaxapp, skype2skypiax, skype2skypiaxsynopsis, skype2skypiaxdescrip);
	ast_register_application(skypiax2skypeapp, skypiax2skype, skypiax2skypesynopsis, skypiax2skypedescrip);
#endif
#endif /* defined(WANT_SKYPE_X11) || defined(__CYGWIN__) */

	/* make sure we can register our channel type with Asterisk */
	i = ast_channel_register(&skypiax_tech);
	if (i < 0) {
		ERRORA("Unable to register channel type '%s'\n", SKYPIAX_P_LOG, skypiax_type);
		return -1;
	}
	/* load skypiax.conf config file */
#ifdef ASTERISK_VERSION_1_6
	cfg = ast_config_load(skypiax_config, config_flags);
#else
	cfg = ast_config_load(skypiax_config);
#endif /* ASTERISK_VERSION_1_6 */
	if (cfg != NULL) {
		char *ctg = NULL;
		int is_first_category = 1;
		while ((ctg = ast_category_browse(cfg, ctg)) != NULL) {
			/* create one interface for each category in skypiax.conf config file, first one set the defaults */
			tmp = skypiax_mkif(cfg, ctg, is_first_category);
			if (tmp) {
				DEBUGA_PBX("Created channel Skypiax: skypiax.conf category '[%s]', channel name '%s'" "\n", SKYPIAX_P_LOG, ctg, tmp->name);
				/* add interface to skypiax_iflist */
				tmp->next = skypiax_iflist;
				skypiax_iflist = tmp;
				/* next one will not be the first ;) */
				if (is_first_category == 1) {
					is_first_category = 0;
					skypiax_console_active = tmp->name;
				}
			} else {
				ERRORA("Unable to create channel Skypiax from skypiax.conf category '[%s]'\n", SKYPIAX_P_LOG, ctg);
				/* if error, unload config from memory and return */
				ast_config_destroy(cfg);
				ast_channel_unregister(&skypiax_tech);
				return -1;
			}
			/* do it for each category described in config */
		}

		/* we finished, unload config from memory */
		ast_config_destroy(cfg);
	} else {
		ERRORA("Unable to load skypiax_config skypiax.conf\n", SKYPIAX_P_LOG);
		ast_channel_unregister(&skypiax_tech);
		return -1;
	}
#ifndef ASTERISK_VERSION_1_6
	ast_cli_register_multiple(myclis, sizeof(myclis) / sizeof(struct ast_cli_entry));
#endif /* ASTERISK_VERSION_1_6 */
	/* start to monitor the interfaces (skypiax_iflist) for the first time */
	if (skypiax_restart_monitor()) {
		ERRORA("skypiax_restart_monitor failed, BAD\n", SKYPIAX_P_LOG);
		return -1;
	}
	/* go through the interfaces list (skypiax_iflist) WITHOUT locking */
	p = skypiax_iflist;
	while (p) {
		int i;
		/* for each interface in list */
		p2 = p->next;
		NOTICA("STARTING interface %s, please be patient\n", SKYPIAX_P_LOG, p->name);
		i = 0;
		while (p->SkypiaxHandles.api_connected == 0 && running && i < 60000) {	// 60sec FIXME
			usleep(1000);
			i++;
		}
		if (p->SkypiaxHandles.api_connected) {
			NOTICA("STARTED interface %s\n", SKYPIAX_P_LOG, p->name);
		} else {
			ERRORA("Interface %s FAILED to start\n", SKYPIAX_P_LOG, p->name);
			running = 0;
			return -1;
		}
		/* next one, please */
		p = p2;
	}
	return 0;
}

/*!
 * \brief Unload the module from Asterisk and shutdown its threads
 *
 * This function unregister the module from Asterisk,
 * destroy the interfaces for the channels, 
 * shutdown the auxiliary threads for the interfaces,
 * then shutdown its monitor thread.
 *
 * \return zero on success, -1 on error.
 */
int unload_module(void)
{
	struct skypiax_pvt *p = NULL, *p2 = NULL;
	int res;

	/* unregister our channel type with Asterisk */
	ast_channel_unregister(&skypiax_tech);
	ast_cli_unregister_multiple(myclis, sizeof(myclis) / sizeof(struct ast_cli_entry));

#if defined(WANT_SKYPE_X11) || defined(__CYGWIN__)
#ifndef __CYGWIN__
	//FIXME what to do? if (!XInitThreads())
	//FIXME what to do? ast_log(LOG_ERROR, "Not initialized XInitThreads!\n");
#endif /* __CYGWIN__ */
#if 0
	ast_unregister_atexit(skypiax_disconnect);
	ast_unregister_application(skype2skypiaxapp);
	ast_unregister_application(skypiax2skypeapp);
#endif
#endif /* defined(WANT_SKYPE_X11) || defined(__CYGWIN__) */

	/* lock the skypiax_monlock, kill the monitor thread, unlock the skypiax_monlock */
	LOKKA(&skypiax_monlock);
	if (skypiax_monitor_thread && (skypiax_monitor_thread != AST_PTHREADT_NULL)
		&& (skypiax_monitor_thread != AST_PTHREADT_STOP)) {
		if (pthread_cancel(skypiax_monitor_thread)) {
			ERRORA("pthread_cancel failed, BAD\n", SKYPIAX_P_LOG);
			return -1;
		}
		if (pthread_kill(skypiax_monitor_thread, SIGURG)) {
			DEBUGA_PBX("pthread_kill failed\n", SKYPIAX_P_LOG);	//maybe it just died
		}
#ifndef __CYGWIN__				/* under cygwin, this seems to be not reliable, get stuck at times */
		if (pthread_join(skypiax_monitor_thread, NULL)) {
			ERRORA("pthread_join failed, BAD\n", SKYPIAX_P_LOG);
			return -1;
		}
#endif /* __CYGWIN__ */
	}
	skypiax_monitor_thread = AST_PTHREADT_STOP;
	UNLOCKA(&skypiax_monlock);

	if (skypiax_monitor_audio_thread && (skypiax_monitor_audio_thread != AST_PTHREADT_NULL)
		&& (skypiax_monitor_audio_thread != AST_PTHREADT_STOP)) {

		if (pthread_cancel(skypiax_monitor_audio_thread)) {
			ERRORA("pthread_cancel skypiax_monitor_audio_thread failed, BAD\n", SKYPIAX_P_LOG);
		}
		if (pthread_kill(skypiax_monitor_audio_thread, SIGURG)) {
			DEBUGA_PBX("pthread_kill skypiax_monitor_audio_thread failed, no problem\n", SKYPIAX_P_LOG);	//maybe it just died
		}

		if (pthread_join(skypiax_monitor_audio_thread, NULL)) {
			ERRORA("pthread_join failed, BAD\n", SKYPIAX_P_LOG);
		}
	}
	/* lock the skypiax_iflock, and go through the interfaces list (skypiax_iflist) */
	LOKKA(&skypiax_iflock);
	p = skypiax_iflist;
	while (p) {
		/* for each interface in list */
		p2 = p->next;
		/* shutdown the sound system, close sound fds, and if exist shutdown the sound managing threads */
		DEBUGA_SOUND("shutting down sound\n", SKYPIAX_P_LOG);
		res = skypiax_sound_shutdown(p);
		if (res == -1) {
			ERRORA("Failed to shutdown sound\n", SKYPIAX_P_LOG);
		}
#if 0
		/* if a dsp struct has been allocated, free it */
		if (p->dsp) {
			ast_dsp_free(p->dsp);
			p->dsp = NULL;
		}
#endif
		DEBUGA_PBX("freeing PVT\n", SKYPIAX_P_LOG);
		/* free the pvt allocated memory */
		free(p);
		/* next one, please */
		p = p2;
	}
	/* finished with the interfaces list, unlock the skypiax_iflock */
	UNLOCKA(&skypiax_iflock);

#ifdef __CYGWIN__
	NOTICA("Sleping 5 secs, please wait...\n", SKYPIAX_P_LOG);
	sleep(5);					/* without this pause, for some unknown (to me) reason it crashes on cygwin */
#endif /* __CYGWIN__ */
	NOTICA("Unloaded Skypiax Module\n", SKYPIAX_P_LOG);
	return 0;
}

/*!
 * \brief Return the count of active channels for this module
 *
 * \return the count of active channels for this module
 */
int usecount()
{
	int res;
	static struct skypiax_pvt *p = &skypiax_log_struct;
/* lock the skypiax_usecnt lock */
	LOKKA(&skypiax_usecnt_lock);
	/* retrieve the skypiax_usecnt */
	res = skypiax_usecnt;
/* unlock the skypiax_usecnt lock */
	UNLOCKA(&skypiax_usecnt_lock);
	/* return the skypiax_usecnt */
	return res;
}

/*!
 * \brief Return the textual description of the module
 *
 * \return the textual description of the module
 */
char *description()
{
	return (char *) skypiax_desc;
}

/*!
 * \brief Return the ASTERISK_GPL_KEY
 *
 * \return the ASTERISK_GPL_KEY
 */
char *key()
{
	struct skypiax_pvt *p = NULL;

	NOTICA("Returning Key\n", SKYPIAX_P_LOG);

	return ASTERISK_GPL_KEY;
}

/*!
 * \brief Create and initialize one interface for the module
 * \param cfg pointer to configuration data from skypiax.conf
 * \param ctg pointer to a category name to be found in cfg
 * \param is_first_category is this the first category in cfg
 *
 * This function create and initialize one interface for the module
 *
 * \return a pointer to the PVT structure of interface on success, NULL on error.
 */
struct skypiax_pvt *skypiax_mkif(struct ast_config *cfg, char *ctg, int is_first_category)
{
	struct skypiax_pvt *tmp;
	struct ast_variable *v;
	int res;

	int debug_all = 0;
	int debug_at = 0;
	int debug_fbus2 = 0;
	int debug_serial = 0;
	int debug_sound = 0;
	int debug_pbx = 0;
	int debug_skype = 0;
	int debug_call = 0;
	int debug_locks = 0;
	int debug_monitorlocks = 0;

	ast_log(LOG_DEBUG, "ENTERING FUNC\n");
	/* alloc memory for PVT */
	tmp = malloc(sizeof(struct skypiax_pvt));
	if (tmp == NULL) {			/* fail */
		return NULL;
	}
	/* clear memory for PVT */
	memset(tmp, 0, sizeof(struct skypiax_pvt));

	/* if we are reading the "first" category of the config file, take SELECTED values as defaults, overriding the values in skypiax_default */
	if (is_first_category == 1) {
		/* for each variable in category, copy it in the skypiax_default struct */
		for (v = ast_variable_browse(cfg, ctg); v; v = v->next) {
			M_START(v->name, v->value);

			M_STR("context", skypiax_default.context)
				M_STR("language", skypiax_default.language)
				M_STR("extension", skypiax_default.exten)
				M_F("playback_boost", skypiax_store_boost(v->value, &skypiax_default.playback_boost))
				M_F("capture_boost", skypiax_store_boost(v->value, &skypiax_default.capture_boost))
				M_UINT("skypiax_dir_entry_extension_prefix", skypiax_default.skypiax_dir_entry_extension_prefix)
				M_END(;
				);
		}
	}

	/* initialize the newly created PVT from the skypiax_default values */
	*tmp = skypiax_default;

	/* the category name becomes the interface name */
	tmp->name = strdup(ctg);

	/* for each category in config file, "first" included, read in ALL the values */
	for (v = ast_variable_browse(cfg, ctg); v; v = v->next) {
		M_START(v->name, v->value);

		M_BOOL("debug_all", debug_all)
			M_BOOL("debug_at", debug_at)
			M_BOOL("debug_fbus2", debug_fbus2)
			M_BOOL("debug_serial", debug_serial)
			M_BOOL("debug_sound", debug_sound)
			M_BOOL("debug_pbx", debug_pbx)
			M_BOOL("debug_skype", debug_skype)
			M_BOOL("debug_call", debug_call)
			M_BOOL("debug_locks", debug_locks)
			M_BOOL("debug_monitorlocks", debug_monitorlocks)
			M_BOOL("skype", tmp->skype)
			M_STR("context", tmp->context)
			M_STR("language", tmp->language)
			M_STR("extension", tmp->exten)
			M_STR("X11_display", tmp->X11_display)
			M_UINT("tcp_cli_port", tmp->tcp_cli_port)
			M_UINT("tcp_srv_port", tmp->tcp_srv_port)
			M_F("playback_boost", skypiax_store_boost(v->value, &tmp->playback_boost))
			M_F("capture_boost", skypiax_store_boost(v->value, &tmp->capture_boost))
			M_STR("skype_user", tmp->skype_user)
			M_UINT("skypiax_dir_entry_extension_prefix", tmp->skypiax_dir_entry_extension_prefix)
			M_END(;
			);
	}

	if (debug_all) {
		skypiax_debug = skypiax_debug | DEBUG_ALL;
		if (!option_debug) {
			WARNINGA
				("DEBUG_ALL activated, but option_debug is 0. You have to set debug level higher than zero to see some debugging output. Please use the command \"set debug 10\" or start Asterisk with \"-dddddddddd\" option for full DEBUG_ALL debugging output.\n",
				 SKYPIAX_TMP_LOG);
		} else {
			NOTICA("DEBUG_ALL activated. \n", SKYPIAX_TMP_LOG);
		}
	}

	if (debug_fbus2) {
		skypiax_debug = skypiax_debug | DEBUG_FBUS2;
		if (!option_debug) {
			WARNINGA
				("DEBUG_FBUS2 activated, but option_debug is 0. You have to set debug level higher than zero to see some debugging output. Please use the command \"set debug 10\" or start Asterisk with \"-dddddddddd\" option for full DEBUG_FBUS2 debugging output.\n",
				 SKYPIAX_TMP_LOG);
		} else {
			NOTICA("DEBUG_FBUS2 activated. \n", SKYPIAX_TMP_LOG);
		}
	}

	if (debug_serial) {
		skypiax_debug = skypiax_debug | DEBUG_SERIAL;
		if (!option_debug) {
			WARNINGA
				("DEBUG_SERIAL activated, but option_debug is 0. You have to set debug level higher than zero to see some debugging output. Please use the command \"set debug 10\" or start Asterisk with \"-dddddddddd\" option for full DEBUG_SERIAL debugging output.\n",
				 SKYPIAX_TMP_LOG);
		} else {
			NOTICA("DEBUG_SERIAL activated. \n", SKYPIAX_TMP_LOG);
		}
	}

	if (debug_sound) {
		skypiax_debug = skypiax_debug | DEBUG_SOUND;
		if (!option_debug) {
			WARNINGA
				("DEBUG_SOUND activated, but option_debug is 0. You have to set debug level higher than zero to see some debugging output. Please use the command \"set debug 10\" or start Asterisk with \"-dddddddddd\" option for full DEBUG_SOUND debugging output.\n",
				 SKYPIAX_TMP_LOG);
		} else {
			NOTICA("DEBUG_SOUND activated. \n", SKYPIAX_TMP_LOG);
		}
	}

	if (debug_pbx) {
		skypiax_debug = skypiax_debug | DEBUG_PBX;
		if (!option_debug) {
			WARNINGA
				("DEBUG_PBX activated, but option_debug is 0. You have to set debug level higher than zero to see some debugging output. Please use the command \"set debug 10\" or start Asterisk with \"-dddddddddd\" option for full DEBUG_PBX debugging output.\n",
				 SKYPIAX_TMP_LOG);
		} else {
			NOTICA("DEBUG_PBX activated. \n", SKYPIAX_TMP_LOG);
		}
	}

	if (debug_skype) {
		skypiax_debug = skypiax_debug | DEBUG_SKYPE;
		if (!option_debug) {
			WARNINGA
				("DEBUG_SKYPE activated, but option_debug is 0. You have to set debug level higher than zero to see some debugging output. Please use the command \"set debug 10\" or start Asterisk with \"-dddddddddd\" option for full DEBUG_SKYPE debugging output.\n",
				 SKYPIAX_TMP_LOG);
		} else {
			NOTICA("DEBUG_SKYPE activated. \n", SKYPIAX_TMP_LOG);
		}
	}

	if (debug_call) {
		skypiax_debug = skypiax_debug | DEBUG_CALL;
		if (!option_debug) {
			WARNINGA
				("DEBUG_CALL activated, but option_debug is 0. You have to set debug level higher than zero to see some debugging output. Please use the command \"set debug 10\" or start Asterisk with \"-dddddddddd\" option for full DEBUG_CALL debugging output.\n",
				 SKYPIAX_TMP_LOG);
		} else {
			NOTICA("DEBUG_CALL activated. \n", SKYPIAX_TMP_LOG);
		}
	}

	if (debug_locks) {
		skypiax_debug = skypiax_debug | DEBUG_LOCKS;
		if (!option_debug) {
			WARNINGA
				("DEBUG_LOCKS activated, but option_debug is 0. You have to set debug level higher than zero to see some debugging output. Please use the command \"set debug 10\" or start Asterisk with \"-dddddddddd\" option for full DEBUG_LOCKS debugging output.\n",
				 SKYPIAX_TMP_LOG);
		} else {
			NOTICA("DEBUG_LOCKS activated. \n", SKYPIAX_TMP_LOG);
		}
	}

	if (debug_monitorlocks) {
		skypiax_debug = skypiax_debug | DEBUG_MONITORLOCKS;
		if (!option_debug) {
			WARNINGA
				("DEBUG_MONITORLOCKS activated, but option_debug is 0. You have to set debug level higher than zero to see some debugging output. Please use the command \"set debug 10\" or start Asterisk with \"-dddddddddd\" option for full DEBUG_MONITORLOCKS debugging output.\n",
				 SKYPIAX_TMP_LOG);
		} else {
			NOTICA("DEBUG_MONITORLOCKS activated. \n", SKYPIAX_TMP_LOG);
		}
	}

	if (option_debug > 1) {
		DEBUGA_SOUND("playback_boost is %f\n", SKYPIAX_TMP_LOG, tmp->playback_boost);
		DEBUGA_SOUND("capture_boost is %f\n", SKYPIAX_TMP_LOG, tmp->capture_boost);
	}
/* initialize the soundcard channels (input and output) used by this interface (a multichannel soundcard can be used by multiple interfaces), optionally starting the sound managing threads */
	res = skypiax_sound_init(tmp);
	if (res == -1) {
		ERRORA("Failed initializing sound device\n", SKYPIAX_TMP_LOG);
		/* we failed, free the PVT */
		free(tmp);
		return NULL;
	}
	/*
	   res = pipe(tmp->SkypiaxHandles.fdesc);
	   if (res) {
	   ast_log(LOG_ERROR, "Unable to create skype pipe\n");
	   if (option_debug > 10) {
	   DEBUGA_PBX("EXITING FUNC\n", SKYPIAX_TMP_LOG);
	   }
	   free(tmp);
	   return NULL;
	   }
	   fcntl(tmp->SkypiaxHandles.fdesc[0], F_SETFL, O_NONBLOCK);
	   fcntl(tmp->SkypiaxHandles.fdesc[1], F_SETFL, O_NONBLOCK);
	 */
	tmp->skype_thread = AST_PTHREADT_NULL;

	if (tmp->skype) {
		ast_log(LOG_DEBUG, "TO BE started skype_thread=%lu STOP=%lu NULL=%lu\n",
				(unsigned long) tmp->skype_thread, (unsigned long) AST_PTHREADT_STOP, (unsigned long) AST_PTHREADT_NULL);
#ifdef __CYGWIN__
		if (ast_pthread_create(&tmp->skype_thread, NULL, do_skypeapi_thread, tmp) < 0) {
			ast_log(LOG_ERROR, "Unable to start skype_main thread.\n");
			free(tmp);
			return NULL;
		}
#else /* __CYGWIN__ */
#ifdef WANT_SKYPE_X11
		ast_log(LOG_DEBUG, "AsteriskHandlesfd: %d\n", tmp->SkypiaxHandles.fdesc[1]);
		if (ast_pthread_create(&tmp->skype_thread, NULL, do_skypeapi_thread, tmp) < 0) {
			ast_log(LOG_ERROR, "Unable to start skype_main thread.\n");
			free(tmp);
			return NULL;
		}
#endif /* WANT_SKYPE_X11 */
#endif /* __CYGWIN__ */
		usleep(100000);			//0.1 sec
		if (tmp->skype_thread == AST_PTHREADT_NULL) {
			ast_log(LOG_ERROR, "Unable to start skype_main thread.\n");
			free(tmp);
			return NULL;
		}
		ast_log(LOG_DEBUG, "STARTED skype_thread=%lu STOP=%lu NULL=%lu\n",
				(unsigned long) tmp->skype_thread, (unsigned long) AST_PTHREADT_STOP, (unsigned long) AST_PTHREADT_NULL);
	}
#if 0
	if (tmp->skype) {
#if 0
		if (option_debug > 1)
			ast_log(LOG_DEBUG, "TO BE started skype_thread=%lu STOP=%lu NULL=%lu\n",
					(unsigned long) tmp->skype_thread, (unsigned long) AST_PTHREADT_STOP, (unsigned long) AST_PTHREADT_NULL);
#endif
#ifdef __CYGWIN__
		if (ast_pthread_create(&tmp->skype_thread, NULL, do_skypeapi_thread, &tmp->SkypiaxHandles) < 0) {
			ast_log(LOG_ERROR, "Unable to start skype_main thread.\n");
			if (option_debug > 10) {
				DEBUGA_PBX("EXITING FUNC\n", SKYPIAX_TMP_LOG);
			}
			free(tmp);
			return NULL;
		}
#else /* __CYGWIN__ */
#ifdef WANT_SKYPE_X11
		if (ast_pthread_create(&tmp->signaling_thread, NULL, do_signaling_thread_fnc, tmp) < 0) {
			ast_log(LOG_ERROR, "Unable to start skype_main thread.\n");
			if (option_debug > 10) {
				DEBUGA_PBX("EXITING FUNC\n", SKYPIAX_TMP_LOG);
			}
			free(tmp);
			return NULL;
		}
#endif /* WANT_SKYPE_X11 */
#endif /* __CYGWIN__ */
		usleep(100000);			//0.1 sec
		if (tmp->skype_thread == AST_PTHREADT_NULL) {
			ast_log(LOG_ERROR, "Unable to start skype_main thread.\n");
			if (option_debug > 10) {
				DEBUGA_PBX("EXITING FUNC\n", SKYPIAX_TMP_LOG);
			}
			free(tmp);
			return NULL;
		}
		if (option_debug > 1)
			ast_log(LOG_DEBUG, "STARTED signaling_thread=%lu STOP=%lu NULL=%lu\n",
					(unsigned long) tmp->signaling_thread, (unsigned long) AST_PTHREADT_STOP, (unsigned long) AST_PTHREADT_NULL);
	}
#endif

	/* return the newly created skypiax_pvt */
	return tmp;
}

/*! \brief (Re)Start the module main monitor thread, watching for incoming calls on the interfaces */
int skypiax_restart_monitor(void)
{
	static struct skypiax_pvt *p = &skypiax_log_struct;

	/* If we're supposed to be stopped -- stay stopped */
	if (skypiax_monitor_thread == AST_PTHREADT_STOP) {
		return 0;
	}
	LOKKA(&skypiax_monlock);
	/* Do not seems possible to me that this function can be called by the very same monitor thread, but let's be paranoid */
	if (skypiax_monitor_thread == pthread_self()) {
		UNLOCKA(&skypiax_monlock);
		ERRORA("Cannot kill myself\n", SKYPIAX_P_LOG);
		return -1;
	}
	/* if the monitor thread exists */
	if (skypiax_monitor_thread != AST_PTHREADT_NULL) {
		/* Wake up the thread, it can be stuck waiting in a select or so */
		pthread_kill(skypiax_monitor_thread, SIGURG);
	} else {
		/* the monitor thread does not exists, start a new monitor */
		if (ast_pthread_create(&skypiax_monitor_thread, NULL, skypiax_do_monitor, NULL) < 0) {
			UNLOCKA(&skypiax_monlock);
			ERRORA("Unable to start monitor thread.\n", SKYPIAX_P_LOG);
			return -1;
		}
	}
	UNLOCKA(&skypiax_monlock);
	return 0;
}

/*! \brief The skypiax monitoring thread 
 * \note   This thread monitors all the skypiax interfaces that are not in a call
 *         (and thus do not have a separate thread) indefinitely 
 *         */
void *skypiax_do_monitor(void *data)
{
	fd_set rfds;
	int res;
	struct skypiax_pvt *p = NULL;
	int max = -1;
	struct timeval to;
	time_t now_timestamp;

	if (pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL)) {
		ERRORA("Unable to set cancel type to deferred\n", SKYPIAX_P_LOG);
		return NULL;
	}

	for (;;) {
		pthread_testcancel();
		/* Don't let anybody kill us right away.  Nobody should lock the interface list
		   and wait for the monitor list, but the other way around is okay. */
		PUSHA_UNLOCKA(&skypiax_monlock);
		MONITORLOKKA(&skypiax_monlock);
		/* Lock the interface list */
		PUSHA_UNLOCKA(&skypiax_iflock);
		MONITORLOKKA(&skypiax_iflock);
		/* Build the stuff we're going to select on, that is the skypiax_serial_fd of every
		   skypiax_pvt that does not have an associated owner channel. In the case of FBUS2 3310
		   and in the case of PROTOCOL_NO_SERIAL we add the audio_fd as well, because there is not serial signaling of incoming calls */
		FD_ZERO(&rfds);

		time(&now_timestamp);
		p = skypiax_iflist;
		while (p) {
			if (!p->owner) {
				/* This interface needs to be watched, as it lacks an owner */

				if (p->skype) {
					if (FD_ISSET(p->SkypiaxHandles.fdesc[0], &rfds))
						ERRORA("Descriptor %d (SkypiaxHandles.fdesc[0]) appears twice ?\n", SKYPIAX_P_LOG, p->SkypiaxHandles.fdesc[0]);

					if (p->SkypiaxHandles.fdesc[0] > 0) {
						FD_SET(p->SkypiaxHandles.fdesc[0], &rfds);
						if (p->SkypiaxHandles.fdesc[0] > max)
							max = p->SkypiaxHandles.fdesc[0];

					}
				}

			}
			/* next interface, please */
			p = p->next;
		}
		/* Okay, now that we know what to do, release the interface lock */
		MONITORUNLOCKA(&skypiax_iflock);
		POPPA_UNLOCKA(&skypiax_iflock);
		/* And from now on, we're okay to be killed, so release the monitor lock as well */
		MONITORUNLOCKA(&skypiax_monlock);
		POPPA_UNLOCKA(&skypiax_monlock);

		/* you want me to die? */
		pthread_testcancel();

		/* Wait for something to happen */
		to.tv_sec = 0;
		to.tv_usec = 500000;	/* we select with this timeout because under cygwin we avoid the signal usage, so there is no way to end the thread if it is stuck waiting for select */
		res = ast_select(max + 1, &rfds, NULL, NULL, &to);

		/* you want me to die? */
		pthread_testcancel();

		/* Okay, select has finished.  Let's see what happened.  */

		/* If there are errors...  */
		if (res < 0) {
			if (errno == EINTR)	/* EINTR is just the select 
								   being interrupted by a SIGURG, or so */
				continue;
			else {
				ERRORA("select returned %d: %s\n", SKYPIAX_P_LOG, res, strerror(errno));
				return NULL;
			}
		}

		/* must not be killed while skypiax_iflist is locked */
		PUSHA_UNLOCKA(&skypiax_monlock);
		MONITORLOKKA(&skypiax_monlock);
		/* Alright, lock the interface list again, and let's look and see what has
		   happened */
		PUSHA_UNLOCKA(&skypiax_iflock);
		MONITORLOKKA(&skypiax_iflock);

		p = skypiax_iflist;
		for (; p; p = p->next) {

			if (p->skype) {
				if (FD_ISSET(p->SkypiaxHandles.fdesc[0], &rfds)) {
					res = skypiax_signaling_read(p);
					if (res == CALLFLOW_INCOMING_CALLID || res == CALLFLOW_INCOMING_RING) {
						//ast_log(LOG_NOTICE, "CALLFLOW_INCOMING_RING SKYPE\n");
						DEBUGA_SKYPE("CALLFLOW_INCOMING_RING\n", SKYPIAX_P_LOG);
						skypiax_new(p, SKYPIAX_STATE_RING, p->context /* p->context */ );
					}
				}
			}

		}
		MONITORUNLOCKA(&skypiax_iflock);
		POPPA_UNLOCKA(&skypiax_iflock);
		MONITORUNLOCKA(&skypiax_monlock);
		POPPA_UNLOCKA(&skypiax_monlock);
		pthread_testcancel();
	}
/* Never reached */
	return NULL;

}

/*!
 * \brief Initialize the soundcard channels (input and output) used by one interface (a multichannel soundcard can be used by multiple interfaces) 
 * \param p the skypiax_pvt of the interface
 *
 * This function initialize the soundcard channels (input and output) used by one interface (a multichannel soundcard can be used by multiple interfaces). It simply pass its parameters to the right function for the sound system for which has been compiled, eg. alsa_init for ALSA, oss_init for OSS, winmm_init for Windows Multimedia, etc and return the result 
 *
 * \return zero on success, -1 on error.
 */

int skypiax_sound_init(struct skypiax_pvt *p)
{
	return skypiax_audio_init(p);
}

/*!
 * \brief Shutdown the soundcard channels (input and output) used by one interface (a multichannel soundcard can be used by multiple interfaces) 
 * \param p the skypiax_pvt of the interface
 *
 * This function shutdown the soundcard channels (input and output) used by one interface (a multichannel soundcard can be used by multiple interfaces). It simply pass its parameters to the right function for the sound system for which has been compiled, eg. alsa_shutdown for ALSA, oss_shutdown for OSS, winmm_shutdown for Windows Multimedia, etc and return the result
 *
 * \return zero on success, -1 on error.
 */

int skypiax_sound_shutdown(struct skypiax_pvt *p)
{

	//return skypiax_portaudio_shutdown(p);

	return -1;
}

/*! \brief Read audio frames from interface */
struct ast_frame *skypiax_sound_read(struct skypiax_pvt *p)
{
	struct ast_frame *f = NULL;
	int res;

	res = skypiax_audio_read(p);
	f = &p->read_frame;
	return f;
}

/*! \brief Send audio frame to interface */
int skypiax_sound_write(struct skypiax_pvt *p, struct ast_frame *f)
{
	int ret = -1;

	ret = skypiax_audio_write(p, f);
	return ret;
}

/*!
 * \brief This thread runs during a call, and monitor the interface serial port for signaling, like hangup, caller id, etc
 *
 */
void *skypiax_do_controldev_thread(void *data)
{
	struct skypiax_pvt *p = data;
	int res;

	DEBUGA_SERIAL("In skypiax_do_controldev_thread: started, p=%p\n", SKYPIAX_P_LOG, p);

	if (pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL)) {
		ERRORA("Unable to set cancel type to deferred\n", SKYPIAX_P_LOG);
		return NULL;
	}

	while (1) {
		usleep(1000);
		pthread_testcancel();
		if (p->skype) {
			res = skypiax_signaling_read(p);
			if (res == CALLFLOW_INCOMING_HANGUP) {
				DEBUGA_SKYPE("skype call ended\n", SKYPIAX_P_LOG);
				if (p->owner) {
					pthread_testcancel();
					ast_queue_control(p->owner, AST_CONTROL_HANGUP);
				}
			}
		}
	}

	return NULL;

}

/************************************************/

/* LUIGI RIZZO's magic */
/*
 * store the boost factor
 */
#ifdef ASTERISK_VERSION_1_6
void skypiax_store_boost(const char *s, double *boost)
#else
void skypiax_store_boost(char *s, double *boost)
#endif							/* ASTERISK_VERSION_1_6 */
{
	struct skypiax_pvt *p = NULL;

	if (sscanf(s, "%lf", boost) != 1) {
		ERRORA("invalid boost <%s>\n", SKYPIAX_P_LOG, s);
		return;
	}
	if (*boost < -BOOST_MAX) {
		WARNINGA("boost %s too small, using %d\n", SKYPIAX_P_LOG, s, -BOOST_MAX);
		*boost = -BOOST_MAX;
	} else if (*boost > BOOST_MAX) {
		WARNINGA("boost %s too large, using %d\n", SKYPIAX_P_LOG, s, BOOST_MAX);
		*boost = BOOST_MAX;
	}
	*boost = exp(log(10) * *boost / 20) * BOOST_SCALE;
	DEBUGA_SOUND("setting boost %s to %f\n", SKYPIAX_P_LOG, s, *boost);
}

/*
 * returns a pointer to the descriptor with the given name
 */
struct skypiax_pvt *skypiax_console_find_desc(char *dev)
{
	struct skypiax_pvt *p = NULL;

	for (p = skypiax_iflist; p && strcmp(p->name, dev) != 0; p = p->next);
	if (p == NULL)
		WARNINGA("could not find <%s>\n", SKYPIAX_P_LOG, dev);

	return p;
}
int skypiax_console_playback_boost(int fd, int argc, char *argv[])
{
	struct skypiax_pvt *p = skypiax_console_find_desc(skypiax_console_active);

	if (argc > 2) {
		return RESULT_SHOWUSAGE;
	}
	if (!p) {
		ast_cli(fd, "No \"current\" skypiax_console for playback_boost, please enter 'help skypiax_console'\n");
		return RESULT_SUCCESS;
	}

	if (argc == 1) {
		ast_cli(fd, "playback_boost on the active skypiax_console, that is [%s], is: %5.1f\n",
				skypiax_console_active, 20 * log10(((double) p->playback_boost / (double) BOOST_SCALE)));
	} else if (argc == 2) {
		skypiax_store_boost(argv[1], &p->playback_boost);

		ast_cli(fd,
				"playback_boost on the active skypiax_console, that is [%s], is now: %5.1f\n",
				skypiax_console_active, 20 * log10(((double) p->playback_boost / (double) BOOST_SCALE)));
	}

	return RESULT_SUCCESS;
}
int skypiax_console_capture_boost(int fd, int argc, char *argv[])
{
	struct skypiax_pvt *p = skypiax_console_find_desc(skypiax_console_active);

	if (argc > 2) {
		return RESULT_SHOWUSAGE;
	}
	if (!p) {
		ast_cli(fd, "No \"current\" skypiax_console for capture_boost, please enter 'help skypiax_console'\n");
		return RESULT_SUCCESS;
	}

	if (argc == 1) {
		ast_cli(fd, "capture_boost on the active skypiax_console, that is [%s], is: %5.1f\n",
				skypiax_console_active, 20 * log10(((double) p->capture_boost / (double) BOOST_SCALE)));
	} else if (argc == 2) {
		skypiax_store_boost(argv[1], &p->capture_boost);

		ast_cli(fd,
				"capture_boost on the active skypiax_console, that is [%s], is now: %5.1f\n",
				skypiax_console_active, 20 * log10(((double) p->capture_boost / (double) BOOST_SCALE)));
	}

	return RESULT_SUCCESS;
}

int skypiax_console_hangup(int fd, int argc, char *argv[])
{
	struct skypiax_pvt *p = skypiax_console_find_desc(skypiax_console_active);

	if (argc != 1) {
		return RESULT_SHOWUSAGE;
	}
	if (!p) {
		ast_cli(fd, "No \"current\" skypiax_console for hanging up, please enter 'help skypiax_console'\n");
		return RESULT_SUCCESS;
	}
	if (!p->owner) {
		ast_cli(fd, "No call to hangup on the active skypiax_console, that is [%s]\n", skypiax_console_active);
		return RESULT_FAILURE;
	}
	if (p->owner)
		ast_queue_hangup(p->owner);
	return RESULT_SUCCESS;
}

int skypiax_console_dial(int fd, int argc, char *argv[])
{
	char *s = NULL;
	struct skypiax_pvt *p = skypiax_console_find_desc(skypiax_console_active);

	if (argc != 2) {
		return RESULT_SHOWUSAGE;
	}
	if (!p) {
		ast_cli(fd, "No \"current\" skypiax_console for dialing, please enter 'help skypiax_console'\n");
		return RESULT_SUCCESS;
	}

	if (p->owner) {				/* already in a call */
		int i;
		struct ast_frame f = { AST_FRAME_DTMF, 0 };

		s = argv[1];
		/* send the string one char at a time */
		for (i = 0; i < strlen(s); i++) {
			f.subclass = s[i];
			ast_queue_frame(p->owner, &f);
		}
		return RESULT_SUCCESS;
	} else
		ast_cli(fd, "No call in which to dial on the \"current\" skypiax_console, that is [%s]\n", skypiax_console_active);
	if (s)
		free(s);
	return RESULT_SUCCESS;
}
int skypiax_console_set_active(int fd, int argc, char *argv[])
{
	if (argc == 1)
		ast_cli(fd,
				"\"current\" skypiax_console is [%s]\n    Enter 'skypiax_console show' to see the available interfaces.\n    Enter 'skypiax_console interfacename' to change the \"current\" skypiax_console.\n",
				skypiax_console_active);
	else if (argc != 2) {
		return RESULT_SHOWUSAGE;
	} else {
		struct skypiax_pvt *p;
		if (strcmp(argv[1], "show") == 0) {
			ast_cli(fd, "Available interfaces:\n");
			for (p = skypiax_iflist; p; p = p->next)
				ast_cli(fd, "     [%s]\n", p->name);
			return RESULT_SUCCESS;
		}
		p = skypiax_console_find_desc(argv[1]);
		if (p == NULL)
			ast_cli(fd, "Interface [%s] do not exists!\n", argv[1]);
		else {
			skypiax_console_active = p->name;
			ast_cli(fd, "\"current\" skypiax_console is now: [%s]\n", argv[1]);
		}
	}
	return RESULT_SUCCESS;
}

int skypiax_console_skypiax(int fd, int argc, char *argv[])
{
	return RESULT_SHOWUSAGE;
}

void *do_skypeapi_thread(void *obj)
{
	return skypiax_do_skypeapi_thread_func(obj);
}

int dtmf_received(private_t * p, char *value)
{

	struct ast_frame f2 = { AST_FRAME_DTMF, value[0], };
	DEBUGA_SKYPE("Received DTMF: %s\n", SKYPIAX_P_LOG, value);
	ast_queue_frame(p->owner, &f2);

	return 0;

}

int start_audio_threads(private_t * p)
{
	//if (!p->tcp_srv_thread) {
	if (ast_pthread_create(&p->tcp_srv_thread, NULL, skypiax_do_tcp_srv_thread, p) < 0) {
		ERRORA("Unable to start tcp_srv_thread thread.\n", SKYPIAX_P_LOG);
		return -1;
	} else {
		DEBUGA_SKYPE("started tcp_srv_thread thread.\n", SKYPIAX_P_LOG);
	}
	//}
	//if (!p->tcp_cli_thread) {
	if (ast_pthread_create(&p->tcp_cli_thread, NULL, skypiax_do_tcp_cli_thread, p) < 0) {
		ERRORA("Unable to start tcp_cli_thread thread.\n", SKYPIAX_P_LOG);
		return -1;
	} else {
		DEBUGA_SKYPE("started tcp_cli_thread thread.\n", SKYPIAX_P_LOG);
	}
	//}

#ifdef NOTDEF
	switch_threadattr_t *thd_attr = NULL;

	switch_threadattr_create(&thd_attr, skypiax_module_pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&tech_pvt->tcp_srv_thread, thd_attr, skypiax_do_tcp_srv_thread, tech_pvt, skypiax_module_pool);
	DEBUGA_SKYPE("started tcp_srv_thread thread.\n", SKYPIAX_P_LOG);

	switch_threadattr_create(&thd_attr, skypiax_module_pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&tech_pvt->tcp_cli_thread, thd_attr, skypiax_do_tcp_cli_thread, tech_pvt, skypiax_module_pool);
	DEBUGA_SKYPE("started tcp_cli_thread thread.\n", SKYPIAX_P_LOG);
	switch_sleep(100000);

#endif

	return 0;
}

int new_inbound_channel(private_t * p)
{

#ifdef NOTDEF
	switch_core_session_t *session = NULL;
	switch_channel_t *channel = NULL;

	if ((session = switch_core_session_request(skypiax_endpoint_interface, SWITCH_CALL_DIRECTION_INBOUND, NULL)) != 0) {
		switch_core_session_add_stream(session, NULL);
		channel = switch_core_session_get_channel(session);
		skypiax_tech_init(tech_pvt, session);

		if ((tech_pvt->caller_profile =
			 switch_caller_profile_new(switch_core_session_get_pool(session), "skypiax",
									   tech_pvt->dialplan, tech_pvt->callid_name,
									   tech_pvt->callid_number, NULL, NULL, NULL, NULL, "mod_skypiax", tech_pvt->context, tech_pvt->destination)) != 0) {
			char name[128];
			//switch_snprintf(name, sizeof(name), "skypiax/%s/%s", tech_pvt->name, tech_pvt->caller_profile->destination_number);
			switch_snprintf(name, sizeof(name), "skypiax/%s", tech_pvt->name);
			switch_channel_set_name(channel, name);
			switch_channel_set_caller_profile(channel, tech_pvt->caller_profile);
		}
		switch_channel_set_state(channel, CS_INIT);
		if (switch_core_session_thread_launch(session) != SWITCH_STATUS_SUCCESS) {
			ERRORA("Error spawning thread\n", SKYPIAX_P_LOG);
			switch_core_session_destroy(&session);
		}
	}
	switch_channel_mark_answered(channel);

#endif
	return 0;
}

int remote_party_is_ringing(private_t * p)
{
	if (p->owner) {
		ast_queue_control(p->owner, AST_CONTROL_RINGING);
	}

	return 0;
}

int remote_party_is_early_media(private_t * p)
{
	if (p->owner) {
		ast_queue_control(p->owner, AST_CONTROL_RINGING);
	}

	return 0;
}

int outbound_channel_answered(private_t * p)
{

	if (p->owner) {
		ast_queue_control(p->owner, AST_CONTROL_ANSWER);
	}

	return 0;
}
void *skypiax_do_tcp_srv_thread(void *obj)
{
	return skypiax_do_tcp_srv_thread_func(obj);
}
void *skypiax_do_tcp_cli_thread(void *obj)
{
	return skypiax_do_tcp_cli_thread_func(obj);
}

int skypiax_audio_write(struct skypiax_pvt *p, struct ast_frame *f)
{
	int sent;

	sent = write(p->audioskypepipe[1], (short *) f->data, f->datalen);

	return 0;
}
int skypiax_console_skype(int fd, int argc, char *argv[])
{
	struct skypiax_pvt *p = skypiax_console_find_desc(skypiax_console_active);
	char skype_msg[1024];
	int i, a, c;

	if (argc == 1) {
		return RESULT_SHOWUSAGE;
	}
	if (!p) {
		ast_cli(fd, "No \"current\" console for skypiax_, please enter 'help console'\n");
		return RESULT_SUCCESS;
	}
	if (!p->skype) {
		ast_cli(fd, "The \"current\" console is not connected to a Skype client'\n");
		return RESULT_SUCCESS;
	}

	memset(skype_msg, 0, sizeof(skype_msg));
	c = 0;
	for (i = 1; i < argc; i++) {
		for (a = 0; a < strlen(argv[i]); a++) {
			skype_msg[c] = argv[i][a];
			c++;
			if (c == 1022)
				break;
		}
		if (i != argc - 1) {
			skype_msg[c] = ' ';
			c++;
		}
		if (c == 1023)
			break;
	}
	skypiax_signaling_write(p, skype_msg);
	return RESULT_SUCCESS;
}

int skypiax_console_skypiax_dir_import(int fd, int argc, char *argv[])
{
	//int res;
	struct skypiax_pvt *p = skypiax_console_find_desc(skypiax_console_active);
	//char list_command[64];
	char fn[256];
	char date[256] = "";
	time_t t;
	char *configfile = SKYPIAX_DIR_CONFIG;
	int add_to_skypiax_dir_conf = 1;
	//int fromskype = 0;
	//int fromcell = 0;

#if 0
	if (directoriax_entry_extension) {
		skypiax_dir_entry_extension = directoriax_entry_extension;
	} else {
		ast_cli(fd, "No 'directoriax_entry_extension', you MUST have loaded directoriax.so\n");
		return RESULT_SUCCESS;
	}
#endif

	if (argc != 2)
		return RESULT_SHOWUSAGE;
	if (!p) {
		ast_cli(fd, "No \"current\" console ???, please enter 'help skypiax_console'\n");
		return RESULT_SUCCESS;
	}

	if (!strcasecmp(argv[1], "add"))
		add_to_skypiax_dir_conf = 1;
	else if (!strcasecmp(argv[1], "replace"))
		add_to_skypiax_dir_conf = 0;
	else {
		ast_cli(fd, "\n\nYou have neither specified 'add' nor 'replace'\n\n");
		return RESULT_SHOWUSAGE;
	}

#if 0
	if (!strcasecmp(argv[2], "fromskype"))
		fromskype = 1;
	else if (!strcasecmp(argv[2], "fromcell"))
		fromcell = 1;
	else {
		ast_cli(fd, "\n\nYou have neither specified 'fromskype' nor 'fromcell'\n\n");
		return RESULT_SHOWUSAGE;
	}

	if (fromcell) {
		ast_cli(fd, "Importing from cellphone is currently supported only on \"AT\" cellphones :( !\n");
		//fclose(p->phonebook_writing_fp);
		//skypiax_dir_create_extensions();
		return RESULT_SUCCESS;
	}

	if (fromskype)
		if (!p->skype) {
			ast_cli(fd, "Importing from skype is supported by skypiax_dir on chan_skypiax!\n");
			//fclose(p->phonebook_writing_fp);
			//skypiax_dir_create_extensions();
			return RESULT_SUCCESS;
		}

	if (fromcell || fromskype)
		if (argc != 3) {
			ast_cli(fd, "\n\nYou don't have to specify a filename with 'fromcell' or with 'fromskype'\n\n");
			return RESULT_SHOWUSAGE;
		}
#endif

  /*******************************************************************************************/

	if (configfile[0] == '/') {
		ast_copy_string(fn, configfile, sizeof(fn));
	} else {
		snprintf(fn, sizeof(fn), "%s/%s", ast_config_AST_CONFIG_DIR, configfile);
	}
	NOTICA("Opening '%s'\n", SKYPIAX_P_LOG, fn);
	time(&t);
	ast_copy_string(date, ctime(&t), sizeof(date));

	if (add_to_skypiax_dir_conf)
		p->phonebook_writing_fp = fopen(fn, "a+");
	else
		p->phonebook_writing_fp = fopen(fn, "w+");

	if (p->phonebook_writing_fp) {
		if (add_to_skypiax_dir_conf) {
			NOTICA("Opened '%s' for appending \n", SKYPIAX_P_LOG, fn);
			fprintf(p->phonebook_writing_fp, ";!\n");
			fprintf(p->phonebook_writing_fp, ";! Update Date: %s", date);
			fprintf(p->phonebook_writing_fp, ";! Updated by: %s, %d\n", __FILE__, __LINE__);
			fprintf(p->phonebook_writing_fp, ";!\n");
		} else {
			NOTICA("Opened '%s' for writing \n", SKYPIAX_P_LOG, fn);
			fprintf(p->phonebook_writing_fp, ";!\n");
			fprintf(p->phonebook_writing_fp, ";! Automatically generated configuration file\n");
			fprintf(p->phonebook_writing_fp, ";! Filename: %s (%s)\n", configfile, fn);
			fprintf(p->phonebook_writing_fp, ";! Creation Date: %s", date);
			fprintf(p->phonebook_writing_fp, ";! Generated by: %s, %d\n", __FILE__, __LINE__);
			fprintf(p->phonebook_writing_fp, ";!\n");
			fprintf(p->phonebook_writing_fp, "[general]\n\n");
			fprintf(p->phonebook_writing_fp, "[default]\n");
		}

  /*******************************************************************************************/
		//if (fromskype) {
		if (p->skype) {
			WARNINGA("About to querying the Skype client 'Contacts', it may take some moments... Don't worry.\n", SKYPIAX_P_LOG);
			if (p->skype_thread != AST_PTHREADT_NULL) {
				char msg_to_skype[1024];

				p->skype_friends[0] = '\0';
				sprintf(msg_to_skype, "#333 SEARCH FRIENDS");
				if (skypiax_signaling_write(p, msg_to_skype) < 0) {
					return -1;
				}

				int friends_count = 0;
				while (p->skype_friends[0] == '\0') {
					/* FIXME needs a timeout, can't wait forever! 
					 * eg. when skype is running but not connected! */
					usleep(100);
					friends_count++;
					if (friends_count > 20000) {
						return -1;	/* FIXME */
					}
				}

			}

			if (p->skype_thread != AST_PTHREADT_NULL) {
				char msg_to_skype[1024];

				if (p->skype_friends[0] != '\0') {
					char *buf, *where;
					char **stringp;
					int skype_dir_file_written = 0;

					buf = p->skype_friends;
					stringp = &buf;
					where = strsep(stringp, ", ");
					while (where) {
						if (where[0] != '\0') {
							/*
							 * So, we have the Skype username (the HANDLE, I think is called).
							 * But we want to call the names we see in the Skype contact list
							 * So, let's check the DISPLAYNAME (the end user modified contact name)
							 * Then, we check the FULLNAME (that appears as it was the DISPLAYNAME 
							 * if the end user has not modify it)
							 * If we still have neither DISPLAYNAME nor FULLNAME, we'll use the 
							 * Skipe username (the HANDLE)
							 */

							p->skype_displayname[0] = '\0';
							sprintf(msg_to_skype, "#765 GET USER %s DISPLAYNAME", where);
							skypiax_signaling_write(p, msg_to_skype);
							int displayname_count = 0;
							while (p->skype_displayname[0] == '\0') {
								/* FIXME needs a timeout, can't wait forever! 
								 * eg. when skype is running but not connected! */
								usleep(100);
								displayname_count++;
								if (displayname_count > 20000)
									return -1;	/* FIXME */
							}
							if (p->skype_displayname[0] != '\0') {
								char *where2;
								char sanitized[300];

								sanitized[0] = '\0';

								where2 = strstr(p->skype_displayname, "DISPLAYNAME ");
								if (where2) {

									/* there can be some *smart* that makes a displayname 
									 * that is different than first<space>last, */
									/* maybe initials, simbols, slashes, 
									 * something smartish... let's check */

									if (where2[12] != '\0') {
										int i = 12;
										int x = 0;
										int spaces = 0;
										int last_char_was_space = 0;

										for (i = 12; i < strlen(where2) && x < 299; i++) {
											if (!isalnum(where2[i])) {
												if (!isblank(where2[i])) {
													/* bad char */
													continue;
												}
												/* is a space */
												if (last_char_was_space == 1)	/* do not write 2 consecutive spaces */
													continue;
												last_char_was_space = 1;
												sanitized[x] = ' ';
												x++;
												continue;
											}
											/* is alphanum */
											last_char_was_space = 0;
											sanitized[x] = where2[i];
											x++;
											continue;
										}

										sanitized[x] = '\0';
										if (spaces == 0) {
										}
										DEBUGA_SKYPE("sanitized=|%s|, where=|%s|, where2=|%s|\n", SKYPIAX_P_LOG, sanitized, where, &where2[12]);
									}

									if (where2[12] != '\0') {
										skypiax_dir_entry_extension++;
										if (where[0] == '+' || isdigit(where[0])) {	/* is a skypeout number */
											fprintf(p->phonebook_writing_fp,
													"%s  => ,%sSKO,,,hidefromdir=%s|phonebook_direct_calling_ext=%d%s%.4d|phonebook_entry_fromskype=%s|phonebook_entry_owner=%s\n",
													where, sanitized, "no",
													p->skypiax_dir_entry_extension_prefix, "2", skypiax_dir_entry_extension, "yes", "not_specified");
										} else {	/* is a skype name */
											fprintf(p->phonebook_writing_fp,
													"%s  => ,%sSKY,,,hidefromdir=%s|phonebook_direct_calling_ext=%d%s%.4d|phonebook_entry_fromskype=%s|phonebook_entry_owner=%s\n",
													where, sanitized, "no",
													p->skypiax_dir_entry_extension_prefix, "1", skypiax_dir_entry_extension, "yes", "not_specified");
										}
										skype_dir_file_written = 1;

									}
								}
							}
							p->skype_displayname[0] = '\0';

							p->skype_fullname[0] = '\0';
							sprintf(msg_to_skype, "#222 GET USER %s FULLNAME", where);
							skypiax_signaling_write(p, msg_to_skype);
							int fullname_count = 0;
							while (p->skype_fullname[0] == '\0') {
								/* FIXME needs a timeout, can't wait forever! 
								 * eg. when skype is running but not connected! */
								usleep(100);
								fullname_count++;
								if (fullname_count > 20000)
									return -1;	/* FIXME */
							}
							if (p->skype_fullname[0] != '\0') {
								char *where2;
								char sanitized[300];

								where2 = strstr(p->skype_fullname, "FULLNAME ");
								if (where2) {

									/* there can be some *smart* that makes a fullname 
									 * that is different than first<space>last, */
									/* maybe initials, simbols, slashes,
									 *  something smartish... let's check */

									if (where2[9] != '\0') {
										int i = 9;
										int x = 0;
										int spaces = 0;
										int last_char_was_space = 0;

										for (i = 9; i < strlen(where2) && x < 299; i++) {
											if (!isalnum(where2[i])) {
												if (!isblank(where2[i])) {
													/* bad char */
													continue;
												}
												/* is a space */
												if (last_char_was_space == 1)	/* do not write 2 consecutive spaces */
													continue;
												last_char_was_space = 1;
												sanitized[x] = ' ';
												x++;
												continue;
											}
											/* alphanum */
											last_char_was_space = 0;
											sanitized[x] = where2[i];
											x++;
											continue;
										}

										sanitized[x] = '\0';
										if (spaces == 0) {
										}
										DEBUGA_SKYPE("sanitized=|%s|, where=|%s|, where2=|%s|\n", SKYPIAX_P_LOG, sanitized, where, &where2[9]);
									}

									if (skype_dir_file_written == 0) {
										skypiax_dir_entry_extension++;
										if (where2[9] != '\0') {
											if (where[0] == '+' || isdigit(where[0])) {	/* is a skypeout number */
												fprintf(p->phonebook_writing_fp,
														"%s  => ,%sSKO,,,hidefromdir=%s|phonebook_direct_calling_ext=%d%s%.4d|phonebook_entry_fromskype=%s|phonebook_entry_owner=%s\n",
														where, sanitized, "no",
														p->skypiax_dir_entry_extension_prefix, "2", skypiax_dir_entry_extension, "yes", "not_specified");
											} else {	/* is a skype name */
												fprintf(p->phonebook_writing_fp,
														"%s  => ,%sSKY,,,hidefromdir=%s|phonebook_direct_calling_ext=%d%s%.4d|phonebook_entry_fromskype=%s|phonebook_entry_owner=%s\n",
														where, sanitized, "no",
														p->skypiax_dir_entry_extension_prefix, "1", skypiax_dir_entry_extension, "yes", "not_specified");

											}

										} else {
											if (where[0] == '+' || isdigit(where[0])) {	/* is a skypeout number */
												fprintf(p->phonebook_writing_fp,
														"%s  => ,%sSKO,,,hidefromdir=%s|phonebook_direct_calling_ext=%d%s%.4d|phonebook_entry_fromskype=%s|phonebook_entry_owner=%s\n",
														where, where, "no", p->skypiax_dir_entry_extension_prefix,
														"2", skypiax_dir_entry_extension, "yes", "not_specified");
											} else {	/* is a skype name */
												fprintf(p->phonebook_writing_fp,
														"%s  => ,%sSKY,,,hidefromdir=%s|phonebook_direct_calling_ext=%d%s%.4d|phonebook_entry_fromskype=%s|phonebook_entry_owner=%s\n",
														where, where, "no", p->skypiax_dir_entry_extension_prefix,
														"1", skypiax_dir_entry_extension, "yes", "not_specified");

											}
										}
									}

									skype_dir_file_written = 0;

								}

							}
							p->skype_fullname[0] = '\0';

						}
						where = strsep(stringp, ", ");
					}

					p->skype_friends[0] = '\0';
				}
			}
		} else {

			ast_cli(fd, "Skype not configured on the 'current' console, not importing from Skype client!\n");
		}
		//}
  /*******************************************************************************************/
  /*******************************************************************************************/
	} else {
		ast_cli(fd, "\n\nfailed to open the skypiax_dir.conf configuration file: %s\n", fn);
		ERRORA("failed to open the skypiax_dir.conf configuration file: %s\n", SKYPIAX_P_LOG, fn);
		return RESULT_FAILURE;
	}

	fclose(p->phonebook_writing_fp);
	//skypiax_dir_create_extensions();

	return RESULT_SUCCESS;
}

private_t *find_available_skypiax_interface(void)
{
	private_t *p;
	int found = 0;

	/* lock the interfaces' list */
	LOKKA(&skypiax_iflock);
	/* make a pointer to the first interface in the interfaces list */
	p = skypiax_iflist;
	/* Search for the requested interface and verify if is unowned */
	while (p) {
		if (!p->owner) {
			DEBUGA_PBX("Interface is NOT OWNED by a channel\n", SKYPIAX_P_LOG);
			found = 1;
			/* we found the requested interface, bail out from the while loop */
			break;
		} else {
			/* interface owned by a channel */
			DEBUGA_PBX("Interface is OWNED by a channel\n", SKYPIAX_P_LOG);
		}
		/* not yet found, next please */
		p = p->next;
	}

	/* lock the interfaces' list */
	UNLOCKA(&skypiax_iflock);

	if (found)
		return p;
	else
		return NULL;
}

/************************************************/
#ifdef ASTERISK_VERSION_1_4
#ifndef AST_MODULE
#define AST_MODULE "chan_skypiax"
#endif
AST_MODULE_INFO_STANDARD(ASTERISK_GPL_KEY, "Skypiax, Audio-Serial Driver");
#endif /* ASTERISK_VERSION_1_4 */

/* rewriting end */
/*******************************************************************************/
