//indent -gnu -ts4 -br -brs -cdw -lp -ce -nbfda -npcs -nprs -npsl -nbbo -saf -sai -saw -cs -bbo -nhnl -nut -sob -l90 
#include "celliax.h"

/* GLOBAL VARIABLES */
char celliax_console_active_array[50] = "";
char *celliax_console_active = celliax_console_active_array;
/*! \brief Count of active channels for this module */
int celliax_usecnt = 0;
int celliax_debug = 0;
/*! \brief This is the thread for the monitor which checks for input on the channels
 *    which are not currently in use.  */
pthread_t celliax_monitor_thread = AST_PTHREADT_NULL;
pthread_t celliax_monitor_audio_thread = AST_PTHREADT_NULL;
int celliax_dir_entry_extension = 0;

/* CONSTANTS */
/*! \brief Name of configuration file for this module */
const char celliax_config[] = "celliax.conf";

/*! \brief Textual description for this module */
const char celliax_desc[] = "Celliax, Audio-Serial Driver";
/*! \brief Textual type for this module */
const char celliax_type[] = "Celliax";

/*! \brief Definition of this channel for PBX channel registration */
const struct ast_channel_tech celliax_tech = {
  .type = celliax_type,
  .description = celliax_desc,
  .capabilities = AST_FORMAT_SLINEAR,
  .requester = celliax_request,
  .hangup = celliax_hangup,
  .answer = celliax_answer,
  .read = celliax_read,
  .call = celliax_call,
  .write = celliax_write,
  .indicate = celliax_indicate,
  .fixup = celliax_fixup,
  .devicestate = celliax_devicestate,
#ifdef ASTERISK_VERSION_1_4
  .send_digit_begin = celliax_senddigit_begin,
  .send_digit_end = celliax_senddigit_end,
#else /* ASTERISK_VERSION_1_4 */
  .send_digit = celliax_senddigit,
#endif /* ASTERISK_VERSION_1_4 */
};

#ifdef ASTERISK_VERSION_1_4
#include "asterisk/abstract_jb.h"
/*! Global jitterbuffer configuration - by default, jb is disabled */
static struct ast_jb_conf default_jbconf = {
	.flags = 0,
	.max_size = -1,
	.resync_threshold = -1,
	.impl = ""
};
static struct ast_jb_conf global_jbconf;
#endif /* ASTERISK_VERSION_1_4 */


#ifdef CELLIAX_ALSA
char celliax_console_alsa_period_usage[] =
  "Usage: celliax_alsa_period [alsa_period_size, in bytes] [alsa_periods_in_buffer, how many]\n"
  "       Shows or set the values of the period and the buffer used by the ALSA subsistem. Standard values are 160 for alsa_period_size and 4 for alsa_periods_in_buffer. Without specifying a value, it just shows the current values. The values are for the  \"current\" console (Celliax) channel.\n"
  "       Enter 'help console' on how to change the \"current\" console\n";
#endif /* CELLIAX_ALSA */

char mandescr_celliax_sendsms[] =
  "Description: Send an SMS through the designated Celliax interface.\n" "Variables: \n"
  "  Interface: <name>  The Celliax interface name you want to use.\n"
  "  Number: <number>   The recipient number you want to send the SMS to.\n"
  "  Text: <text>       The text of the SMS to be sent.\n"
  "  ActionID: <id>     The Action ID for this AMI transaction.\n";

char celliax_console_celliax_usage[] =
  "       \n" "chan_celliax commands info\n" "       \n"
  "       chan_celliax adds to Asterisk the following CLI commands and DIALPLAN applications:\n"
  "       \n" "       CLI COMMANDS:\n" "          celliax_hangup\n"
  "          celliax_dial\n" "          celliax_console\n"
#ifdef CELLIAX_DIR
  "          celliax_dir_import\n" "          celliax_dir_export\n"
#endif /* CELLIAX_DIR */
  "          celliax_playback_boost\n" "          celliax_capture_boost\n"
  "          celliax_sendsms\n" "          celliax_echo\n" "          celliax_at\n"
  "       \n" "       DIALPLAN APPLICATIONS:\n" "          CelliaxSendsms\n" "       \n"
  "       You can type 'help [command]' or 'show application [application]' to obtain more specific info on usage.\n"
  "       \n";
char celliax_console_hangup_usage[] =
  "Usage: celliax_hangup\n"
  "       Hangs up any call currently placed on the \"current\" celliax_console (Celliax) channel.\n"
  "       Enter 'help celliax_console' on how to change the \"current\" celliax_console\n";
char celliax_console_playback_boost_usage[] =
  "Usage: celliax_playback_boost [value]\n"
  "       Shows or set the value of boost applied to the outgoing sound (voice). Possible values are: 0 (no boost applied), -40 to 40 (negative to positive range, in db). Without specifying a value, it just shows the current value. The value is for the  \"current\" celliax_console (Celliax) channel.\n"
  "       Enter 'help celliax_console' on how to change the \"current\" celliax_console\n";
char celliax_console_capture_boost_usage[] =
  "Usage: celliax_capture_boost [value]\n"
  "       Shows or set the value of boost applied to the incoming sound (voice). Possible values are: 0 (no boost applied), -40 to 40 (negative to positive range, in db). Without specifying a value, it just shows the current value. The value is for the  \"current\" celliax_console (Celliax) channel.\n"
  "       Enter 'help celliax_console' on how to change the \"current\" celliax_console\n";

#ifdef CELLIAX_DIR
char celliax_console_celliax_dir_import_usage[] =
  "Usage: celliax_dir_import [add | replace] fromcell\n"
  "       Write in the celliax_dir.conf config file all the entries found in the phonebook of the cellphone connected on the \"current\" celliax_console (Celliax) channel or in the 'Contacts' list of the Skype client.\n"
  "       Enter 'help celliax_console' on how to change the \"current\" celliax_console\n";
char celliax_console_celliax_dir_export_usage[] =
#ifdef CELLIAX_LIBCSV
  "Usage: celliax_dir_export tocell|tocsv celliax_cellphonenumber [csv_filename]\n"
#else /* CELLIAX_LIBCSV */
  "Usage: celliax_dir_export tocell celliax_cellphonenumber\n"
#endif /* CELLIAX_LIBCSV */
  "       With 'tocell' modifier, write in the cellphone connected on the \"current\" celliax_console (Celliax) all the entries found in directoriax.conf, in the form: [celliax_cellphonenumber]wait_for_answer celliax_dir_prefix celliax_dir_entry. So, you can choose the new entry from the cellphone phonebook, dial it, and be directly connected to celliax_dir extension chosen, without having to pass through the voice menu.\n"
#ifdef CELLIAX_LIBCSV
  "       With 'tocsv' modifier, write in a file (Comma Separated Values, suitable to be imported by various software (eg Outlook) and smartphones) all the entries found in directoriax.conf, in the form: [celliax_cellphonenumber]wait_for_answer celliax_dir_prefix celliax_dir_entry. So, you can choose the new entry from the imported phonebook, dial it, and be directly connected to celliax_dir extension chosen, without having to pass through the voice menu.\n"
#endif /* CELLIAX_LIBCSV */
  "       Enter 'help celliax_console' on how to change the \"current\" celliax_console\n";

#endif /* CELLIAX_DIR */

char celliax_console_dial_usage[] =
  "Usage: celliax_dial [DTMFs]\n"
  "       Dials a given DTMF string in the call currently placed on the\n"
  "       \"current\" celliax_console (Celliax) channel.\n"
  "       Enter 'help celliax_console' on how to change the \"current\" celliax_console\n";
char celliax_console_sendsms_usage[] =
  "Usage: celliax_sendsms interfacename/number_to_send_sms_to SMS_TEXT\n"
  "  This CLI command will use the specified Celliax interface to send an SMS with content SMS_TEXT to the number_to_send_sms_to\n"
  "  Eg:\n" "  celliax_sendsms nicephone/3472665618 \"ciao bello\"\n";

char celliax_console_celliax_console_usage[] =
  "Usage: celliax_console [interface] | show\n"
  "       If used without a parameter, displays which interface is the \"current\"\n"
  "       celliax_console.  If a device is specified, the \"current\" celliax_console is changed to\n"
  "       the interface specified.\n"
  "       If the parameter is \"show\", the available interfaces are listed\n";
char *celliax_sendsmsapp = "CelliaxSendsms";

char *celliax_sendsmssynopsis = "CelliaxSendsms sends an SMS through the cellphone";
char *celliax_sendsmsdescrip =
  "  CelliaxSendsms(interface_name / number_to_send_sms_to , SMS_TEXT):\n"
  "  This application will use the specified Celliax interface to send an SMS with content SMS_TEXT to the number_to_send_sms_to\n"
  "  Eg:\n" "  CelliaxSendsms(nicephone/3472665618,\"ciao bello\")\n" "  or\n"
  "  CelliaxSendsms(nicephone/3472665618,${DATETIME}\"ciao bello\")\n" "\n";
char celliax_console_echo_usage[] =
  "Usage: celliax_echo [0|1] [0|1]\n"
  "       Shows or set the values (0 meaning OFF and 1 meaning ON) of the echo suppression options: speexecho and speexpreprocess. Without specifying a value, it just shows the current values. The values are for the  \"current\" celliax_console (Celliax) channel.\n"
  "       Enter 'help celliax_console' on how to change the \"current\" celliax_console\n";
char celliax_console_at_usage[] =
  "Usage: celliax_at [command string]\n"
  "       Send the 'command string' to the 'AT modem' (cellphone) connected to the  \"current\" celliax_console (Celliax) channel.\n"
  "       Enter 'help celliax_console' on how to change the \"current\" celliax_console\n";
/*! \brief fake celliax_pvt structure values, 
 * just for logging purposes */
struct celliax_pvt celliax_log_struct = {
  .name = "none",
};

/*! \brief Default celliax_pvt structure values, 
 * used by celliax_mkif to initialize the interfaces */
struct celliax_pvt celliax_default = {
  .readpos = AST_FRIENDLY_OFFSET,   /* start here on reads */
  .oss_write_dst = 0,           /* start here on reads */
  .interface_state = AST_STATE_DOWN,
  .phone_callflow = CALLFLOW_CALL_IDLE,
  .dsp_silence_threshold = 512,
  .context = "default",
  .language = "en",
  .exten = "s",
  .controldevice_name = "none",
  .controldevfd = 0,
  .next = NULL,
  .owner = NULL,
  .dsp = NULL,
  .fbus2_outgoing_list = NULL,
  .seqnumfbus = FBUS2_SEQNUM_MAX,
  .controldev_thread = AST_PTHREADT_NULL,
  .arraycounter = 0,
#ifndef GIOVA48
  .celliax_sound_rate = 8000,
#else // GIOVA48
  .celliax_sound_rate = 48000,
#endif // GIOVA48
  .celliax_sound_capt_fd = -1,
  .need_acoustic_ring = 0,
  .celliax_serial_synced_timestamp = 0,
  .celliax_serial_sync_period = 300,
  .audio_play_reset_timestamp = 0,
  .audio_capture_reset_timestamp = 0,
  .controldevice_speed = B38400,
  .capture_boost = 0,
  .playback_boost = 0,
  .stripmsd = 0,
  .controldev_dead = 0,
  .dtmf_inited = 0,
  .at_dial_pre_number = "AT+CKPD=\"",
  //.at_dial_post_number = "S\"",
  .at_dial_post_number = ";",
  .at_dial_expect = "OK",
  .at_early_audio = 0,
  .at_hangup = "AT+CKPD=\"E\"",
  .at_hangup_expect = "OK",
  .at_answer = "ATA",
  .at_answer_expect = "OK",
  .at_send_dtmf = "AT+CKPD",
  .at_initial_pause = 0,
  .at_preinit_1 = "",
  .at_preinit_1_expect = "",
  .at_preinit_2 = "",
  .at_preinit_2_expect = "",
  .at_preinit_3 = "",
  .at_preinit_3_expect = "",
  .at_preinit_4 = "",
  .at_preinit_4_expect = "",
  .at_preinit_5 = "",
  .at_preinit_5_expect = "",
  .at_after_preinit_pause = 0,
  .at_postinit_1 = "",
  .at_postinit_1_expect = "",
  .at_postinit_2 = "",
  .at_postinit_2_expect = "",
  .at_postinit_3 = "",
  .at_postinit_3_expect = "",
  .at_postinit_4 = "",
  .at_postinit_4_expect = "",
  .at_postinit_5 = "",
  .at_postinit_5_expect = "",
  .at_query_battchg = "",
  .at_query_battchg_expect = "",
  .at_query_signal = "",
  .at_query_signal_expect = "",
  .at_call_idle = "",
  .at_call_incoming = "",
  .at_call_active = "",
  .at_call_failed = "",
  .at_call_calling = "",
  .at_indicator_noservice_string = "CIEV: 2,0",
  .at_indicator_nosignal_string = "CIEV: 5,0",
  .at_indicator_lowsignal_string = "CIEV: 5,1",
  .at_indicator_lowbattchg_string = "CIEV: 0,1",
  .at_indicator_nobattchg_string = "CIEV: 0,0",
  .at_indicator_callactive_string = "CIEV: 3,1",
  .at_indicator_nocallactive_string = "CIEV: 3,0",
  .at_indicator_nocallsetup_string = "CIEV: 6,0",
  .at_indicator_callsetupincoming_string = "CIEV: 6,1",
  .at_indicator_callsetupoutgoing_string = "CIEV: 6,2",
  .at_indicator_callsetupremoteringing_string = "CIEV: 6,3",
  .at_has_clcc = 0,
  .at_has_ecam = 0,

  .skype = 0,
  .celliax_dir_entry_extension_prefix = 5,

#ifdef CELLIAX_CVM
  .cvm_subsc_1_pin = "0000",
  .cvm_subsc_2_pin = "0000",
  .cvm_subsc_no = 1,
  .cvm_lock_state = CVM_UNKNOWN_LOCK_STATE,
  .cvm_register_state = CVM_UNKNOWN_REGISTER_STATE,
  .cvm_busmail_outgoing_list = NULL,
  .busmail_rxseq_cvm_last = 0xFF,   /*!< \brief sequential number of BUSMAIL messages, (0-7) */
  .busmail_txseq_celliax_last = 0xFF,   /*!< \brief sequential number of BUSMAIL messages, (0-7) */
  .cvm_volume_level = 5,
  .cvm_celliax_serial_delay = 200,  /* 200ms delay after sending down the wire, fix for a bug ? */
  .cvm_handset_no = 0,
  .cvm_fp_is_cvm = 0,
  .cvm_rssi = 0,

#endif /* CELLIAX_CVM */
#ifdef CELLIAX_LIBCSV
  .csv_separator_is_semicolon = 0,  //FIXME as option
  .csv_complete_name_pos = 4,   //FIXME as option was 4 for outlook express and some outlook, other outlook 2
  .csv_email_pos = 6,           //FIXME as option for outlook express
  .csv_home_phone_pos = 32,     //FIXME as option was 12 for outlook express
  .csv_mobile_phone_pos = 33,   //FIXME as option was 14 for outlook express
  .csv_business_phone_pos = 41, //FIXME as option was 22 for outlook express
  .csv_first_row_is_title = 1,  //FIXME as option
#endif /* CELLIAX_LIBCSV */

  .audio_play_reset_period = 0, //do not reset

  .isInputInterleaved = 1,
  .isOutputInterleaved = 1,
  .numInputChannels = 1,
  .numOutputChannels = 1,
#ifndef GIOVA48
  .framesPerCallback = 160,
#else // GIOVA48
  .framesPerCallback = 960,
#endif // GIOVA48
  .speexecho = 1,
  .speexpreprocess = 1,
  .portaudiocindex = -1,
  .portaudiopindex = -1,
#ifdef CELLIAX_ALSA
  .alsa_period_size = 160,
  .alsa_periods_in_buffer = 4,
  .alsac = NULL,
  .alsap = NULL,
  .alsawrite_filled = 0,
#endif /* CELLIAX_ALSA */

};

/*! 
 * \brief PVT structure for a celliax interface (channel), created by celliax_mkif
 */
struct celliax_pvt *celliax_iflist = NULL;

#ifdef ASTERISK_VERSION_1_6_0
struct ast_cli_entry myclis[] = {
  AST_CLI_DEFINE(celliax_console_hangup, "Hangup a call on the console"),
  //AST_CLI_DEFINE(celliax_console_dial, "Dial an extension on the console"),
  //AST_CLI_DEFINE(celliax_console_playback_boost, "Sets/displays spk boost in dB"),
  //AST_CLI_DEFINE(celliax_console_capture_boost, "Sets/displays mic boost in dB"),
  //AST_CLI_DEFINE(celliax_console_set_active, "Sets/displays active console"),
  //AST_CLI_DEFINE(celliax_console_at, "Sends an AT command"),
  //AST_CLI_DEFINE(celliax_console_echo, "Echo suppression"),
#ifdef CELLIAX_DIR
  //AST_CLI_DEFINE(celliax_console_celliax_dir_import, "imports entries from cellphone"),
  //AST_CLI_DEFINE(celliax_console_celliax_dir_export, "exports entries to cellphone"),
#endif /* CELLIAX_DIR */
  //AST_CLI_DEFINE(celliax_console_celliax, "all things celliax"),
  //AST_CLI_DEFINE(celliax_console_sendsms, "Send an SMS from a Celliax interface"),
};
#else
struct ast_cli_entry myclis[] = {
  {{"celliax_hangup", NULL}, celliax_console_hangup,
   "Hangup a call on the celliax_console",
   celliax_console_hangup_usage},
  {{"celliax_playback_boost", NULL}, celliax_console_playback_boost, "playback boost",
   celliax_console_playback_boost_usage},
  {{"celliax_capture_boost", NULL}, celliax_console_capture_boost, "capture boost",
   celliax_console_capture_boost_usage},
  {{"celliax_usage", NULL}, celliax_console_celliax, "chan_celliax commands info",
   celliax_console_celliax_usage},

  {{"celliax_at", NULL}, celliax_console_at, "AT command",
   celliax_console_at_usage},
  {{"celliax_echo", NULL}, celliax_console_echo, "echo suppression",
   celliax_console_echo_usage},
#ifdef CELLIAX_DIR
  {{"celliax_dir_import", NULL}, celliax_console_celliax_dir_import,
   "Write the celliax_dir.conf file, used by celliax_dir app",
   celliax_console_celliax_dir_import_usage},
  {{"celliax_dir_export", NULL}, celliax_console_celliax_dir_export,
   "Write in the cellphone the contents of the celliax_dir.conf file, used by celliax_dir app",
   celliax_console_celliax_dir_export_usage},
#endif /* CELLIAX_DIR */
#ifdef CELLIAX_ALSA
  {{"celliax_alsa_period", NULL}, console_alsa_period, "alsa_period",
   celliax_console_alsa_period_usage},
#endif /* CELLIAX_ALSA */

  {{"celliax_dial", NULL}, celliax_console_dial,
   "Dial an extension on the celliax_console",
   celliax_console_dial_usage},
  {{"celliax_sendsms", NULL}, celliax_console_sendsms,
   "Send an SMS from a Celliax interface",
   celliax_console_sendsms_usage},
  {{"celliax_console", NULL}, celliax_console_set_active,
   "Sets/displays active celliax_console",
   celliax_console_celliax_console_usage},
};
#endif /* ASTERISK_VERSION_1_6_0 */

/* IMPLEMENTATION */

#ifdef CELLIAX_ALSA
int console_alsa_period(int fd, int argc, char *argv[])
{
  struct celliax_pvt *p = celliax_console_find_desc(celliax_console_active);

  if (argc > 3 || argc == 2)
    return RESULT_SHOWUSAGE;
  if (!p) {
    ast_cli(fd,
            "No \"current\" console for alsa_period_size, please enter 'help console'\n");
    return RESULT_SUCCESS;
  }

  if (argc == 1) {
    ast_cli(fd,
            "On the active console, that is [%s], alsa_period_size and alsa_periods_in_buffer are: %d and %d\n",
            celliax_console_active, p->alsa_period_size, p->alsa_periods_in_buffer);
  } else if (argc == 3) {

    if (p->owner) {
      ast_cli(fd,
              "CANNOT SET alsa_period_size and alsa_periods_in_buffer on the active console, that is [%s], because there is a call ongoing\n",
              celliax_console_active);
      return RESULT_SUCCESS;
    }
    sscanf(argv[1], "%d", &p->alsa_period_size);
    sscanf(argv[2], "%d", &p->alsa_periods_in_buffer);
    ast_cli(fd,
            "alsa_period_size and alsa_periods_in_buffer on the active console, that is [%s], are now: %d and %d\n",
            celliax_console_active, p->alsa_period_size, p->alsa_periods_in_buffer);

    if (celliax_monitor_audio_thread
        && (celliax_monitor_audio_thread != AST_PTHREADT_NULL)
        && (celliax_monitor_audio_thread != AST_PTHREADT_STOP)) {

      if (pthread_cancel(celliax_monitor_audio_thread)) {
        ERRORA("pthread_cancel celliax_monitor_audio_thread failed, BAD\n",
               CELLIAX_P_LOG);
      }
      if (pthread_kill(celliax_monitor_audio_thread, SIGURG)) {
        DEBUGA_PBX("pthread_kill celliax_monitor_audio_thread failed, no problem\n", CELLIAX_P_LOG);    //maybe it just died
      }

      if (pthread_join(celliax_monitor_audio_thread, NULL)) {
        ERRORA("pthread_join failed, BAD\n", CELLIAX_P_LOG);
      }
    }

    alsa_shutdown(p);
    //sleep(5);
    alsa_init(p);

    if (ast_pthread_create
        (&celliax_monitor_audio_thread, NULL, celliax_do_audio_monitor, NULL) < 0) {
      ERRORA("Unable to start audio_monitor thread.\n", CELLIAX_P_LOG);
      return -1;
    }

    ast_cli(fd,
            "ACTIVATED alsa_period_size and alsa_periods_in_buffer on the active console\n");
  }

  return RESULT_SUCCESS;
}
#endif /* CELLIAX_ALSA */

void celliax_unlocka_log(void *x)
{
  ast_mutex_t *y;
  y = x;
  int i;

  for (i = 0; i < 5; i++) {     //let's be generous

    ast_log(LOG_DEBUG,
            CELLIAX_SVN_VERSION
            "[%-7lx] I'm a dying thread, and I'm to go unlocking mutex %p for the %dth time\n",
            (unsigned long int) pthread_self(), y, i);

    ast_mutex_unlock(y);
  }
  ast_log(LOG_DEBUG,
          CELLIAX_SVN_VERSION
          "[%-7lx] I'm a dying thread, I've finished unlocking mutex %p\n",
          (unsigned long int) pthread_self(), y);
}

int celliax_queue_control(struct ast_channel *c, int control)
{
  struct celliax_pvt *p = c->tech_pvt;
  int times;

/* queue the frame */
  if (p)
    p->control_to_send = control;
  else
    return 0;
  DEBUGA_PBX("Queued CONTROL FRAME %d\n", CELLIAX_P_LOG, control);

/* wait for the frame to be sent */
  while (p->control_to_send){
    usleep(1000);
    times++;
    if(times == 1000){
	    ERRORA("Queued CONTROL FRAME %d FAILED to be sent\n", CELLIAX_P_LOG, control);
	    p->control_to_send = 0;
	    break;
    }
  }

  return 0;
}

int celliax_devicestate(void *data)
{
  struct celliax_pvt *p = NULL;
  char *name = data;
  int res = AST_DEVICE_INVALID;

  if (!data) {
    ERRORA("Devicestate requested with no data\n", CELLIAX_P_LOG);
    return res;
  }

  /* lock the interfaces' list */
  LOKKA(&celliax_iflock);
  /* make a pointer to the first interface in the interfaces list */
  p = celliax_iflist;
  /* Search for the requested interface and verify if is unowned */
  while (p) {
    size_t length = strlen(p->name);
    /* is this the requested interface? */
    if (strncmp(name, p->name, length) == 0) {
      /* is this interface unowned? */
      if (!p->owner) {
        res = AST_DEVICE_NOT_INUSE;
        DEBUGA_PBX("Interface is NOT OWNED by a channel\n", CELLIAX_P_LOG);
      } else {
        /* interface owned by a channel */
        res = AST_DEVICE_INUSE;
        DEBUGA_PBX("Interface is OWNED by a channel\n", CELLIAX_P_LOG);
      }

      /* we found the requested interface, bail out from the while loop */
      break;
    }
    /* not yet found, next please */
    p = p->next;
  }
  /* unlock the interfaces' list */
  UNLOCKA(&celliax_iflock);

  if (res == AST_DEVICE_INVALID) {
    ERRORA("Checking device state for interface [%s] returning AST_DEVICE_INVALID\n",
           CELLIAX_P_LOG, name);
  }
  return res;
}

#ifndef ASTERISK_VERSION_1_4
int celliax_indicate(struct ast_channel *c, int cond)
#else
int celliax_indicate(struct ast_channel *c, int cond, const void *data, size_t datalen)
#endif
{
  struct celliax_pvt *p = c->tech_pvt;
  int res = 0;

  switch (cond) {
  case AST_CONTROL_BUSY:
  case AST_CONTROL_CONGESTION:
  case AST_CONTROL_RINGING:
  case -1:
    NOTICA("Let's INDICATE %d\n", CELLIAX_P_LOG, cond);
    res = -1;                   /* Ask for inband indications */
    break;
  case AST_CONTROL_PROGRESS:
  case AST_CONTROL_PROCEEDING:
  case AST_CONTROL_VIDUPDATE:
  case AST_CONTROL_HOLD:
  case AST_CONTROL_UNHOLD:
#ifdef ASTERISK_VERSION_1_4
    //FIXME case AST_CONTROL_SRCUPDATE:
#endif /* ASTERISK_VERSION_1_4 */
    NOTICA("Let's NOT INDICATE %d\n", CELLIAX_P_LOG, cond);
    break;
  default:
    WARNINGA("Don't know how to display condition %d on %s\n", CELLIAX_P_LOG, cond,
             c->name);
    /* The core will play inband indications for us if appropriate */
    res = -1;
  }

  return res;
}

/*! \brief PBX interface function -build celliax pvt structure 
 *         celliax calls initiated by the PBX arrive here */
struct ast_channel *celliax_request(const char *type, int format, void *data, int *cause)
{
  struct celliax_pvt *p = NULL;
  struct ast_channel *tmp = NULL;
  char *name = data;

  if (option_debug) {
    DEBUGA_PBX("ENTERING FUNC\n", CELLIAX_P_LOG);
  }

  DEBUGA_PBX("Try to request type: %s, name: %s, cause: %d," " format: %d\n",
             CELLIAX_P_LOG, type, name, *cause, format);

  if (!data) {
    ERRORA("Channel requested with no data\n", CELLIAX_P_LOG);
    if (option_debug) {
      DEBUGA_PBX("EXITING FUNC\n", CELLIAX_P_LOG);
    }
    return NULL;
  }

  /* lock the interfaces' list */
  LOKKA(&celliax_iflock);
  /* make a pointer to the first interface in the interfaces list */
  p = celliax_iflist;
  /* Search for the requested interface and verify if is unowned and format compatible */
  //TODO implement groups a la chan_zap
  while (p) {
    size_t length = strlen(p->name);
    /* is this the requested interface? */
    if (strncmp(name, p->name, length) == 0) {
      /* is the requested format supported by this interface? */
      if ((format & AST_FORMAT_SLINEAR) != 0) {
        /* is this interface unowned? */
        if (!p->owner) {
          DEBUGA_PBX("Requesting: %s, name: %s, format: %d\n", CELLIAX_P_LOG, type, name,
                     format);
          /* create a new channel owning this interface */
          tmp = celliax_new(p, AST_STATE_DOWN, p->context);
          if (!tmp) {
            /* the channel was not created, probable memory allocation error */
            *cause = AST_CAUSE_SWITCH_CONGESTION;
          }
        } else {
          /* interface owned by another channel */
          WARNINGA("owned by another channel\n", CELLIAX_P_LOG);
          *cause = AST_CAUSE_REQUESTED_CHAN_UNAVAIL;
        }
      } else {
        /* requested format not supported */
        WARNINGA("format %d not supported\n", CELLIAX_P_LOG, format);
        *cause = AST_CAUSE_BEARERCAPABILITY_NOTAVAIL;
      }
      /* we found the requested interface, bail out from the while loop */
      break;
    }
    /* not yet found, next please */
    p = p->next;
  }
  /* unlock the interfaces' list */
  UNLOCKA(&celliax_iflock);
  /* restart the monitor so it will watch only the remaining unowned interfaces  */
  celliax_restart_monitor();
  if (tmp == NULL) {
    /* new channel was not created */
    WARNINGA("Unable to create new Celliax channel %s\n", CELLIAX_P_LOG, name);
  }
  if (option_debug) {
    DEBUGA_PBX("EXITING FUNC\n", CELLIAX_P_LOG);
  }
  /* return the newly created channel */
  return tmp;
}

/*! \brief  Hangup celliax call
 * Part of PBX interface, called from ast_hangup */

int celliax_hangup(struct ast_channel *c)
{
  struct celliax_pvt *p;
        int res;

  /* get our celliax pvt interface from channel */
  p = c->tech_pvt;
  /* if there is not celliax pvt why we are here ? */
  if (!p) {
    ERRORA("Asked to hangup channel not connected\n", CELLIAX_P_LOG);
    return 0;
  }

  if (option_debug) {
    DEBUGA_PBX("ENTERING FUNC\n", CELLIAX_P_LOG);
  }

    p->phone_callflow = CALLFLOW_CALL_HANGUP_REQUESTED;
  /* shutdown the serial monitoring thread */
  if (p->controldev_thread && (p->controldev_thread != AST_PTHREADT_NULL)
      && (p->controldev_thread != AST_PTHREADT_STOP)) {
    if (pthread_cancel(p->controldev_thread)) {
      ERRORA("controldev_thread pthread_cancel failed, maybe he killed himself?\n",
             CELLIAX_P_LOG);
    }
    /* push it, maybe is stuck in a select or so */
    if (pthread_kill(p->controldev_thread, SIGURG)) {
      DEBUGA_SERIAL("controldev_thread pthread_kill failed, no problem\n", CELLIAX_P_LOG);
    }
#ifndef __CYGWIN__              /* under cygwin, this seems to be not reliable, get stuck at times */
    /* wait for it to die */
    if (pthread_join(p->controldev_thread, NULL)) {
      ERRORA("controldev_thread pthread_join failed, BAD\n", CELLIAX_P_LOG);
    }
#else /* __CYGWIN__ */
/* allow the serial thread to die */
    usleep(300000);             //300msecs
#endif /* __CYGWIN__ */
  }
  p->controldev_thread = AST_PTHREADT_NULL;

  if (p->controldevprotocol != PROTOCOL_NO_SERIAL) {
    if (p->interface_state != AST_STATE_DOWN) {
      /* actually hangup through the serial port */
      if (p->controldevprotocol != PROTOCOL_NO_SERIAL) {
        res = celliax_serial_hangup(p);
        if (res) {
          ERRORA("celliax_serial_hangup error: %d\n", CELLIAX_P_LOG, res);
          if (option_debug) {
            DEBUGA_PBX("EXITING FUNC\n", CELLIAX_P_LOG);
          }
          return -1;
        }
      }

      while (p->interface_state != AST_STATE_DOWN) {
        usleep(10000);          //10msec
      }
      if (p->interface_state != AST_STATE_DOWN) {
        ERRORA("call hangup failed\n", CELLIAX_P_LOG);
        return -1;
      } else {
        DEBUGA_SERIAL("call hungup\n", CELLIAX_P_LOG);
      }
    }
  } else {
    p->interface_state = AST_STATE_DOWN;
    p->phone_callflow = CALLFLOW_CALL_IDLE;
  }
  /* if there is a dsp struct alloced, free it */
  if (p->dsp) {
    ast_dsp_free(p->dsp);
    p->dsp = NULL;
  }
#ifndef __CYGWIN__
#ifdef CELLIAX_ALSA
/* restart alsa */
  snd_pcm_drop(p->alsap);
  snd_pcm_prepare(p->alsap);

  snd_pcm_prepare(p->alsac);
  snd_pcm_start(p->alsac);

    /* shutdown the sound system, close sound fds, and if exist shutdown the sound managing threads */
    DEBUGA_SOUND("shutting down sound\n", CELLIAX_P_LOG);
    res = celliax_sound_shutdown(p);
    if (res == -1) {
      ERRORA("Failed to shutdown sound\n", CELLIAX_P_LOG);
    }


#endif /* CELLIAX_ALSA */

#endif /* __CYGWIN__ */
#ifdef CELLIAX_PORTAUDIO
  speex_echo_state_reset(p->stream->echo_state);
#endif // CELLIAX_PORTAUDIO

  /* re-init the serial port, be paranoid */
  if (p->controldevprotocol != PROTOCOL_NO_SERIAL) {
    p->controldevfd = celliax_serial_init(p, p->controldevice_speed);
    if (p->controldevfd < 1) {
      ERRORA("bad, bad, bad\n", CELLIAX_P_LOG);
      if (option_debug) {
        DEBUGA_PBX("EXITING FUNC\n", CELLIAX_P_LOG);
      }
      return -1;
    }
  }
#ifndef ASTERISK_VERSION_1_4
  /* subtract one to the usage count of Celliax-type channels */
  LOKKA(&celliax_usecnt_lock);
  celliax_usecnt--;
  if (celliax_usecnt < 0)
    ERRORA("Usecnt < 0???\n", CELLIAX_P_LOG);
  UNLOCKA(&celliax_usecnt_lock);
  ast_update_use_count();
#else /* ASTERISK_VERSION_1_4 */
	ast_module_unref(ast_module_info->self);
#endif /* ASTERISK_VERSION_1_4 */

  /* our celliax pvt interface is no more part of a channel */
  p->owner = NULL;
  /* our channel has no more this celliax pvt interface to manage */
  c->tech_pvt = NULL;
  /* set the channel state to DOWN, eg. available, not in active use */
  if (ast_setstate(c, AST_STATE_DOWN)) {
    ERRORA("ast_setstate failed, BAD\n", CELLIAX_P_LOG);
    if (option_debug) {
      DEBUGA_PBX("EXITING FUNC\n", CELLIAX_P_LOG);
    }
    return -1;
  }

  if (option_debug)
    DEBUGA_PBX("Hanged Up\n", CELLIAX_P_LOG);
  /* restart the monitor thread, so it can recheck which interfaces it have to watch during its loop (the interfaces that are not owned by channels) */
  if (celliax_restart_monitor()) {
    ERRORA("celliax_restart_monitor failed, BAD\n", CELLIAX_P_LOG);
    if (option_debug) {
      DEBUGA_PBX("EXITING FUNC\n", CELLIAX_P_LOG);
    }
    return -1;
  }

  if (option_debug) {
    DEBUGA_PBX("EXITING FUNC\n", CELLIAX_P_LOG);
  }
  return 0;
}

/*! \brief  Answer incoming call,
 * Part of PBX interface */
int celliax_answer(struct ast_channel *c)
{
  struct celliax_pvt *p = c->tech_pvt;
  int res;

  if (option_debug) {
    DEBUGA_PBX("ENTERING FUNC\n", CELLIAX_P_LOG);
  }
  /* do something to actually answer the call, if needed (eg. pick up the phone) */
  if (p->controldevprotocol != PROTOCOL_NO_SERIAL) {
    if (celliax_serial_answer(p)) {
      ERRORA("celliax_answer FAILED\n", CELLIAX_P_LOG);
      if (option_debug) {
        DEBUGA_PBX("EXITING FUNC\n", CELLIAX_P_LOG);
      }
      return -1;
    }
  }
  p->interface_state = AST_STATE_UP;
  p->phone_callflow = CALLFLOW_CALL_ACTIVE;

  while (p->interface_state == AST_STATE_RING) {
    usleep(10000);              //10msec
  }
  if (p->interface_state != AST_STATE_UP) {
    ERRORA("call answering failed\n", CELLIAX_P_LOG);
    res = -1;
  } else {
    if (option_debug)
      DEBUGA_PBX("call answered\n", CELLIAX_P_LOG);
    res = 0;
#ifdef CELLIAX_PORTAUDIO
    speex_echo_state_reset(p->stream->echo_state);
#endif // CELLIAX_PORTAUDIO

    if (p->owner) {
      DEBUGA_PBX("going to send AST_STATE_UP\n", CELLIAX_P_LOG);
      ast_setstate(p->owner, AST_STATE_UP);
      //ast_queue_control(p->owner, AST_CONTROL_ANSWER);
      //celliax_queue_control(p->owner, AST_CONTROL_ANSWER);
      DEBUGA_PBX("just sent AST_STATE_UP\n", CELLIAX_P_LOG);
    }
  }
  if (option_debug) {
    DEBUGA_PBX("EXITING FUNC\n", CELLIAX_P_LOG);
  }
  return res;
}

#ifdef ASTERISK_VERSION_1_4
int celliax_senddigit_begin(struct ast_channel *c, char digit)
{
  struct celliax_pvt *p = c->tech_pvt;

  DEBUGA_PBX("DIGIT BEGIN received: %c\n", CELLIAX_P_LOG, digit);

  return 0;
}

int celliax_senddigit_end(struct ast_channel *c, char digit, unsigned int duration)
{
  struct celliax_pvt *p = c->tech_pvt;

  NOTICA("DIGIT END received: %c %d\n", CELLIAX_P_LOG, digit, duration);

  if (p->controldevprotocol == PROTOCOL_AT && p->at_send_dtmf[0]) {
    int res = 0;
    char at_command[256];

    memset(at_command, '\0', 256);
    sprintf(at_command, "%s=\"%c\"", p->at_send_dtmf, digit);
    res = celliax_serial_write_AT_ack(p, at_command);
    if (res) {
      ERRORA("senddigit command failed, command used: '%s=\"%c\"', giving up\n",
             CELLIAX_P_LOG, p->at_send_dtmf, digit);
    }
  }
  return 0;
}
#else /* ASTERISK_VERSION_1_4 */
int celliax_senddigit(struct ast_channel *c, char digit)
{
  struct celliax_pvt *p = c->tech_pvt;

  NOTICA("DIGIT received: %c\n", CELLIAX_P_LOG, digit);

  if (p->controldevprotocol == PROTOCOL_AT && p->at_send_dtmf[0]) {
    int res = 0;
    char at_command[256];

    memset(at_command, '\0', 256);
    sprintf(at_command, "%s=\"%c\"", p->at_send_dtmf, digit);
    res = celliax_serial_write_AT_ack(p, at_command);
    if (res) {
      ERRORA("senddigit command failed, command used: '%s=\"%c\"', giving up\n",
             CELLIAX_P_LOG, p->at_send_dtmf, digit);
    }
  }
  return 0;
}

#endif /* ASTERISK_VERSION_1_4 */

/*! \brief Read audio frames from channel */
struct ast_frame *celliax_read(struct ast_channel *c)
{
  struct ast_frame *f;
  struct celliax_pvt *p = c->tech_pvt;
  int actual;
  char buf[128 + 1];

  if (p->dtmf_inited == 0) {
    dtmf_rx_init(&p->dtmf_state, NULL, NULL);
    p->dtmf_inited = 1;
    dtmf_rx_parms(&p->dtmf_state, 0, 10, 10);
    p->dtmf_timestamp.tv_sec=0;
    p->dtmf_timestamp.tv_usec=0;
    DEBUGA_SOUND("DTMF recognition inited\n", CELLIAX_P_LOG);
  }

/* if there are control frames queued to be sent by celliax_queue_control, send it the first */
//FIXME maybe better a real queue?
  if (p && p->owner && p->control_to_send) {
    ast_queue_control(p->owner, p->control_to_send);
    DEBUGA_PBX("Sent CONTROL FRAME %d\n", CELLIAX_P_LOG, p->control_to_send);
    p->control_to_send = 0;
  }

#ifdef CELLIAX_PORTAUDIO
/* if the call is not active (ie: answered), do not send audio frames, they would pile up in a lag queue */
  if (!p->owner || p->owner->_state != AST_STATE_UP) 
#else /* CELLIAX_PORTAUDIO */
  if (!p->owner ) 
#endif /* CELLIAX_PORTAUDIO */
  {
    static struct ast_frame f;
#ifdef CELLIAX_PORTAUDIO
    char c;
#endif /* CELLIAX_PORTAUDIO */

    f.frametype = AST_FRAME_NULL;
    f.subclass = 0;
    f.samples = 0;
    f.datalen = 0;
#ifdef ASTERISK_VERSION_1_6_0_1
    f.data.ptr = NULL;
#else
    f.data = NULL;
#endif /* ASTERISK_VERSION_1_6_0_1 */
    f.offset = 0;
    f.src = celliax_type;
    f.mallocd = 0;
    f.delivery.tv_sec = 0;
    f.delivery.tv_usec = 0;
/* read the char that was written by the audio thread in this pipe, this pipe is the fd monitored by asterisk, asterisk then has called the function we are inside) */
#ifdef CELLIAX_PORTAUDIO
    read(p->audiopipe[0], &c, 1);
#endif /* CELLIAX_PORTAUDIO */

    return &f;
  }

  /* read one asterisk frame of audio from sound interface */
  f = celliax_sound_read(p);
  if (f) {
  struct timeval now_timestamp;
#ifndef __CYGWIN__
#ifdef CELLIAX_PORTAUDIO
    char c[1000];
    int letti = 2;

    while (letti > 1) {
      letti = read(p->audiopipe[0], &c, 1000);
      if (letti > 0)
        DEBUGA_SOUND("READ from audiopipe: %d\n", CELLIAX_P_LOG, letti);
      //usleep(1);
    }
    //if(letti == -1)
    //ERRORA("error: %s\n", CELLIAX_P_LOG, strerror(errno));
#endif /* CELLIAX_PORTAUDIO */
#endif /* __CYGWIN__ */

    /* scale sound samples volume up or down */
    celliax_sound_boost(f, p->capture_boost);

  gettimeofday(&now_timestamp, NULL);

  if( (((now_timestamp.tv_sec - p->dtmf_timestamp.tv_sec) * 1000000) < 0) || ( ((now_timestamp.tv_sec - p->dtmf_timestamp.tv_sec) * 1000000) + (now_timestamp.tv_usec - p->dtmf_timestamp.tv_usec) ) > 300000) { // if more than 0.3 seconds from last DTMF, or never got DTMFs before

#ifdef ASTERISK_VERSION_1_6_0_1
    dtmf_rx(&p->dtmf_state, f->data.ptr, f->samples);
#else
    dtmf_rx(&p->dtmf_state, f->data, f->samples);
#endif /* ASTERISK_VERSION_1_6_0_1 */
    actual = dtmf_rx_get(&p->dtmf_state, buf, 128);
    if (actual) {
      //if (option_debug)
        NOTICA("delta_usec=%ld, inband audio DTMF: %s\n", CELLIAX_P_LOG, ( (now_timestamp.tv_sec - p->dtmf_timestamp.tv_sec) * 1000000) + (now_timestamp.tv_usec - p->dtmf_timestamp.tv_usec), buf);
      struct ast_frame f2 = { AST_FRAME_DTMF, buf[0], };
      ast_queue_frame(p->owner, &f2);
  	gettimeofday(&p->dtmf_timestamp, NULL);
    }
  }
    return f;
  }
  return NULL;
}

/*! \brief Initiate celliax call from PBX 
 * used from the dial() application
 */
int celliax_call(struct ast_channel *c, char *idest, int timeout)
{
  struct celliax_pvt *p = NULL;
  p = c->tech_pvt;
  char rdest[80], *where, dstr[100] = "";
  char *stringp = NULL;
  int status;

  if (option_debug) {
    DEBUGA_PBX("ENTERING FUNC\n", CELLIAX_P_LOG);
  }
  if ((c->_state != AST_STATE_DOWN)
      && (c->_state != AST_STATE_RESERVED)) {
    ERRORA("celliax_call called on %s, neither down nor reserved\n", CELLIAX_P_LOG,
           c->name);
    if (option_debug) {
      DEBUGA_PBX("EXITING FUNC\n", CELLIAX_P_LOG);
    }
    return -1;
  }

  if (option_debug > 1)
    DEBUGA_PBX("celliax_call to call idest: %s, timeout: %d!\n", CELLIAX_P_LOG, idest,
               timeout);

  strncpy(rdest, idest, sizeof(rdest) - 1);
  // try '/' as separator
  stringp = rdest;
  strsep(&stringp, "/");
  where = strsep(&stringp, "/");

  if (!where) {
    ERRORA
      ("Destination %s is not recognized. Chan_celliax requires a standard destination with slashes (Celliax/device/destination, eg: 'Celliax/nicephone/3472665618')\n",
       CELLIAX_P_LOG, idest);
    if (option_debug) {
      DEBUGA_PBX("EXITING FUNC\n", CELLIAX_P_LOG);
    }
    return -1;
  }

  strncpy(dstr, where + p->stripmsd, sizeof(dstr) - 1);
  if (option_debug > 1)
    DEBUGA_PBX("celliax_call dialing idest: %s, timeout: %d, dstr: %s!\n", CELLIAX_P_LOG,
               idest, timeout, dstr);

  if (p->controldev_dead) {
    WARNINGA("celliax_call: device is dead, cannot call!\n", CELLIAX_P_LOG);
    status = -1;
  } else {
    ast_setstate(c, AST_STATE_DIALING);
    status = celliax_serial_call(p, dstr);
  }

  if (status) {
    WARNINGA("celliax_call dialing failed: %d!\n", CELLIAX_P_LOG, status);
    if (option_debug) {
      DEBUGA_PBX("EXITING FUNC\n", CELLIAX_P_LOG);
    }
    return -1;
  } else {
    if (option_debug)
      DEBUGA_PBX("call ongoing\n", CELLIAX_P_LOG);
    ast_queue_control(p->owner, AST_CONTROL_RINGING);
  }

  if (option_debug > 1)
    DEBUGA_PBX("celliax_call dialed idest: %s, timeout: %d, dstr: %s!\n", CELLIAX_P_LOG,
               idest, timeout, dstr);

  if (option_debug) {
    DEBUGA_PBX("EXITING FUNC\n", CELLIAX_P_LOG);
  }
#ifdef CELLIAX_PORTAUDIO
  speex_echo_state_reset(p->stream->echo_state);
#endif // CELLIAX_PORTAUDIO
  return 0;
}

/*! \brief Send audio frame to channel */
int celliax_write(struct ast_channel *c, struct ast_frame *f)
{
  struct celliax_pvt *p = c->tech_pvt;
  if (p->owner && p->owner->_state != AST_STATE_UP) {
    return 0;
  }

  celliax_sound_boost(f, p->playback_boost);

  return celliax_sound_write(p, f);
}

/*! \brief  Fix up a channel:  If a channel is consumed, this is called.
 * Basically update any ->owner links */
int celliax_fixup(struct ast_channel *oldchan, struct ast_channel *newchan)
{
  struct celliax_pvt *p = newchan->tech_pvt;

  if (!p) {
    ERRORA("No pvt after masquerade. Strange things may happen\n", CELLIAX_P_LOG);
    return -1;
  }

  if (p->owner != oldchan) {
    ERRORA("old channel wasn't %p but was %p\n", CELLIAX_P_LOG, oldchan, p->owner);
    return -1;
  }

  p->owner = newchan;
  return 0;
}

int celliax_sound_boost(struct ast_frame *f, double boost)
{
/* LUIGI RIZZO's magic */
  if (boost != 0) {             /* scale and clip values */
    int i, x;

#ifdef ASTERISK_VERSION_1_6_0_1
    int16_t *ptr = (int16_t *) f->data.ptr;
#else
    int16_t *ptr = (int16_t *) f->data;
#endif /* ASTERISK_VERSION_1_6_0_1 */
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

struct ast_channel *celliax_new(struct celliax_pvt *p, int state, char *context)
{
  struct ast_channel *tmp;

  if (option_debug) {
    DEBUGA_PBX("ENTERING FUNC\n", CELLIAX_P_LOG);
  }
  /* alloc a generic channel struct */
#ifndef ASTERISK_VERSION_1_4
  tmp = ast_channel_alloc(1);
#else
  //tmp = ast_channel_alloc(1, state, 0, 0, "", p->exten, p->context, 0, "");
  tmp =
    ast_channel_alloc(1, state, 0, 0, "", p->exten, p->context, 0, "Celliax/%s", p->name);

#endif /* ASTERISK_VERSION_1_4 */
  if (tmp) {
int res;

/* initialize the soundcard channels (input and output) used by this interface (a multichannel soundcard can be used by multiple interfaces), optionally starting the sound managing threads */
  res = celliax_sound_init(p);
  if (res == -1) {
    ERRORA("Failed initializing sound device\n", CELLIAX_P_LOG);
    /* we failed, free the PVT */
    if (tmp)
      free(tmp);
    return NULL;
  }

    /* give a name to the newly created channel */
#ifndef ASTERISK_VERSION_1_4
    snprintf(tmp->name, sizeof(tmp->name), "Celliax/%s", p->name);
    tmp->type = celliax_type;
#else /* ASTERISK_VERSION_1_4 */
    ast_string_field_build(tmp, name, "Celliax/%s", p->name);
#endif /* ASTERISK_VERSION_1_4 */

    DEBUGA_PBX("new channel: name=%s requested_state=%d\n", CELLIAX_P_LOG, tmp->name,
               state);

    /* fd for the channel to poll for incoming audio */
    tmp->fds[0] = p->celliax_sound_capt_fd;

    /* audio formats managed */
    tmp->nativeformats = AST_FORMAT_SLINEAR;
    tmp->readformat = AST_FORMAT_SLINEAR;
    tmp->writeformat = AST_FORMAT_SLINEAR;
    /* the technology description (eg. the interface type) of the newly created channel is the Celliax's one */
    tmp->tech = &celliax_tech;
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
                     !ast_strlen_zero(p->callid_name) ? p->callid_name : NULL,
                     !ast_strlen_zero(p->callid_number) ? p->callid_number : NULL);

    /* the owner of this interface pvt is the newly created channel */
    p->owner = tmp;
    /* if this interface pvt has an initialized dsp struct, free it */
    if (p->dsp) {
      DEBUGA_SOUND("freeing dsp\n", CELLIAX_P_LOG);
      ast_dsp_free(p->dsp);
      p->dsp = NULL;
    }
#ifndef ASTERISK_VERSION_1_4
    /* set the newly created channel state to the requested state */
    if (ast_setstate(tmp, state)) {
      ERRORA("ast_setstate failed, BAD\n", CELLIAX_P_LOG);
      ast_dsp_free(p->dsp);
      ast_channel_free(tmp);
      if (option_debug) {
        DEBUGA_PBX("EXITING FUNC\n", CELLIAX_P_LOG);
      }
      return NULL;
    }
#endif /* ASTERISK_VERSION_1_4 */

#ifdef AST_VERION_1_4
	ast_module_ref(ast_module_info->self);
	ast_jb_configure(tmp, &global_jbconf);
#endif /* AST_VERION_1_4 */

    /* if the requested state is different from DOWN, let the pbx manage this interface (now part of the newly created channel) */
    if (state != AST_STATE_DOWN) {
      DEBUGA_PBX("Try to start PBX on %s, state=%d\n", CELLIAX_P_LOG, tmp->name, state);
      if (ast_pbx_start(tmp)) {
        ERRORA("Unable to start PBX on %s\n", CELLIAX_P_LOG, tmp->name);
        ast_dsp_free(p->dsp);
        ast_channel_free(tmp);
        if (option_debug) {
          DEBUGA_PBX("EXITING FUNC\n", CELLIAX_P_LOG);
        }
        return NULL;
      }
    }
    /* let's start the serial monitoring thread too, so we can have serial signaling */
    if (ast_pthread_create(&p->controldev_thread, NULL, celliax_do_controldev_thread, p) <
        0) {
      ERRORA("Unable to start controldev thread.\n", CELLIAX_P_LOG);
      ast_dsp_free(p->dsp);
      ast_channel_free(tmp);
      tmp = NULL;
    }
    DEBUGA_SERIAL("STARTED controldev_thread=%lu STOP=%lu NULL=%lu\n", CELLIAX_P_LOG,
                  (unsigned long) p->controldev_thread, (unsigned long) AST_PTHREADT_STOP,
                  (unsigned long) AST_PTHREADT_NULL);

#ifndef ASTERISK_VERSION_1_4
    /* add one to the usage count of Celliax-type channels */
    LOKKA(&celliax_usecnt_lock);
    celliax_usecnt++;
    UNLOCKA(&celliax_usecnt_lock);
    ast_update_use_count();
#endif /* ASTERISK_VERSION_1_4 */

    /* return the newly created channel */
    if (option_debug) {
      DEBUGA_PBX("EXITING FUNC\n", CELLIAX_P_LOG);
    }
    return tmp;
  }
  ERRORA("failed memory allocation for Celliax channel\n", CELLIAX_P_LOG);
  if (option_debug) {
    DEBUGA_PBX("EXITING FUNC\n", CELLIAX_P_LOG);
  }
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
  struct celliax_pvt *tmp;
  struct celliax_pvt *p = NULL;
#ifdef ASTERISK_VERSION_1_6_0
  struct ast_flags config_flags = { 0 };
#endif /* ASTERISK_VERSION_1_6_0 */



#ifdef ASTERISK_VERSION_1_4
	/* Copy the default jb config over global_jbconf */
	memcpy(&global_jbconf, &default_jbconf, sizeof(struct ast_jb_conf));
#endif /* ASTERISK_VERSION_1_4 */

  if (option_debug) {
    DEBUGA_PBX("ENTERING FUNC\n", CELLIAX_P_LOG);
  }
  ast_register_application(celliax_sendsmsapp, celliax_sendsms, celliax_sendsmssynopsis,
                           celliax_sendsmsdescrip);

  ast_manager_register2("CELLIAXsendsms", EVENT_FLAG_SYSTEM, celliax_manager_sendsms,
                        "Send an SMS", mandescr_celliax_sendsms);
  /* make sure we can register our channel type with Asterisk */
  i = ast_channel_register(&celliax_tech);
  if (i < 0) {
    ERRORA("Unable to register channel type '%s'\n", CELLIAX_P_LOG, celliax_type);
    if (option_debug) {
      DEBUGA_PBX("EXITING FUNC\n", CELLIAX_P_LOG);
    }
    return -1;
  }
  /* load celliax.conf config file */
#ifdef ASTERISK_VERSION_1_6_0
  cfg = ast_config_load(celliax_config, config_flags);
#else
  cfg = ast_config_load(celliax_config);
#endif /* ASTERISK_VERSION_1_6_0 */
  if (cfg != NULL) {
    char *ctg = NULL;
    int is_first_category = 1;
    while ((ctg = ast_category_browse(cfg, ctg)) != NULL) {
      /* create one interface for each category in celliax.conf config file, first one set the defaults */
      tmp = celliax_mkif(cfg, ctg, is_first_category);
      if (tmp) {
        NOTICA("Created channel Celliax: celliax.conf category '[%s]', channel name '%s'"
               " control_device_name '%s'\n", CELLIAX_P_LOG, ctg, tmp->name,
               tmp->controldevice_name);
        /* add interface to celliax_iflist */
        tmp->next = celliax_iflist;
        celliax_iflist = tmp;
        /* next one will not be the first ;) */
        if (is_first_category == 1) {
          is_first_category = 0;
          celliax_console_active = tmp->name;
        }
      } else {
        ERRORA("Unable to create channel Celliax from celliax.conf category '[%s]'\n",
               CELLIAX_P_LOG, ctg);
        /* if error, unload config from memory and return */
        ast_config_destroy(cfg);
        ast_channel_unregister(&celliax_tech);
        if (option_debug) {
          DEBUGA_PBX("EXITING FUNC\n", CELLIAX_P_LOG);
        }
        return -1;
      }
      /* do it for each category described in config */
    }

    /* we finished, unload config from memory */
    ast_config_destroy(cfg);
  } else {
    ERRORA("Unable to load celliax_config celliax.conf\n", CELLIAX_P_LOG);
    ast_channel_unregister(&celliax_tech);
    if (option_debug) {
      DEBUGA_PBX("EXITING FUNC\n", CELLIAX_P_LOG);
    }
    return -1;
  }
#ifndef ASTERISK_VERSION_1_6_0
  ast_cli_register_multiple(myclis, sizeof(myclis) / sizeof(struct ast_cli_entry));
#endif /* ASTERISK_VERSION_1_6_0 */
  /* start to monitor the interfaces (celliax_iflist) for the first time */
  if (celliax_restart_monitor()) {
    ERRORA("celliax_restart_monitor failed, BAD\n", CELLIAX_P_LOG);
    if (option_debug) {
      DEBUGA_PBX("EXITING FUNC\n", CELLIAX_P_LOG);
    }
    return -1;
  }
#ifdef CELLIAX_DIR
  //celliax_dir_create_extensions();
#endif /* CELLIAX_DIR */
  if (option_debug) {
    DEBUGA_PBX("EXITING FUNC\n", CELLIAX_P_LOG);
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
  struct celliax_pvt *p = NULL, *p2 = NULL;
  int res;

  if (option_debug) {
    DEBUGA_PBX("ENTERING FUNC\n", CELLIAX_P_LOG);
  }

  /* unregister our channel type with Asterisk */
  ast_channel_unregister(&celliax_tech);
  ast_cli_unregister_multiple(myclis, sizeof(myclis) / sizeof(struct ast_cli_entry));

  ast_unregister_application(celliax_sendsmsapp);

  /* lock the celliax_monlock, kill the monitor thread, unlock the celliax_monlock */
  LOKKA(&celliax_monlock);
  if (celliax_monitor_thread && (celliax_monitor_thread != AST_PTHREADT_NULL)
      && (celliax_monitor_thread != AST_PTHREADT_STOP)) {
    if (pthread_cancel(celliax_monitor_thread)) {
      ERRORA("pthread_cancel failed, BAD\n", CELLIAX_P_LOG);
      if (option_debug) {
        DEBUGA_PBX("EXITING FUNC\n", CELLIAX_P_LOG);
      }
      return -1;
    }
    if (pthread_kill(celliax_monitor_thread, SIGURG)) {
      DEBUGA_PBX("pthread_kill failed\n", CELLIAX_P_LOG);   //maybe it just died
    }
#ifndef __CYGWIN__              /* under cygwin, this seems to be not reliable, get stuck at times */
    if (pthread_join(celliax_monitor_thread, NULL)) {
      ERRORA("pthread_join failed, BAD\n", CELLIAX_P_LOG);
      if (option_debug) {
        DEBUGA_PBX("EXITING FUNC\n", CELLIAX_P_LOG);
      }
      return -1;
    }
#endif /* __CYGWIN__ */
  }
  celliax_monitor_thread = AST_PTHREADT_STOP;
  UNLOCKA(&celliax_monlock);

  if (celliax_monitor_audio_thread && (celliax_monitor_audio_thread != AST_PTHREADT_NULL)
      && (celliax_monitor_audio_thread != AST_PTHREADT_STOP)) {

    if (pthread_cancel(celliax_monitor_audio_thread)) {
      ERRORA("pthread_cancel celliax_monitor_audio_thread failed, BAD\n", CELLIAX_P_LOG);
    }
    if (pthread_kill(celliax_monitor_audio_thread, SIGURG)) {
      DEBUGA_PBX("pthread_kill celliax_monitor_audio_thread failed, no problem\n", CELLIAX_P_LOG);  //maybe it just died
    }

    if (pthread_join(celliax_monitor_audio_thread, NULL)) {
      ERRORA("pthread_join failed, BAD\n", CELLIAX_P_LOG);
    }
  }
  /* lock the celliax_iflock, and go through the interfaces list (celliax_iflist) */
  LOKKA(&celliax_iflock);
  p = celliax_iflist;
  while (p) {
    /* for each interface in list */
    p2 = p->next;
    /* shutdown the sound system, close sound fds, and if exist shutdown the sound managing threads */
    DEBUGA_SOUND("shutting down sound\n", CELLIAX_P_LOG);
    res = celliax_sound_shutdown(p);
    if (res == -1) {
      ERRORA("Failed to shutdown sound\n", CELLIAX_P_LOG);
    }

    /* if a serial port has been opened, close it */
    if (p->controldevprotocol != PROTOCOL_NO_SERIAL)
      if (p->controldevfd)
        close(p->controldevfd);

    /* if a dsp struct has been allocated, free it */
    if (p->dsp) {
      ast_dsp_free(p->dsp);
      p->dsp = NULL;
    }
    DEBUGA_PBX("freeing PVT\n", CELLIAX_P_LOG);
    /* free the pvt allocated memory */
    free(p);
    /* next one, please */
    p = p2;
  }
  /* finished with the interfaces list, unlock the celliax_iflock */
  UNLOCKA(&celliax_iflock);

#ifdef __CYGWIN__
  NOTICA("Sleping 5 secs, please wait...\n", CELLIAX_P_LOG);
  sleep(5);                     /* without this pause, for some unknown (to me) reason it crashes on cygwin */
#endif /* __CYGWIN__ */
  NOTICA("Unloaded Celliax Module\n", CELLIAX_P_LOG);
  if (option_debug) {
    DEBUGA_PBX("EXITING FUNC\n", CELLIAX_P_LOG);
  }
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
  static struct celliax_pvt *p = &celliax_log_struct;
/* lock the celliax_usecnt lock */
  LOKKA(&celliax_usecnt_lock);
  /* retrieve the celliax_usecnt */
  res = celliax_usecnt;
/* unlock the celliax_usecnt lock */
  UNLOCKA(&celliax_usecnt_lock);
  /* return the celliax_usecnt */
  return res;
}

/*!
 * \brief Return the textual description of the module
 *
 * \return the textual description of the module
 */
char *description()
{
  return (char *) celliax_desc;
}

/*!
 * \brief Return the ASTERISK_GPL_KEY
 *
 * \return the ASTERISK_GPL_KEY
 */
char *key()
{
  struct celliax_pvt *p = NULL;

  if (option_debug)
    NOTICA("Returning Key\n", CELLIAX_P_LOG);

  return ASTERISK_GPL_KEY;
}

/*!
 * \brief Create and initialize one interface for the module
 * \param cfg pointer to configuration data from celliax.conf
 * \param ctg pointer to a category name to be found in cfg
 * \param is_first_category is this the first category in cfg
 *
 * This function create and initialize one interface for the module
 *
 * \return a pointer to the PVT structure of interface on success, NULL on error.
 */
struct celliax_pvt *celliax_mkif(struct ast_config *cfg, char *ctg, int is_first_category)
{
  struct celliax_pvt *tmp;
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
#ifdef CELLIAX_CVM
  int debug_cvm = 0;
#endif /* CELLIAX_CVM */

  /* alloc memory for PVT */
  tmp = malloc(sizeof(struct celliax_pvt));
  if (tmp == NULL)              /* fail */
    return NULL;
  /* clear memory for PVT */
  memset(tmp, 0, sizeof(struct celliax_pvt));

  //NOTICA("malloced %d bytes\n", CELLIAX_TMP_LOG, sizeof(struct celliax_pvt));

  /* if we are reading the "first" category of the config file, take SELECTED values as defaults, overriding the values in celliax_default */
  if (is_first_category == 1) {
    /* for each variable in category, copy it in the celliax_default struct */
    for (v = ast_variable_browse(cfg, ctg); v; v = v->next) {
      M_START(v->name, v->value);

      M_STR("control_device_protocol", celliax_default.controldevprotocolname)
        M_STR("context", celliax_default.context)
        M_STR("language", celliax_default.language)
        M_STR("extension", celliax_default.exten)
        M_UINT("dsp_silence_threshold", celliax_default.dsp_silence_threshold)
        M_UINT("audio_play_reset_period", celliax_default.audio_play_reset_period)
#ifdef CELLIAX_ALSA
        M_UINT("alsa_period_size", celliax_default.alsa_period_size)
        M_UINT("alsa_periods_in_buffer", celliax_default.alsa_periods_in_buffer)
#endif /* CELLIAX_ALSA */
        M_F("playback_boost",
            celliax_store_boost(v->value, &celliax_default.playback_boost))
        M_F("capture_boost",
            celliax_store_boost(v->value, &celliax_default.capture_boost))
        M_UINT("celliax_dir_entry_extension_prefix",
               celliax_default.celliax_dir_entry_extension_prefix)
        M_UINT("celliax_dir_prefix", celliax_default.celliax_dir_prefix)
        M_STR("sms_receiving_program", tmp->sms_receiving_program)
        M_END(;
        );
    }
  }

  /* initialize the newly created PVT from the celliax_default values */
  *tmp = celliax_default;

  /* initialize the mutexes */
  ast_mutex_init(&tmp->controldev_lock);
  ast_mutex_init(&tmp->fbus2_outgoing_list_lock);
#ifdef CELLIAX_CVM
  ast_mutex_init(&tmp->cvm_busmail_outgoing_list_lock);
#endif /* CELLIAX_CVM */

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
#ifdef CELLIAX_CVM
      M_BOOL("debug_cvm", debug_cvm)
      M_STR("cvm_subscription_1_pin", tmp->cvm_subsc_1_pin)
      M_STR("cvm_subscription_2_pin", tmp->cvm_subsc_2_pin)
      M_UINT("cvm_subscription_no", tmp->cvm_subsc_no)
      M_UINT("cvm_volume_level", tmp->cvm_volume_level)
      M_UINT("cvm_celliax_serial_delay", tmp->cvm_celliax_serial_delay)
#endif /* CELLIAX_CVM */
      M_BOOL("skype", tmp->skype)
      M_BOOL("need_acoustic_ring", tmp->need_acoustic_ring)
      M_STR("control_device_name", tmp->controldevice_name)
      M_UINT("control_device_speed", tmp->controldevice_speed)
      M_STR("control_device_protocol", tmp->controldevprotocolname)
      M_STR("context", tmp->context)
      M_STR("language", tmp->language)
      M_STR("extension", tmp->exten)
      M_UINT("dsp_silence_threshold", tmp->dsp_silence_threshold)
      M_UINT("audio_play_reset_period", tmp->audio_play_reset_period)
      M_UINT("portaudio_capture_device_id", tmp->portaudiocindex)
      M_UINT("portaudio_playback_device_id", tmp->portaudiopindex)
      M_F("playback_boost", celliax_store_boost(v->value, &tmp->playback_boost))
      M_F("capture_boost", celliax_store_boost(v->value, &tmp->capture_boost))
#ifdef CELLIAX_ALSA
      M_STR("alsa_capture_device_name", tmp->alsacname)
      M_STR("alsa_playback_device_name", tmp->alsapname)
      M_UINT("alsa_period_size", tmp->alsa_period_size)
      M_UINT("alsa_periods_in_buffer", tmp->alsa_periods_in_buffer)
#endif /* CELLIAX_WINMM */
      M_STR("at_dial_pre_number", tmp->at_dial_pre_number)
      M_STR("at_dial_post_number", tmp->at_dial_post_number)

      M_STR("at_dial_expect", tmp->at_dial_expect)
      M_UINT("at_early_audio", tmp->at_early_audio)
      M_STR("at_hangup", tmp->at_hangup)
      M_STR("at_hangup_expect", tmp->at_hangup_expect)
      M_STR("at_answer", tmp->at_answer)
      M_STR("at_answer_expect", tmp->at_answer_expect)
      M_STR("at_send_dtmf", tmp->at_send_dtmf)

      M_UINT("at_initial_pause", tmp->at_initial_pause)
      M_STR("at_preinit_1", tmp->at_preinit_1)
      M_STR("at_preinit_1_expect", tmp->at_preinit_1_expect)
      M_STR("at_preinit_2", tmp->at_preinit_2)
      M_STR("at_preinit_2_expect", tmp->at_preinit_2_expect)
      M_STR("at_preinit_3", tmp->at_preinit_3)
      M_STR("at_preinit_3_expect", tmp->at_preinit_3_expect)
      M_STR("at_preinit_4", tmp->at_preinit_4)
      M_STR("at_preinit_4_expect", tmp->at_preinit_4_expect)
      M_STR("at_preinit_5", tmp->at_preinit_5)
      M_STR("at_preinit_5_expect", tmp->at_preinit_5_expect)
      M_UINT("at_after_preinit_pause", tmp->at_after_preinit_pause)

      M_STR("at_postinit_1", tmp->at_postinit_1)
      M_STR("at_postinit_1_expect", tmp->at_postinit_1_expect)
      M_STR("at_postinit_2", tmp->at_postinit_2)
      M_STR("at_postinit_2_expect", tmp->at_postinit_2_expect)
      M_STR("at_postinit_3", tmp->at_postinit_3)
      M_STR("at_postinit_3_expect", tmp->at_postinit_3_expect)
      M_STR("at_postinit_4", tmp->at_postinit_4)
      M_STR("at_postinit_4_expect", tmp->at_postinit_4_expect)
      M_STR("at_postinit_5", tmp->at_postinit_5)
      M_STR("at_postinit_5_expect", tmp->at_postinit_5_expect)

      M_STR("at_query_battchg", tmp->at_query_battchg)
      M_STR("at_query_battchg_expect", tmp->at_query_battchg_expect)
      M_STR("at_query_signal", tmp->at_query_signal)
      M_STR("at_query_signal_expect", tmp->at_query_signal_expect)
      M_STR("at_call_idle", tmp->at_call_idle)
      M_STR("at_call_incoming", tmp->at_call_incoming)
      M_STR("at_call_active", tmp->at_call_active)
      M_STR("at_call_failed", tmp->at_call_failed)
      M_STR("at_call_calling", tmp->at_call_calling)
      M_STR("at_indicator_noservice_string", tmp->at_indicator_noservice_string)
      M_STR("at_indicator_nosignal_string", tmp->at_indicator_nosignal_string)
      M_STR("at_indicator_lowsignal_string", tmp->at_indicator_lowsignal_string)
      M_STR("at_indicator_lowbattchg_string", tmp->at_indicator_lowbattchg_string)
      M_STR("at_indicator_nobattchg_string", tmp->at_indicator_nobattchg_string)
      M_STR("at_indicator_callactive_string", tmp->at_indicator_callactive_string)
      M_STR("at_indicator_nocallactive_string", tmp->at_indicator_nocallactive_string)
      M_STR("at_indicator_nocallsetup_string", tmp->at_indicator_nocallsetup_string)
      M_STR("at_indicator_callsetupincoming_string",
            tmp->at_indicator_callsetupincoming_string)
      M_STR("at_indicator_callsetupoutgoing_string",
            tmp->at_indicator_callsetupoutgoing_string)
      M_STR("at_indicator_callsetupremoteringing_string",
            tmp->at_indicator_callsetupremoteringing_string)
      M_UINT("celliax_dir_entry_extension_prefix",
             tmp->celliax_dir_entry_extension_prefix)
      M_UINT("celliax_dir_prefix", tmp->celliax_dir_prefix)
#ifdef CELLIAX_LIBCSV
      M_UINT("csv_separator_is_semicolon", tmp->csv_separator_is_semicolon)
      M_UINT("csv_complete_name_pos", tmp->csv_complete_name_pos)
      M_UINT("csv_email_pos", tmp->csv_email_pos)
      M_UINT("csv_home_phone_pos", tmp->csv_home_phone_pos)
      M_UINT("csv_mobile_phone_pos", tmp->csv_mobile_phone_pos)
      M_UINT("csv_business_phone_pos", tmp->csv_business_phone_pos)
      M_UINT("csv_first_row_is_title", tmp->csv_first_row_is_title)
#endif /* CELLIAX_LIBCSV */
      M_STR("sms_receiving_program", tmp->sms_receiving_program)
      M_BOOL("speexecho", tmp->speexecho)
      M_BOOL("speexpreprocess", tmp->speexpreprocess)
      M_END(;
      );
  }

  if (debug_all) {
    celliax_debug = celliax_debug | DEBUG_ALL;
    if (!option_debug) {
      WARNINGA
        ("DEBUG_ALL activated, but option_debug is 0. You have to set debug level higher than zero to see some debugging output. Please use the command \"set debug 10\" or start Asterisk with \"-dddddddddd\" option for full DEBUG_ALL debugging output.\n",
         CELLIAX_TMP_LOG);
    } else {
      NOTICA("DEBUG_ALL activated. \n", CELLIAX_TMP_LOG);
    }
  }
  if (debug_at) {
    celliax_debug = celliax_debug | DEBUG_AT;
    if (!option_debug) {
      WARNINGA
        ("DEBUG_AT activated, but option_debug is 0. You have to set debug level higher than zero to see some debugging output. Please use the command \"set debug 10\" or start Asterisk with \"-dddddddddd\" option for full DEBUG_AT debugging output.\n",
         CELLIAX_TMP_LOG);
    } else {
      NOTICA("DEBUG_AT activated. \n", CELLIAX_TMP_LOG);
    }
  }

  if (debug_fbus2) {
    celliax_debug = celliax_debug | DEBUG_FBUS2;
    if (!option_debug) {
      WARNINGA
        ("DEBUG_FBUS2 activated, but option_debug is 0. You have to set debug level higher than zero to see some debugging output. Please use the command \"set debug 10\" or start Asterisk with \"-dddddddddd\" option for full DEBUG_FBUS2 debugging output.\n",
         CELLIAX_TMP_LOG);
    } else {
      NOTICA("DEBUG_FBUS2 activated. \n", CELLIAX_TMP_LOG);
    }
  }

  if (debug_serial) {
    celliax_debug = celliax_debug | DEBUG_SERIAL;
    if (!option_debug) {
      WARNINGA
        ("DEBUG_SERIAL activated, but option_debug is 0. You have to set debug level higher than zero to see some debugging output. Please use the command \"set debug 10\" or start Asterisk with \"-dddddddddd\" option for full DEBUG_SERIAL debugging output.\n",
         CELLIAX_TMP_LOG);
    } else {
      NOTICA("DEBUG_SERIAL activated. \n", CELLIAX_TMP_LOG);
    }
  }

  if (debug_sound) {
    celliax_debug = celliax_debug | DEBUG_SOUND;
    if (!option_debug) {
      WARNINGA
        ("DEBUG_SOUND activated, but option_debug is 0. You have to set debug level higher than zero to see some debugging output. Please use the command \"set debug 10\" or start Asterisk with \"-dddddddddd\" option for full DEBUG_SOUND debugging output.\n",
         CELLIAX_TMP_LOG);
    } else {
      NOTICA("DEBUG_SOUND activated. \n", CELLIAX_TMP_LOG);
    }
  }

  if (debug_pbx) {
    celliax_debug = celliax_debug | DEBUG_PBX;
    if (!option_debug) {
      WARNINGA
        ("DEBUG_PBX activated, but option_debug is 0. You have to set debug level higher than zero to see some debugging output. Please use the command \"set debug 10\" or start Asterisk with \"-dddddddddd\" option for full DEBUG_PBX debugging output.\n",
         CELLIAX_TMP_LOG);
    } else {
      NOTICA("DEBUG_PBX activated. \n", CELLIAX_TMP_LOG);
    }
  }

  if (debug_call) {
    celliax_debug = celliax_debug | DEBUG_CALL;
    if (!option_debug) {
      WARNINGA
        ("DEBUG_CALL activated, but option_debug is 0. You have to set debug level higher than zero to see some debugging output. Please use the command \"set debug 10\" or start Asterisk with \"-dddddddddd\" option for full DEBUG_CALL debugging output.\n",
         CELLIAX_TMP_LOG);
    } else {
      NOTICA("DEBUG_CALL activated. \n", CELLIAX_TMP_LOG);
    }
  }

  if (debug_locks) {
    celliax_debug = celliax_debug | DEBUG_LOCKS;
    if (!option_debug) {
      WARNINGA
        ("DEBUG_LOCKS activated, but option_debug is 0. You have to set debug level higher than zero to see some debugging output. Please use the command \"set debug 10\" or start Asterisk with \"-dddddddddd\" option for full DEBUG_LOCKS debugging output.\n",
         CELLIAX_TMP_LOG);
    } else {
      NOTICA("DEBUG_LOCKS activated. \n", CELLIAX_TMP_LOG);
    }
  }

  if (debug_monitorlocks) {
    celliax_debug = celliax_debug | DEBUG_MONITORLOCKS;
    if (!option_debug) {
      WARNINGA
        ("DEBUG_MONITORLOCKS activated, but option_debug is 0. You have to set debug level higher than zero to see some debugging output. Please use the command \"set debug 10\" or start Asterisk with \"-dddddddddd\" option for full DEBUG_MONITORLOCKS debugging output.\n",
         CELLIAX_TMP_LOG);
    } else {
      NOTICA("DEBUG_MONITORLOCKS activated. \n", CELLIAX_TMP_LOG);
    }
  }
#ifdef CELLIAX_CVM
  if (debug_cvm) {
    celliax_debug = celliax_debug | DEBUG_CVM;
    if (!option_debug) {
      WARNINGA
        ("DEBUG_CVM activated, but option_debug is 0. You have to set debug level higher than zero to see some debugging output. Please use the command \"set debug 10\" or start Asterisk with \"-dddddddddd\" option for full DEBUG_CVM debugging output.\n",
         CELLIAX_TMP_LOG);
    } else {
      NOTICA("DEBUG_CVM activated. \n", CELLIAX_TMP_LOG);
    }
  }
#endif /* CELLIAX_CVM */

  if (option_debug > 1) {
    DEBUGA_SOUND("playback_boost is %f\n", CELLIAX_TMP_LOG, tmp->playback_boost);
    DEBUGA_SOUND("capture_boost is %f\n", CELLIAX_TMP_LOG, tmp->capture_boost);
  }

  /* serial protocols are named in config with a string, but as int in this software */
  if (strcasecmp(tmp->controldevprotocolname, "fbus2") == 0)
    tmp->controldevprotocol = PROTOCOL_FBUS2;
  else if (strcasecmp(tmp->controldevprotocolname, "at") == 0)
    tmp->controldevprotocol = PROTOCOL_AT;
  else if (strcasecmp(tmp->controldevprotocolname, "no_serial") == 0)
    tmp->controldevprotocol = PROTOCOL_NO_SERIAL;
  else if (strcasecmp(tmp->controldevprotocolname, "alsa_voicemodem") == 0)
    tmp->controldevprotocol = PROTOCOL_ALSA_VOICEMODEM;
#ifdef CELLIAX_CVM
  else if (strcasecmp(tmp->controldevprotocolname, "cvm_busmail") == 0)
    tmp->controldevprotocol = PROTOCOL_CVM_BUSMAIL;
#endif /* CELLIAX_CVM */
  else {
#ifndef CELLIAX_CVM
    ERRORA
      ("control_device_protocol in celliax.conf MUST be = fbus2|at|no_serial|alsa_voicemodem, but is = '%s'\n",
       CELLIAX_TMP_LOG,
       tmp->controldevprotocolname ? tmp->controldevprotocolname : "NULL");
#else
    ERRORA
      ("control_device_protocol in celliax.conf MUST be = fbus2|at|no_serial|alsa_voicemodem|cvm_busmail, but is = '%s'\n",
       CELLIAX_TMP_LOG,
       tmp->controldevprotocolname ? tmp->controldevprotocolname : "NULL");
#endif /* CELLIAX_CVM */

    /* we failed, free the PVT */
    free(tmp);
    return NULL;
  }

  if (tmp->controldevice_speed != celliax_default.controldevice_speed) {
    /* serial speeds are numbers in config file, but we needs definitions in this software */
    if (tmp->controldevice_speed == 9600)
      tmp->controldevice_speed = B9600;
    else if (tmp->controldevice_speed == 19200)
      tmp->controldevice_speed = B19200;
    else if (tmp->controldevice_speed == 38400)
      tmp->controldevice_speed = B38400;
    else if (tmp->controldevice_speed == 57600)
      tmp->controldevice_speed = B57600;
    else if (tmp->controldevice_speed == 115200)
      tmp->controldevice_speed = B115200;
    else {
      ERRORA
        ("controldevice_speed has to be given one of the following values: 9600|19200|38400|57600|115200. In the config file, was given: %d\n",
         CELLIAX_TMP_LOG, tmp->controldevice_speed);
      free(tmp);
      return NULL;
    }
  }

/* CVM PP DECT modules supports registration to two DECT FPs (bases), but CVM can be only connected to one DECT FP at the time, so we need two PINs (for registration) and info to which DECT FP connect*/
#ifdef CELLIAX_CVM
  if (tmp->cvm_subsc_no != celliax_default.cvm_subsc_no) {
    if ((tmp->cvm_subsc_no != 1) && (tmp->cvm_subsc_no != 2)) {
      ERRORA
        ("cvm_subscription_no has to be given one of the following values: 1|2. In the config file, was given: %d\n",
         CELLIAX_TMP_LOG, tmp->cvm_subsc_no);
      free(tmp);
      return NULL;
    }
  }

  if (tmp->cvm_subsc_1_pin != celliax_default.cvm_subsc_1_pin) {
    if (4 != strlen(tmp->cvm_subsc_1_pin)) {
      ERRORA
        ("cvm_subscription_1_pin has to be 4 digits long. In the config file, was given: %s\n",
         CELLIAX_TMP_LOG, tmp->cvm_subsc_1_pin);
      free(tmp);
      return NULL;
    }
  }

  if (tmp->cvm_subsc_2_pin != celliax_default.cvm_subsc_2_pin) {
    if (4 != strlen(tmp->cvm_subsc_2_pin)) {
      ERRORA
        ("cvm_subscription_2_pin has to be 4 digits long. In the config file, was given: %s\n",
         CELLIAX_TMP_LOG, tmp->cvm_subsc_2_pin);
      free(tmp);
      return NULL;
    }
  }

  if (tmp->cvm_volume_level != celliax_default.cvm_volume_level) {
    if ((0 > tmp->cvm_volume_level) && (9 < tmp->cvm_volume_level)) {
      ERRORA("cvm_volume_level has to be 0-9. In the config file, was given: %d\n",
             CELLIAX_TMP_LOG, tmp->cvm_volume_level);
      free(tmp);
      return NULL;
    }
  }

  if (tmp->cvm_celliax_serial_delay != celliax_default.cvm_celliax_serial_delay) {
    if ((0 > tmp->cvm_celliax_serial_delay) && (65535 < tmp->cvm_celliax_serial_delay)) {
      ERRORA
        ("cvm_celliax_serial_dealy has to be 0-65535. In the config file, was given: %d\n",
         CELLIAX_TMP_LOG, tmp->cvm_celliax_serial_delay);
      free(tmp);
      return NULL;
    }
  }
#endif /* CELLIAX_CVM */
  if (tmp->need_acoustic_ring) {
    /* alloc and initialize a new dsp struct for this interface pvt, WITH silence suppression */
    if (celliax_sound_dsp_set(tmp, tmp->dsp_silence_threshold, 1)) {
      ERRORA("celliax_sound_dsp_set failed\n", CELLIAX_TMP_LOG);
      celliax_sound_shutdown(tmp);
      if (tmp)
        free(tmp);
      return NULL;
    }

/* initialize the soundcard channels (input and output) used by this interface (a multichannel soundcard can be used by multiple interfaces), optionally starting the sound managing threads */
  res = celliax_sound_init(tmp);
  if (res == -1) {
    ERRORA("Failed initializing sound device\n", CELLIAX_TMP_LOG);
    /* we failed, free the PVT */
    if (tmp)
      free(tmp);
    return NULL;
  }

  }

  /* init the serial port */
  if (tmp->controldevprotocol != PROTOCOL_NO_SERIAL) {
    tmp->controldevfd = celliax_serial_init(tmp, tmp->controldevice_speed);
    if (tmp->controldevfd < 1) {
      ERRORA("celliax_serial_init failed\n", CELLIAX_TMP_LOG);
      celliax_sound_shutdown(tmp);
      if (tmp)
        free(tmp);
      return NULL;
    }
  }

  /* config the phone/modem on the serial port */
  if (tmp->controldevprotocol != PROTOCOL_NO_SERIAL) {
    //int res;
    res = celliax_serial_config(tmp);
    if (res) {
      ERRORA("celliax_serial_config failed\n", CELLIAX_TMP_LOG);
      celliax_sound_shutdown(tmp);
      if (tmp)
        free(tmp);
      return NULL;
    }
  }

  /* return the newly created celliax_pvt */
  return tmp;
}

/*! \brief (Re)Start the module main monitor thread, watching for incoming calls on the interfaces */
int celliax_restart_monitor(void)
{
  static struct celliax_pvt *p = &celliax_log_struct;
  /* If we're supposed to be stopped -- stay stopped */
  if (celliax_monitor_thread == AST_PTHREADT_STOP)
    return 0;
  LOKKA(&celliax_monlock);
  /* Do not seems possible to me that this function can be called by the very same monitor thread, but let's be paranoid */
  if (celliax_monitor_thread == pthread_self()) {
    UNLOCKA(&celliax_monlock);
    ERRORA("Cannot kill myself\n", CELLIAX_P_LOG);
    return -1;
  }
  /* if the monitor thread exists */
  if (celliax_monitor_thread != AST_PTHREADT_NULL) {
    /* Wake up the thread, it can be stuck waiting in a select or so */
    pthread_kill(celliax_monitor_thread, SIGURG);
    pthread_kill(celliax_monitor_audio_thread, SIGURG);
  } else {
    /* the monitor thread does not exists, start a new monitor */
    if (ast_pthread_create(&celliax_monitor_thread, NULL, celliax_do_monitor, NULL) < 0) {
      UNLOCKA(&celliax_monlock);
      ERRORA("Unable to start monitor thread.\n", CELLIAX_P_LOG);
      return -1;
    }

    if (ast_pthread_create
        (&celliax_monitor_audio_thread, NULL, celliax_do_audio_monitor, NULL) < 0) {
      ERRORA("Unable to start audio_monitor thread.\n", CELLIAX_P_LOG);
      return -1;
    }

  }
  UNLOCKA(&celliax_monlock);
  return 0;
}

/*! \brief The celliax monitoring thread 
 * \note   This thread monitors all the celliax interfaces that are not in a call
 *         (and thus do not have a separate thread) indefinitely 
 *         */
void *celliax_do_monitor(void *data)
{
  fd_set rfds;
  int res;
  struct celliax_pvt *p = NULL;
  int max = -1;
  struct timeval to;
  time_t now_timestamp;

  if (pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL)) {
    ERRORA("Unable to set cancel type to deferred\n", CELLIAX_P_LOG);
    return NULL;
  }

  for (;;) {
    pthread_testcancel();
    /* Don't let anybody kill us right away.  Nobody should lock the interface list
       and wait for the monitor list, but the other way around is okay. */
    PUSHA_UNLOCKA(&celliax_monlock);
    MONITORLOKKA(&celliax_monlock);
    /* Lock the interface list */
    PUSHA_UNLOCKA(&celliax_iflock);
    MONITORLOKKA(&celliax_iflock);
    /* Build the stuff we're going to select on, that is the celliax_serial_fd of every
       celliax_pvt that does not have an associated owner channel. In the case of FBUS2 3310
       and in the case of PROTOCOL_NO_SERIAL we add the audio_fd as well, because there is not serial signaling of incoming calls */
    FD_ZERO(&rfds);

    time(&now_timestamp);
    p = celliax_iflist;
    while (p) {
      if (!p->owner) {
        /* This interface needs to be watched, as it lacks an owner */

        if (p->controldevprotocol != PROTOCOL_NO_SERIAL && !p->controldev_dead) {
          /* This interface needs its serial connection to be watched, nokia 3310 and compatibles needs sounds as well */
          if (FD_ISSET(p->controldevfd, &rfds)) {
            ERRORA("Bizarre! Descriptor %d (controldevfd) appears twice ?\n",
                   CELLIAX_P_LOG, p->controldevfd);
          }
          if (p->controldevfd > 0) {

            //time(&now_timestamp);
            if ((now_timestamp - p->celliax_serial_synced_timestamp) > p->celliax_serial_sync_period) { //TODO find a sensible period. 5min? in config?
              int rt;
              if (option_debug > 1)
                DEBUGA_SERIAL("Syncing Serial\n", CELLIAX_P_LOG);
              rt = celliax_serial_sync(p);
              if (rt) {
                p->controldev_dead = 1;
                close(p->controldevfd);
                ERRORA("serial sync failed, declaring %s dead\n", CELLIAX_P_LOG,
                       p->controldevice_name);
              }
              rt = celliax_serial_getstatus(p);
              if (rt) {
                p->controldev_dead = 1;
                close(p->controldevfd);
                ERRORA("serial getstatus failed, declaring %s dead\n", CELLIAX_P_LOG,
                       p->controldevice_name);
              }

            }

            if (!p->controldev_dead) {
              /* add this file descriptor to the set watched by the select */
              FD_SET(p->controldevfd, &rfds);
              if (p->controldevfd > max) {
                /* adjust the maximum file descriptor value the select has to watch for */
                max = p->controldevfd;
              }
            }
          }
        }

      }
      /* next interface, please */
      p = p->next;
    }
    /* Okay, now that we know what to do, release the interface lock */
    MONITORUNLOCKA(&celliax_iflock);
    POPPA_UNLOCKA(&celliax_iflock);
    /* And from now on, we're okay to be killed, so release the monitor lock as well */
    MONITORUNLOCKA(&celliax_monlock);
    POPPA_UNLOCKA(&celliax_monlock);

    /* you want me to die? */
    pthread_testcancel();

    /* Wait for something to happen */
    to.tv_sec = 0;
    to.tv_usec = 500000;        /* we select with this timeout because under cygwin we avoid the signal usage, so there is no way to end the thread if it is stuck waiting for select */
    res = ast_select(max + 1, &rfds, NULL, NULL, &to);

    /* you want me to die? */
    pthread_testcancel();

    /* Okay, select has finished.  Let's see what happened.  */

    /* If there are errors...  */
    if (res < 0) {
      if (errno == EINTR)       /* EINTR is just the select 
                                   being interrupted by a SIGURG, or so */
        continue;
      else {
        ERRORA("select returned %d: %s\n", CELLIAX_P_LOG, res, strerror(errno));
//FIXME what to do here? is the interface that failed signaled? which interface we have to disable?
        return NULL;
      }
    }

    /* must not be killed while celliax_iflist is locked */
    PUSHA_UNLOCKA(&celliax_monlock);
    MONITORLOKKA(&celliax_monlock);
    /* Alright, lock the interface list again, and let's look and see what has
       happened */
    PUSHA_UNLOCKA(&celliax_iflock);
    MONITORLOKKA(&celliax_iflock);

    p = celliax_iflist;
    for (; p; p = p->next) {

      if (p->controldevprotocol != PROTOCOL_NO_SERIAL && !p->controldev_dead) {
        if (!p->owner) {        //give all the serial channels that have no owner a read, so we can have the timers clicking

          if (!p->celliax_serial_monitoring) {
            p->celliax_serial_monitoring = 1;
            res = celliax_serial_monitor(p);
            if (res == -1) {    //manage the graceful interface shutdown
              p->controldev_dead = 1;
              close(p->controldevfd);
              ERRORA("celliax_serial_monitor failed, declaring %s dead\n", CELLIAX_P_LOG,
                     p->controldevice_name);
            } else if (!p->need_acoustic_ring
                       && p->controldevprotocol != PROTOCOL_NO_SERIAL
                       && p->interface_state == AST_STATE_RING) {
              if (option_debug)
                DEBUGA_PBX("INCOMING RING\n", CELLIAX_P_LOG);
              if (!celliax_new(p, AST_STATE_RING, p->context)) {
                //FIXME what to do here?
                ERRORA("celliax_new failed! BAD BAD BAD\n", CELLIAX_P_LOG);
              }
            }
            p->celliax_serial_monitoring = 0;
          }
        }
      }

      if (p->controldevprotocol != PROTOCOL_NO_SERIAL && p->controldev_dead) {

        /* init the serial port */
        p->controldevfd = celliax_serial_init(p, p->controldevice_speed);
        if (p->controldevfd < 1) {
          DEBUGA_SERIAL("celliax_serial_init failed\n", CELLIAX_P_LOG);
        } else {

          /* config the phone/modem on the serial port */
          res = celliax_serial_config(p);
          if (res) {
            DEBUGA_SERIAL("celliax_serial_config failed\n", CELLIAX_P_LOG);
            close(p->controldevfd);
          } else {

            NOTICA("Wow, the serial port has come back! Let's see if it will work\n",
                   CELLIAX_P_LOG);
            p->controldev_dead = 0;
          }

        }

      }

    }
    MONITORUNLOCKA(&celliax_iflock);
    POPPA_UNLOCKA(&celliax_iflock);
    MONITORUNLOCKA(&celliax_monlock);
    POPPA_UNLOCKA(&celliax_monlock);
    pthread_testcancel();
  }
/* Never reached */
  return NULL;

}

void *celliax_do_audio_monitor(void *data)
{
  fd_set rfds;
  int res;
  struct celliax_pvt *p = NULL;
  int max = -1;
  struct timeval to;

  if (pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL)) {
    ERRORA("Unable to set cancel type to deferred\n", CELLIAX_P_LOG);
    return NULL;
  }

  for (;;) {
    pthread_testcancel();
    /* Lock the interface list */
    PUSHA_UNLOCKA(&celliax_iflock);
    MONITORLOKKA(&celliax_iflock);

    FD_ZERO(&rfds);

    p = celliax_iflist;
    while (p) {
      if (!p->owner) {
        /* This interface needs to be watched, as it lacks an owner */

        if (p->controldevprotocol == PROTOCOL_NO_SERIAL || p->need_acoustic_ring) {
          /* This interface needs its incoming sound to be watched, because it cannot signal incoming ring from serial (eg nokia 3310 and compatibles) */
          if (p->celliax_sound_capt_fd > 0) {
            /* if fd exist */
            if (FD_ISSET(p->celliax_sound_capt_fd, &rfds)) {
              ERRORA("Bizarre! Descriptor %d (celliax_sound_capt_fd) appears twice ?\n",
                     CELLIAX_P_LOG, p->celliax_sound_capt_fd);
            }
            /* add this file descriptor to the set watched by the select */
            FD_SET(p->celliax_sound_capt_fd, &rfds);
            if (p->celliax_sound_capt_fd > max) {
              /* adjust the maximum file descriptor value the select has to watch for */
              max = p->celliax_sound_capt_fd;
            }
          }
        }

      }
      /* next interface, please */
      p = p->next;
    }
    /* Okay, now that we know what to do, release the interface lock */

    MONITORUNLOCKA(&celliax_iflock);
    POPPA_UNLOCKA(&celliax_iflock);
    /* you want me to die? */
    pthread_testcancel();

    /* Wait for something to happen */
    to.tv_sec = 0;
    to.tv_usec = 500000;        /* we select with this timeout because under cygwin we avoid the signal usage, so there is no way to end the thread if it is stuck waiting for select */
    res = ast_select(max + 1, &rfds, NULL, NULL, &to);

    /* you want me to die? */
    pthread_testcancel();

    /* Okay, select has finished.  Let's see what happened.  */

    /* If there are errors...  */
    if (res < 0) {
      if (errno == EINTR) {     /* EINTR is just the select 
                                   being interrupted by a SIGURG, or so */
        usleep(100);
        continue;
      } else {
        ERRORA("select returned %d: %s\n", CELLIAX_P_LOG, res, strerror(errno));
//FIXME what to do here? is the interface that failed signaled? which interface we have to disable?
        return NULL;
      }
    }
    /* If there are no file descriptors changed, just continue  */

    if (res == 0) {
      usleep(100);              //let's breath
      continue;
    }
//usleep(10); //let's breath

    /* Lock the interface list */
    PUSHA_UNLOCKA(&celliax_iflock);
    MONITORLOKKA(&celliax_iflock);

    p = celliax_iflist;
    for (; p; p = p->next) {

      if (FD_ISSET(p->celliax_sound_capt_fd, &rfds)) {
        res = celliax_sound_monitor(p);
        if (res < 0) {
          ERRORA("celliax_sound_monitor ERROR %d\n", CELLIAX_P_LOG, res);
        } else if (res == CALLFLOW_INCOMING_RING) {
          p->phone_callflow = CALLFLOW_INCOMING_RING;
          p->interface_state = AST_STATE_RING;
          if (option_debug)
            DEBUGA_PBX("INCOMING RING\n", CELLIAX_P_LOG);
          if (!celliax_new(p, AST_STATE_RING, p->context))
            ERRORA("celliax_new failed! BAD BAD BAD\n", CELLIAX_P_LOG);
        } else {
        }

      }

    }
/* Okay, now that we know what to do, release the interface lock */

    MONITORUNLOCKA(&celliax_iflock);
    POPPA_UNLOCKA(&celliax_iflock);

    pthread_testcancel();
  }
/* Never reached */
  return NULL;
}

/*!
 * \brief Initialize the soundcard channels (input and output) used by one interface (a multichannel soundcard can be used by multiple interfaces) 
 * \param p the celliax_pvt of the interface
 *
 * This function initialize the soundcard channels (input and output) used by one interface (a multichannel soundcard can be used by multiple interfaces). It simply pass its parameters to the right function for the sound system for which has been compiled, eg. alsa_init for ALSA, oss_init for OSS, winmm_init for Windows Multimedia, etc and return the result 
 *
 * \return zero on success, -1 on error.
 */

int celliax_sound_init(struct celliax_pvt *p)
{
#ifdef CELLIAX_ALSA
  return alsa_init(p);
#endif /* CELLIAX_ALSA */
#ifdef CELLIAX_PORTAUDIO
  return celliax_portaudio_init(p);
#endif /* CELLIAX_PORTAUDIO */

  return -1;
}

/*!
 * \brief Shutdown the soundcard channels (input and output) used by one interface (a multichannel soundcard can be used by multiple interfaces) 
 * \param p the celliax_pvt of the interface
 *
 * This function shutdown the soundcard channels (input and output) used by one interface (a multichannel soundcard can be used by multiple interfaces). It simply pass its parameters to the right function for the sound system for which has been compiled, eg. alsa_shutdown for ALSA, oss_shutdown for OSS, winmm_shutdown for Windows Multimedia, etc and return the result
 *
 * \return zero on success, -1 on error.
 */

int celliax_sound_shutdown(struct celliax_pvt *p)
{

#ifdef CELLIAX_ALSA
  return alsa_shutdown(p);
#endif /* CELLIAX_ALSA */
#ifdef CELLIAX_PORTAUDIO
  return celliax_portaudio_shutdown(p);
#endif /* CELLIAX_PORTAUDIO */

  return -1;
}

/*! \brief returns an asterisk frame categorized by dsp algorithms */
struct ast_frame *celliax_sound_dsp_analize(struct celliax_pvt *p, struct ast_frame *f,
                                            int dsp_silence_threshold)
{
  if (!p->dsp) {
    DEBUGA_SOUND("no dsp, initializing it \n", CELLIAX_P_LOG);
    if (celliax_sound_dsp_set(p, dsp_silence_threshold, 1)) {
      ERRORA("celliax_sound_dsp_set failed\n", CELLIAX_P_LOG);
      return NULL;
    }
  }

  /* process with dsp */
  if (p->dsp) {
    if (f->frametype == AST_FRAME_VOICE) {
      f = ast_dsp_process(p->owner, p->dsp, f);
    } else {
      //WARNINGA("not a VOICE frame ! \n", CELLIAX_P_LOG);
    }
  }
  return f;
}

/*! \brief initialize the dsp algorithms and structures */
int celliax_sound_dsp_set(struct celliax_pvt *p, int dsp_silence_threshold,
                          int silence_suppression)
{

/*  let asterisk dsp algorithms detect dtmf */
  if (p->dsp) {
    return 0;
  }
  if (option_debug > 1)
    DEBUGA_SOUND("alloc dsp \n", CELLIAX_P_LOG);
  p->dsp = ast_dsp_new();
  if (p->dsp) {
    if (silence_suppression) {
      ast_dsp_set_threshold(p->dsp, dsp_silence_threshold);
      DEBUGA_SOUND("set dsp_silence_threshold=%d\n", CELLIAX_P_LOG,
                   dsp_silence_threshold);
      if (option_debug > 1)
        DEBUGA_SOUND("Detecting silence, I mean, voice\n", CELLIAX_P_LOG);
      ast_dsp_set_features(p->dsp, 0 | DSP_FEATURE_SILENCE_SUPPRESS);
    } else {
      if (option_debug > 1)
        DEBUGA_SOUND("WITHOUT SILENCE_SUPPRESS, Detecting inband dtmf with sw DSP\n",
                     CELLIAX_P_LOG);

#ifdef ASTERISK_VERSION_1_6_0_1
      ast_dsp_set_features(p->dsp, 0 | DSP_FEATURE_DIGIT_DETECT);
#else
      ast_dsp_set_features(p->dsp, 0 | DSP_FEATURE_DTMF_DETECT);
#endif /* ASTERISK_VERSION_1_6_0_1 */
    }

    /*
       if (ast_dsp_digitmode(p->dsp, DSP_DIGITMODE_DTMF | DSP_DIGITMODE_RELAXDTMF)) {
       ERRORA("ast_dsp_digitmode failed\n", CELLIAX_P_LOG);
       return -1;
       }
     */
  } else {
    ERRORA("ast_dsp_new failed\n", CELLIAX_P_LOG);
    return -1;
  }
  return 0;
}

/*! \brief Read audio frames from interface */
struct ast_frame *celliax_sound_read(struct celliax_pvt *p)
{
  struct ast_frame *f = NULL;
#ifdef CELLIAX_ALSA
  f = alsa_read(p);
#endif /* CELLIAX_ALSA */
#ifdef CELLIAX_PORTAUDIO
  f = celliax_portaudio_read(p);
#endif /* CELLIAX_PORTAUDIO */

  return f;
}

/*! \brief Send audio frame to interface */
int celliax_sound_write(struct celliax_pvt *p, struct ast_frame *f)
{
  int ret = -1;

#ifdef CELLIAX_ALSA
  ret = alsa_write(p, f);
#endif /* CELLIAX_ALSA */
#ifdef CELLIAX_PORTAUDIO
  ret = celliax_portaudio_write(p, f);
#endif /* CELLIAX_PORTAUDIO */

  return ret;
}

/*! \brief read an audio frame and tell if is "voice" (interpreted as incoming RING) */
int celliax_sound_monitor(struct celliax_pvt *p)
{
  struct ast_frame *f;
  f = celliax_sound_read(p);
  if (f) {
    f = celliax_sound_dsp_analize(p, f, p->dsp_silence_threshold);
    if (f) {
      if (f->frametype == AST_FRAME_VOICE) {
        DEBUGA_SOUND("VOICE\n", CELLIAX_P_LOG);
        return CALLFLOW_INCOMING_RING;
      } else {
        return AST_STATE_DOWN;
      }
    }
  }
  return -1;
}

/*!
 * \brief This thread runs during a call, and monitor the interface serial port for signaling, like hangup, caller id, etc
 *
 */
void *celliax_do_controldev_thread(void *data)
{
  struct celliax_pvt *p = data;
  int res;

  DEBUGA_SERIAL("In celliax_do_controldev_thread: started, p=%p\n", CELLIAX_P_LOG, p);

  if (pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL)) {
    ERRORA("Unable to set cancel type to deferred\n", CELLIAX_P_LOG);
    return NULL;
  }

  while (1) {
    int rt;
    time_t now_timestamp;

    if (p->controldevprotocol == PROTOCOL_NO_SERIAL) {
      while (1) {
        usleep(10000);
        pthread_testcancel();
      }
    }
#ifdef CELLIAX_CVM
    if (p->controldevprotocol == PROTOCOL_CVM_BUSMAIL) {
      usleep(p->cvm_celliax_serial_delay * 1000);   //to get msecs
    } else {
      usleep(1000);
    }

#else
    usleep(1000);
#endif /* CELLIAX_CVM */

    pthread_testcancel();
    /* do not read from a dead controldev */
    if (p->controldev_dead) {
      DEBUGA_SERIAL("celliax_do_controldev_thread: device %s is dead\n", CELLIAX_P_LOG,
                    p->controldevice_name);
      if (p->owner)
        celliax_queue_control(p->owner, AST_CONTROL_HANGUP);
      return NULL;
    } else {
      pthread_testcancel();
      res = celliax_serial_read(p);
      pthread_testcancel();
      if (res == -1) {
        p->controldev_dead = 1;
        close(p->controldevfd);
        ERRORA("serial read failed, declaring %s dead\n", CELLIAX_P_LOG,
               p->controldevice_name);
      }
    }

    pthread_testcancel();
    time(&now_timestamp);
    if ((now_timestamp - p->celliax_serial_synced_timestamp) > p->celliax_serial_synced_timestamp && !p->controldev_dead) { //TODO find a sensible period. 5min? in config?
      DEBUGA_SERIAL("Syncing Serial\n", CELLIAX_P_LOG);
      pthread_testcancel();
      rt = celliax_serial_sync(p);
      pthread_testcancel();
      if (rt) {
        p->controldev_dead = 1;
        close(p->controldevfd);
        ERRORA("serial sync failed, declaring %s dead\n", CELLIAX_P_LOG,
               p->controldevice_name);
      }
      pthread_testcancel();
      rt = celliax_serial_getstatus(p);
      pthread_testcancel();
      if (rt) {
        p->controldev_dead = 1;
        close(p->controldevfd);
        ERRORA("serial getstatus failed, declaring %s dead\n", CELLIAX_P_LOG,
               p->controldevice_name);
      }

    }
    pthread_testcancel();
  }
  return NULL;

}

int celliax_serial_init(struct celliax_pvt *p, speed_t controldevice_speed)
{
  int fd;
  int rt;
  struct termios tp;

/* if there is a file descriptor, close it. But it is probably just an old value, so don't check for close success*/
  fd = p->controldevfd;
  if (fd) {
    close(fd);
  }
/*  open the serial port */
#ifdef __CYGWIN__
  fd = open(p->controldevice_name, O_RDWR | O_NOCTTY | O_NONBLOCK);
  sleep(1);
  close(fd);
#endif /* __CYGWIN__ */
  fd = open(p->controldevice_name, O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (fd == -1) {
    DEBUGA_SERIAL("serial error: %s\n", CELLIAX_P_LOG, strerror(errno));
    p->controldevfd = fd;
    return -1;
  }
/*  flush it */
  rt = tcflush(fd, TCIFLUSH);
  if (rt == -1) {
    ERRORA("serial error: %s", CELLIAX_P_LOG, strerror(errno));
  }
/*  attributes */
  tp.c_cflag = B0 | CS8 | CLOCAL | CREAD | HUPCL;
  tp.c_iflag = IGNPAR;
  tp.c_cflag &= ~CRTSCTS;
  tp.c_oflag = 0;
  tp.c_lflag = 0;
  tp.c_cc[VMIN] = 1;
  tp.c_cc[VTIME] = 0;
/*  set controldevice_speed */
  rt = cfsetispeed(&tp, p->controldevice_speed);
  if (rt == -1) {
    ERRORA("serial error: %s", CELLIAX_P_LOG, strerror(errno));
  }
  rt = cfsetospeed(&tp, p->controldevice_speed);
  if (rt == -1) {
    ERRORA("serial error: %s", CELLIAX_P_LOG, strerror(errno));
  }
/*  set port attributes */
  if (tcsetattr(fd, TCSADRAIN, &tp) == -1) {
    ERRORA("serial error: %s", CELLIAX_P_LOG, strerror(errno));
  }
  rt = tcsetattr(fd, TCSANOW, &tp);
  if (rt == -1) {
    ERRORA("serial error: %s", CELLIAX_P_LOG, strerror(errno));
  }
  unsigned int status = 0;
#ifndef __CYGWIN__
  ioctl(fd, TIOCMGET, &status);
  status |= TIOCM_DTR;          /*  Set DTR high */
  status &= ~TIOCM_RTS;         /*  Set RTS low */
  ioctl(fd, TIOCMSET, &status);
  ioctl(fd, TIOCMGET, &status);
  unsigned int flags = TIOCM_DTR;
  ioctl(fd, TIOCMBIS, &flags);
  flags = TIOCM_RTS;
  ioctl(fd, TIOCMBIC, &flags);
  ioctl(fd, TIOCMGET, &status);
#else /* __CYGWIN__ */
  ioctl(fd, TIOCMGET, &status);
  status |= TIOCM_DTR;          /*  Set DTR high */
  status &= ~TIOCM_RTS;         /*  Set RTS low */
  ioctl(fd, TIOCMSET, &status);
#endif /* __CYGWIN__ */
  p->controldevfd = fd;
  DEBUGA_SERIAL("Syncing Serial\n", CELLIAX_P_LOG);
  rt = celliax_serial_sync(p);
  if (rt == -1) {
    ERRORA("Serial init error\n", CELLIAX_P_LOG);
    return -1;
  }
  return (fd);
}

int celliax_serial_sync(struct celliax_pvt *p)
{
  if (p->controldevprotocol == PROTOCOL_AT)
    return celliax_serial_sync_AT(p);
#ifdef CELLIAX_FBUS2
  if (p->controldevprotocol == PROTOCOL_FBUS2)
    return celliax_serial_sync_FBUS2(p);
#endif /* CELLIAX_FBUS2 */
#ifdef CELLIAX_CVM
  if (p->controldevprotocol == PROTOCOL_CVM_BUSMAIL)
    return celliax_serial_sync_CVM_BUSMAIL(p);
#endif /* CELLIAX_CVM */

  return -1;
}

int celliax_serial_getstatus(struct celliax_pvt *p)
{
  if (p->controldevprotocol == PROTOCOL_AT)
    return celliax_serial_getstatus_AT(p);
#ifdef CELLIAX_FBUS2
  if (p->controldevprotocol == PROTOCOL_FBUS2)
    return celliax_serial_getstatus_FBUS2(p);
#endif /* CELLIAX_FBUS2 */

#ifdef CELLIAX_CVM
  if (p->controldevprotocol == PROTOCOL_CVM_BUSMAIL)
    return celliax_serial_getstatus_CVM_BUSMAIL(p);
#endif /* CELLIAX_CVM */
  return -1;
}

int celliax_serial_read(struct celliax_pvt *p)
{
  if (p->controldevprotocol == PROTOCOL_AT)
    return celliax_serial_read_AT(p, 0, 100000, 0, NULL, 1);    // a 10th of a second timeout
#ifdef CELLIAX_FBUS2
  if (p->controldevprotocol == PROTOCOL_FBUS2)
    return celliax_serial_read_FBUS2(p);
#endif /* CELLIAX_FBUS2 */
#ifdef CELLIAX_CVM
  if (p->controldevprotocol == PROTOCOL_CVM_BUSMAIL)
    return celliax_serial_read_CVM_BUSMAIL(p);
#endif /* CELLIAX_CVM */
  return -1;
}

int celliax_serial_hangup(struct celliax_pvt *p)
{
  if (p->controldevprotocol == PROTOCOL_AT)
    return celliax_serial_hangup_AT(p);
#ifdef CELLIAX_FBUS2
  if (p->controldevprotocol == PROTOCOL_FBUS2)
    return celliax_serial_hangup_FBUS2(p);
#endif /* CELLIAX_FBUS2 */
#ifdef CELLIAX_CVM
  if (p->controldevprotocol == PROTOCOL_CVM_BUSMAIL)
    return celliax_serial_hangup_CVM_BUSMAIL(p);
#endif /* CELLIAX_CVM */
  return -1;
}

int celliax_serial_answer(struct celliax_pvt *p)
{
  if (p->controldevprotocol == PROTOCOL_AT)
    return celliax_serial_answer_AT(p);
#ifdef CELLIAX_FBUS2
  if (p->controldevprotocol == PROTOCOL_FBUS2)
    return celliax_serial_answer_FBUS2(p);
#endif /* CELLIAX_FBUS2 */
#ifdef CELLIAX_CVM
  if (p->controldevprotocol == PROTOCOL_CVM_BUSMAIL)
    return celliax_serial_answer_CVM_BUSMAIL(p);
#endif /* CELLIAX_CVM */
  return -1;
}

int celliax_serial_config(struct celliax_pvt *p)
{
  if (p->controldevprotocol == PROTOCOL_AT)
    return celliax_serial_config_AT(p);
#ifdef CELLIAX_FBUS2
  if (p->controldevprotocol == PROTOCOL_FBUS2)
    return celliax_serial_config_FBUS2(p);
#endif /* CELLIAX_FBUS2 */
#ifdef CELLIAX_CVM
  if (p->controldevprotocol == PROTOCOL_CVM_BUSMAIL)
    return celliax_serial_config_CVM_BUSMAIL(p);
#endif /* CELLIAX_CVM */
  return -1;
}

int celliax_serial_monitor(struct celliax_pvt *p)
{
  if (p->controldevprotocol == PROTOCOL_AT)
    return celliax_serial_read_AT(p, 0, 100000, 0, NULL, 1);    // a 10th of a second timeout
#ifdef CELLIAX_FBUS2
  if (p->controldevprotocol == PROTOCOL_FBUS2)
    return celliax_serial_read_FBUS2(p);
#endif /* CELLIAX_FBUS2 */
#ifdef CELLIAX_CVM
  if (p->controldevprotocol == PROTOCOL_CVM_BUSMAIL)
    return celliax_serial_read_CVM_BUSMAIL(p);
#endif /* CELLIAX_CVM */
  return -1;
}

/************************************************/

/* LUIGI RIZZO's magic */
/*
 * store the boost factor
 */
#ifdef ASTERISK_VERSION_1_6_0
void celliax_store_boost(const char *s, double *boost)
#else
void celliax_store_boost(char *s, double *boost)
#endif                          /* ASTERISK_VERSION_1_6_0 */
{
  struct celliax_pvt *p = NULL;

  if (sscanf(s, "%lf", boost) != 1) {
    ERRORA("invalid boost <%s>\n", CELLIAX_P_LOG, s);
    return;
  }
  if (*boost < -BOOST_MAX) {
    WARNINGA("boost %s too small, using %d\n", CELLIAX_P_LOG, s, -BOOST_MAX);
    *boost = -BOOST_MAX;
  } else if (*boost > BOOST_MAX) {
    WARNINGA("boost %s too large, using %d\n", CELLIAX_P_LOG, s, BOOST_MAX);
    *boost = BOOST_MAX;
  }
  *boost = exp(log(10) * *boost / 20) * BOOST_SCALE;
  if (option_debug > 1)
    DEBUGA_SOUND("setting boost %s to %f\n", CELLIAX_P_LOG, s, *boost);
}

int celliax_serial_call(struct celliax_pvt *p, char *dstr)
{
  if (p->controldevprotocol == PROTOCOL_AT)
    return celliax_serial_call_AT(p, dstr);
#ifdef CELLIAX_FBUS2
  if (p->controldevprotocol == PROTOCOL_FBUS2)
    return celliax_serial_call_FBUS2(p, dstr);
#endif /* CELLIAX_FBUS2 */
  if (p->controldevprotocol == PROTOCOL_NO_SERIAL)
    return 0;
#ifdef CELLIAX_CVM
  if (p->controldevprotocol == PROTOCOL_CVM_BUSMAIL)
    return celliax_serial_call_CVM_BUSMAIL(p, dstr);
#endif /* CELLIAX_CVM */
  return -1;
}

/*
 * returns a pointer to the descriptor with the given name
 */
struct celliax_pvt *celliax_console_find_desc(char *dev)
{
  struct celliax_pvt *p;

  for (p = celliax_iflist; p && strcmp(p->name, dev) != 0; p = p->next);
  if (p == NULL)
    WARNINGA("could not find <%s>\n", CELLIAX_P_LOG, dev);

  return p;
}

int celliax_console_playback_boost(int fd, int argc, char *argv[])
{
  struct celliax_pvt *p = celliax_console_find_desc(celliax_console_active);

  if (argc > 2)
    return RESULT_SHOWUSAGE;
  if (!p) {
    ast_cli(fd,
            "No \"current\" celliax_console for playback_boost, please enter 'help celliax_console'\n");
    return RESULT_SUCCESS;
  }

  if (argc == 1) {
    ast_cli(fd, "playback_boost on the active celliax_console, that is [%s], is: %5.1f\n",
            celliax_console_active,
            20 * log10(((double) p->playback_boost / (double) BOOST_SCALE)));
  } else if (argc == 2) {
    celliax_store_boost(argv[1], &p->playback_boost);

    ast_cli(fd,
            "playback_boost on the active celliax_console, that is [%s], is now: %5.1f\n",
            celliax_console_active,
            20 * log10(((double) p->playback_boost / (double) BOOST_SCALE)));
  }

  return RESULT_SUCCESS;
}

int celliax_console_capture_boost(int fd, int argc, char *argv[])
{
  struct celliax_pvt *p = celliax_console_find_desc(celliax_console_active);

  if (argc > 2)
    return RESULT_SHOWUSAGE;
  if (!p) {
    ast_cli(fd,
            "No \"current\" celliax_console for capture_boost, please enter 'help celliax_console'\n");
    return RESULT_SUCCESS;
  }

  if (argc == 1) {
    ast_cli(fd, "capture_boost on the active celliax_console, that is [%s], is: %5.1f\n",
            celliax_console_active,
            20 * log10(((double) p->capture_boost / (double) BOOST_SCALE)));
  } else if (argc == 2) {
    celliax_store_boost(argv[1], &p->capture_boost);

    ast_cli(fd,
            "capture_boost on the active celliax_console, that is [%s], is now: %5.1f\n",
            celliax_console_active,
            20 * log10(((double) p->capture_boost / (double) BOOST_SCALE)));
  }

  return RESULT_SUCCESS;
}

int celliax_console_echo(int fd, int argc, char *argv[])
{
  struct celliax_pvt *p = celliax_console_find_desc(celliax_console_active);

  if (argc != 3 && argc != 1)
    return RESULT_SHOWUSAGE;
  if (!p) {
    ast_cli(fd,
            "No \"current\" celliax_console for celliax_echo, please enter 'help celliax_console'\n");
    return RESULT_SUCCESS;
  }

  if (argc == 1) {
    ast_cli(fd,
            "On the active celliax_console, that is [%s], speexecho and speexpreprocess are: %d and %d\n",
            celliax_console_active, p->speexecho, p->speexpreprocess);
  } else if (argc == 3) {
    sscanf(argv[1], "%d", &p->speexecho);
    sscanf(argv[2], "%d", &p->speexpreprocess);
    ast_cli(fd,
            "On the active celliax_console, that is [%s], speexecho and speexpreprocess are NOW: %d and %d\n",
            celliax_console_active, p->speexecho, p->speexpreprocess);
  }

  return RESULT_SUCCESS;
}

#ifndef ASTERISK_VERSION_1_6_0
int celliax_console_hangup(int fd, int argc, char *argv[])
{
  struct celliax_pvt *p = celliax_console_find_desc(celliax_console_active);

  if (argc != 1)
    return RESULT_SHOWUSAGE;
  if (!p) {
    ast_cli(fd,
            "No \"current\" celliax_console for hanging up, please enter 'help celliax_console'\n");
    return RESULT_SUCCESS;
  }
  if (!p->owner) {
    ast_cli(fd, "No call to hangup on the active celliax_console, that is [%s]\n",
            celliax_console_active);
    return RESULT_FAILURE;
  }
  if (p->owner)
    ast_queue_hangup(p->owner);
  return RESULT_SUCCESS;
}
#else /* ASTERISK_VERSION_1_6_0 */
char *celliax_console_hangup(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
  struct celliax_pvt *p = celliax_console_find_desc(celliax_console_active);


	switch (cmd) {
	case CLI_INIT:
		e->command = "celliax hangup";
		e->usage =
			"Usage: celliax hangup\n"
			"       Hangup a call on the celliax channel.\n";

		return NULL;
	case CLI_GENERATE:
		return NULL; 
	}

  if (a->argc != 2)
    return CLI_SHOWUSAGE;
  if (!p) {
    ast_cli(a->fd,
            "No \"current\" celliax_console for hanging up, please enter 'help celliax_console'\n");
    return CLI_SUCCESS;
  }
  if (!p->owner) {
    ast_cli(a->fd, "No call to hangup on the active celliax_console, that is [%s]\n",
            celliax_console_active);
    return CLI_FAILURE;
  }
  if (p->owner)
    ast_queue_hangup(p->owner);
  return CLI_SUCCESS;
}

#endif /* ASTERISK_VERSION_1_6_0 */
int celliax_console_sendsms(int fd, int argc, char *argv[])
{
  char *s = NULL;
  char *s1 = NULL;
  char command[512];

  if (argc != 3)
    return RESULT_SHOWUSAGE;

  s = argv[1];
  s1 = argv[2];

  memset(command, 0, sizeof(command));

  sprintf(command, "%s|%s|", s, s1);

  celliax_sendsms(NULL, (void *) &command);

  return RESULT_SUCCESS;
}

int celliax_console_dial(int fd, int argc, char *argv[])
{
  char *s = NULL;
  struct celliax_pvt *p = celliax_console_find_desc(celliax_console_active);

  if (argc != 2)
    return RESULT_SHOWUSAGE;
  if (!p) {
    ast_cli(fd,
            "No \"current\" celliax_console for dialing, please enter 'help celliax_console'\n");
    return RESULT_SUCCESS;
  }

  if (p->owner) {               /* already in a call */
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
    ast_cli(fd,
            "No call in which to dial on the \"current\" celliax_console, that is [%s]\n",
            celliax_console_active);
  if (s)
    free(s);
  return RESULT_SUCCESS;
}

int celliax_console_set_active(int fd, int argc, char *argv[])
{
  if (argc == 1)
    ast_cli(fd,
            "\"current\" celliax_console is [%s]\n    Enter 'celliax_console show' to see the available interfaces.\n    Enter 'celliax_console interfacename' to change the \"current\" celliax_console.\n",
            celliax_console_active);
  else if (argc != 2)
    return RESULT_SHOWUSAGE;
  else {
    struct celliax_pvt *p;
    if (strcmp(argv[1], "show") == 0) {
      ast_cli(fd, "Available interfaces:\n");
      for (p = celliax_iflist; p; p = p->next)
        ast_cli(fd, "     [%s]\n", p->name);
      return RESULT_SUCCESS;
    }
    p = celliax_console_find_desc(argv[1]);
    if (p == NULL)
      ast_cli(fd, "Interface [%s] do not exists!\n", argv[1]);
    else {
      celliax_console_active = p->name;
      ast_cli(fd, "\"current\" celliax_console is now: [%s]\n", argv[1]);
    }
  }
  return RESULT_SUCCESS;
}

int celliax_console_celliax(int fd, int argc, char *argv[])
{
  return RESULT_SHOWUSAGE;
}

#ifdef ASTERISK_VERSION_1_4
#ifndef AST_MODULE
#define AST_MODULE "chan_celliax"
#endif
AST_MODULE_INFO_STANDARD(ASTERISK_GPL_KEY, "Celliax, Audio-Serial Driver");
#endif /* ASTERISK_VERSION_1_4 */
