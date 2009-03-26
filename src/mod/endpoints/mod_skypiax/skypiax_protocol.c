#include "skypiax.h"

#ifdef ASTERISK
#define skypiax_sleep usleep
#define skypiax_strncpy strncpy
#define tech_pvt p
extern int skypiax_debug;
extern char *skypiax_console_active;
#else /* FREESWITCH */
#define skypiax_sleep switch_sleep
#define skypiax_strncpy switch_copy_string
extern switch_memory_pool_t *skypiax_module_pool;
extern switch_endpoint_interface_t *skypiax_endpoint_interface;
#endif /* ASTERISK */
int samplerate_skypiax = SAMPLERATE_SKYPIAX;

extern int running;

/*************************************/
/* suspicious globals FIXME */
#ifdef WIN32
DWORD win32_dwThreadId;
#else
XErrorHandler old_handler = 0;
int xerror = 0;
#endif /* WIN32 */
/*************************************/

int skypiax_signaling_read(private_t * tech_pvt)
{
  char read_from_pipe[4096];
  char message[4096];
  char message_2[4096];
  char *buf, obj[512] = "", id[512] = "", prop[512] = "", value[512] = "", *where;
  char **stringp = NULL;
  int a;
  unsigned int howmany;
  unsigned int i;

  memset(read_from_pipe, 0, 4096);
  memset(message, 0, 4096);
  memset(message_2, 0, 4096);

  howmany =
    skypiax_pipe_read(tech_pvt->SkypiaxHandles.fdesc[0], (short *) read_from_pipe,
                      sizeof(read_from_pipe));

  a = 0;
  for (i = 0; i < howmany; i++) {
    message[a] = read_from_pipe[i];
    a++;

    if (read_from_pipe[i] == '\0') {

      DEBUGA_SKYPE("READING: |||%s||| \n", SKYPIAX_P_LOG, message);

      if (!strcasecmp(message, "ERROR 68")) {
        DEBUGA_SKYPE
          ("If I don't connect immediately, please give the Skype client authorization to be connected by Skypiax (and to not ask you again)\n",
           SKYPIAX_P_LOG);
        skypiax_sleep(1000000);
        skypiax_signaling_write(tech_pvt, "PROTOCOL 7");
        skypiax_sleep(10000);
        return 0;
      }
      if (!strncasecmp(message, "ERROR 92 CALL", 12)) {
        ERRORA
          ("Skype got ERROR: |||%s|||, the (skypeout) number we called was not recognized as valid\n",
           SKYPIAX_P_LOG, message);
        tech_pvt->skype_callflow = CALLFLOW_STATUS_FINISHED;
        DEBUGA_SKYPE("skype_call now is DOWN\n", SKYPIAX_P_LOG);
        tech_pvt->skype_call_id[0] = '\0';

        if (tech_pvt->interface_state != SKYPIAX_STATE_HANGUP_REQUESTED) {
          tech_pvt->interface_state = SKYPIAX_STATE_DOWN;
          return CALLFLOW_INCOMING_HANGUP;
        } else {
          tech_pvt->interface_state = SKYPIAX_STATE_DOWN;
        }
      }
      skypiax_strncpy(message_2, message, sizeof(message) - 1);
      buf = message;
      stringp = &buf;
      where = strsep(stringp, " ");
      if (!where) {
        WARNINGA("Skype MSG without spaces: %s\n", SKYPIAX_P_LOG, message);
      }
      if (!strcasecmp(message, "ERROR")) {
        DEBUGA_SKYPE("Skype got ERROR: |||%s|||\n", SKYPIAX_P_LOG, message);
        tech_pvt->skype_callflow = CALLFLOW_STATUS_FINISHED;
        DEBUGA_SKYPE("skype_call now is DOWN\n", SKYPIAX_P_LOG);
        tech_pvt->skype_call_id[0] = '\0';

        if (tech_pvt->interface_state != SKYPIAX_STATE_HANGUP_REQUESTED) {
          tech_pvt->interface_state = SKYPIAX_STATE_DOWN;
          return CALLFLOW_INCOMING_HANGUP;
        } else {
          tech_pvt->interface_state = SKYPIAX_STATE_DOWN;
        }
      }
      if (!strcasecmp(message, "CURRENTUSERHANDLE")) {
        skypiax_strncpy(obj, where, sizeof(obj) - 1);
        where = strsep(stringp, " ");
        skypiax_strncpy(id, where, sizeof(id) - 1);
        if (!strcasecmp(id, tech_pvt->skype_user)) {
          tech_pvt->SkypiaxHandles.currentuserhandle = 1;
          DEBUGA_SKYPE
            ("Skype MSG: message: %s, currentuserhandle: %s, cuh: %s, skype_user: %s!\n",
             SKYPIAX_P_LOG, message, obj, id, tech_pvt->skype_user);
        }
      }
      if (!strcasecmp(message, "USER")) {
        skypiax_strncpy(obj, where, sizeof(obj) - 1);
        where = strsep(stringp, " ");
        skypiax_strncpy(id, where, sizeof(id) - 1);
        where = strsep(stringp, " ");
        skypiax_strncpy(prop, where, sizeof(prop) - 1);
        if (!strcasecmp(prop, "RECEIVEDAUTHREQUEST")) {
          char msg_to_skype[256];
          DEBUGA_SKYPE("Skype MSG: message: %s, obj: %s, id: %s, prop: %s!\n",
                       SKYPIAX_P_LOG, message, obj, id, prop);
          //TODO: allow authorization based on config param
          sprintf(msg_to_skype, "SET USER %s ISAUTHORIZED TRUE", id);
          skypiax_signaling_write(tech_pvt, msg_to_skype);
        }
      }
      if (!strcasecmp(message, "MESSAGE")) {
        skypiax_strncpy(obj, where, sizeof(obj) - 1);
        where = strsep(stringp, " ");
        skypiax_strncpy(id, where, sizeof(id) - 1);
        where = strsep(stringp, " ");
        skypiax_strncpy(prop, where, sizeof(prop) - 1);
        if (!strcasecmp(prop, "STATUS")) {
          where = strsep(stringp, " ");
          skypiax_strncpy(value, where, sizeof(value) - 1);
          if (!strcasecmp(value, "RECEIVED")) {
            char msg_to_skype[256];
            DEBUGA_SKYPE("Skype MSG: message: %s, obj: %s, id: %s, prop: %s value: %s!\n",
                         SKYPIAX_P_LOG, message, obj, id, prop, value);
            //TODO: authomatically flag messages as read based on config param
            sprintf(msg_to_skype, "SET MESSAGE %s SEEN", id);
            skypiax_signaling_write(tech_pvt, msg_to_skype);
          }
        } else if (!strcasecmp(prop, "BODY")) {
          char msg_to_skype[256];
          DEBUGA_SKYPE("Skype MSG: message: %s, obj: %s, id: %s, prop: %s!\n",
                       SKYPIAX_P_LOG, message, obj, id, prop);
          //TODO: authomatically flag messages as read based on config param
          sprintf(msg_to_skype, "SET MESSAGE %s SEEN", id);
          skypiax_signaling_write(tech_pvt, msg_to_skype);
        }
      }
      if (!strcasecmp(message, "CALL")) {
        skypiax_strncpy(obj, where, sizeof(obj) - 1);
        where = strsep(stringp, " ");
        skypiax_strncpy(id, where, sizeof(id) - 1);
        where = strsep(stringp, " ");
        skypiax_strncpy(prop, where, sizeof(prop) - 1);
        where = strsep(stringp, " ");
        skypiax_strncpy(value, where, sizeof(value) - 1);
        where = strsep(stringp, " ");

        DEBUGA_SKYPE
          ("Skype MSG: message: %s, obj: %s, id: %s, prop: %s, value: %s,where: %s!\n",
           SKYPIAX_P_LOG, message, obj, id, prop, value, where ? where : "NULL");

        if (!strcasecmp(prop, "PARTNER_HANDLE")) {
          skypiax_strncpy(tech_pvt->callid_number, value, sizeof(tech_pvt->callid_number) - 1);
          DEBUGA_SKYPE
            ("the skype_call %s caller PARTNER_HANDLE (tech_pvt->callid_number) is: %s\n",
             SKYPIAX_P_LOG, id, tech_pvt->callid_number);
          return CALLFLOW_INCOMING_RING;
        }
        if (!strcasecmp(prop, "PARTNER_DISPNAME")) {
          snprintf(tech_pvt->callid_name, sizeof(tech_pvt->callid_name) - 1, "%s%s%s",
                   value, where ? " " : "", where ? where : "");
          DEBUGA_SKYPE
            ("the skype_call %s caller PARTNER_DISPNAME (tech_pvt->callid_name) is: %s\n",
             SKYPIAX_P_LOG, id, tech_pvt->callid_name);
        }
        if (!strcasecmp(prop, "CONF_ID") && !strcasecmp(value, "0")) {
          DEBUGA_SKYPE("the skype_call %s is NOT a conference call\n", SKYPIAX_P_LOG, id);
          if (tech_pvt->interface_state == SKYPIAX_STATE_DOWN)
            tech_pvt->interface_state = SKYPIAX_STATE_PRERING;
        }
        if (!strcasecmp(prop, "CONF_ID") && strcasecmp(value, "0")) {
          DEBUGA_SKYPE("the skype_call %s is a conference call\n", SKYPIAX_P_LOG, id);
          if (tech_pvt->interface_state == SKYPIAX_STATE_DOWN)
            tech_pvt->interface_state = SKYPIAX_STATE_PRERING;
        }
        if (!strcasecmp(prop, "DTMF")) {
          DEBUGA_SKYPE("Call %s received a DTMF: %s\n", SKYPIAX_P_LOG, id, value);
          dtmf_received(tech_pvt, value);
        }
        if (!strcasecmp(prop, "FAILUREREASON")) {
          DEBUGA_SKYPE
            ("Skype FAILED on skype_call %s. Let's wait for the FAILED message.\n",
             SKYPIAX_P_LOG, id);
        }
        if (!strcasecmp(prop, "DURATION") && (!strcasecmp(value, "1"))) {
          if (strcasecmp(id, tech_pvt->skype_call_id)) {
            skypiax_strncpy(tech_pvt->skype_call_id, id, sizeof(tech_pvt->skype_call_id) - 1);
            DEBUGA_SKYPE
              ("We called a Skype contact and he answered us on skype_call: %s.\n",
               SKYPIAX_P_LOG, id);
          }
        }
        if (!strcasecmp(prop, "STATUS")) {

          if (!strcasecmp(value, "RINGING")) {
            char msg_to_skype[1024];
            if (tech_pvt->interface_state != SKYPIAX_STATE_DIALING) {
              /* we are not calling out */
              if (!strlen(tech_pvt->skype_call_id)) {
                /* we are not inside an active call */
                tech_pvt->skype_callflow = CALLFLOW_STATUS_RINGING;
                tech_pvt->interface_state = SKYPIAX_STATE_RING;
                /* no owner, no active call, let's answer */
                skypiax_signaling_write(tech_pvt, "SET AGC OFF");
                skypiax_sleep(10000);
                skypiax_signaling_write(tech_pvt, "SET AEC OFF");
                skypiax_sleep(10000);
                sprintf(msg_to_skype, "GET CALL %s PARTNER_DISPNAME", id);
                skypiax_signaling_write(tech_pvt, msg_to_skype);
                skypiax_sleep(10000);
                sprintf(msg_to_skype, "GET CALL %s PARTNER_HANDLE", id);
                skypiax_signaling_write(tech_pvt, msg_to_skype);
                skypiax_sleep(10000);
                sprintf(msg_to_skype, "ALTER CALL %s ANSWER", id);
                skypiax_signaling_write(tech_pvt, msg_to_skype);
                DEBUGA_SKYPE("We answered a Skype RING on skype_call %s\n", SKYPIAX_P_LOG,
                             id);
                skypiax_strncpy(tech_pvt->skype_call_id, id, sizeof(tech_pvt->skype_call_id) - 1);
              } else {
                /* we're owned, we're in a call, let's try to transfer */
        /************************** TODO
		 Checking here if it is possible to transfer this call to Test2
		 -> GET CALL 288 CAN_TRANSFER Test2
		 <- CALL 288 CAN_TRANSFER test2 TRUE
		**********************************/

                private_t *available_skypiax_interface;

                available_skypiax_interface = find_available_skypiax_interface();
                if (available_skypiax_interface) {
                  /* there is a skypiax interface idle, let's transfer the call to it */
                  DEBUGA_SKYPE
                    ("Let's transfer the skype_call %s to %s interface (with skype_user: %s), because we are already in a skypiax call(%s)\n",
                     SKYPIAX_P_LOG, tech_pvt->skype_call_id,
                     available_skypiax_interface->name, available_skypiax_interface->skype_user, id);
                  sprintf(msg_to_skype, "ALTER CALL %s TRANSFER %s", id,
                          available_skypiax_interface->skype_user);
                } else {
                  /* no skypiax interfaces idle, let's refuse the call */
                  DEBUGA_SKYPE
                    ("Let's refuse the skype_call %s, because we are already in a skypiax call(%s) and no other skypiax interfaces are available\n",
                     SKYPIAX_P_LOG, tech_pvt->skype_call_id, id);
                  sprintf(msg_to_skype, "ALTER CALL %s END HANGUP", id);
                }
                skypiax_signaling_write(tech_pvt, msg_to_skype);
                skypiax_sleep(10000);
                DEBUGA_SKYPE
                  ("We (%s) have NOT answered a Skype RING on skype_call %s, because we are already in a skypiax call\n",
                   SKYPIAX_P_LOG, tech_pvt->skype_call_id, id);
              }
            } else {
              /* we are calling out */
              tech_pvt->skype_callflow = CALLFLOW_STATUS_RINGING;
              tech_pvt->interface_state = SKYPIAX_STATE_RINGING;
              skypiax_strncpy(tech_pvt->skype_call_id, id, sizeof(tech_pvt->skype_call_id) - 1);
              DEBUGA_SKYPE("Our remote party in skype_call %s is RINGING\n",
                           SKYPIAX_P_LOG, id);
              remote_party_is_ringing(tech_pvt);
            }
          } else if (!strcasecmp(value, "EARLYMEDIA")) {
            char msg_to_skype[1024];
            tech_pvt->skype_callflow = CALLFLOW_STATUS_EARLYMEDIA;
            tech_pvt->interface_state = SKYPIAX_STATE_DIALING;
            NOTICA("Our remote party in skype_call %s is EARLYMEDIA\n", SKYPIAX_P_LOG,
                   id);
            sprintf(msg_to_skype, "ALTER CALL %s SET_INPUT PORT=\"%d\"", id,
                    tech_pvt->tcp_cli_port);
            skypiax_signaling_write(tech_pvt, msg_to_skype);
            start_audio_threads(tech_pvt);
            sprintf(msg_to_skype, "ALTER CALL %s SET_OUTPUT PORT=\"%d\"", id,
                    tech_pvt->tcp_srv_port);
            skypiax_signaling_write(tech_pvt, msg_to_skype);

            remote_party_is_early_media(tech_pvt);
          } else if (!strcasecmp(value, "MISSED")) {
            DEBUGA_SKYPE("We missed skype_call %s\n", SKYPIAX_P_LOG, id);
          } else if (!strcasecmp(value, "FINISHED")) {
            DEBUGA_SKYPE("skype_call %s now is DOWN\n", SKYPIAX_P_LOG, id);
            if (!strcasecmp(tech_pvt->skype_call_id, id)) {
              DEBUGA_SKYPE("skype_call %s is MY call, now I'm going DOWN\n",
                           SKYPIAX_P_LOG, id);
              tech_pvt->skype_call_id[0] = '\0';
              if (tech_pvt->interface_state != SKYPIAX_STATE_HANGUP_REQUESTED) {
                //tech_pvt->interface_state = SKYPIAX_STATE_DOWN;
                return CALLFLOW_INCOMING_HANGUP;
              } else {
                tech_pvt->interface_state = SKYPIAX_STATE_DOWN;
              }
            } else {
              DEBUGA_SKYPE("skype_call %s is NOT MY call, ignoring\n", SKYPIAX_P_LOG, id);
            }

          } else if (!strcasecmp(value, "CANCELLED")) {
            tech_pvt->skype_callflow = CALLFLOW_STATUS_CANCELLED;
            DEBUGA_SKYPE
              ("we tried to call Skype on skype_call %s and Skype has now CANCELLED\n",
               SKYPIAX_P_LOG, id);
            tech_pvt->skype_call_id[0] = '\0';
            if (tech_pvt->interface_state != SKYPIAX_STATE_HANGUP_REQUESTED) {
              tech_pvt->interface_state = SKYPIAX_STATE_DOWN;
              return CALLFLOW_INCOMING_HANGUP;
            } else {
              tech_pvt->interface_state = SKYPIAX_STATE_DOWN;
            }
          } else if (!strcasecmp(value, "FAILED")) {
            tech_pvt->skype_callflow = CALLFLOW_STATUS_FAILED;
            DEBUGA_SKYPE
              ("we tried to call Skype on skype_call %s and Skype has now FAILED\n",
               SKYPIAX_P_LOG, id);
            tech_pvt->skype_call_id[0] = '\0';
            skypiax_strncpy(tech_pvt->skype_call_id, id, sizeof(tech_pvt->skype_call_id) - 1);
            tech_pvt->interface_state = SKYPIAX_STATE_DOWN;
            return CALLFLOW_INCOMING_HANGUP;
          } else if (!strcasecmp(value, "REFUSED")) {
            if (!strcasecmp(id, tech_pvt->skype_call_id)) {
              /* this is the id of the call we are in, probably we generated it */
              tech_pvt->skype_callflow = CALLFLOW_STATUS_REFUSED;
              DEBUGA_SKYPE
                ("we tried to call Skype on skype_call %s and Skype has now REFUSED\n",
                 SKYPIAX_P_LOG, id);
              skypiax_strncpy(tech_pvt->skype_call_id, id, sizeof(tech_pvt->skype_call_id) - 1);
              tech_pvt->interface_state = SKYPIAX_STATE_DOWN;
              tech_pvt->skype_call_id[0] = '\0';
              return CALLFLOW_INCOMING_HANGUP;
            } else {
              /* we're here because were us that refused an incoming call */
              DEBUGA_SKYPE("we REFUSED skype_call %s\n", SKYPIAX_P_LOG, id);
            }
          } else if (!strcasecmp(value, "TRANSFERRING")) {
              DEBUGA_SKYPE("skype_call %s is transferring\n", SKYPIAX_P_LOG, id);
          } else if (!strcasecmp(value, "TRANSFERRED")) {
              DEBUGA_SKYPE("skype_call %s has been transferred\n", SKYPIAX_P_LOG, id);
          } else if (!strcasecmp(value, "ROUTING")) {
            tech_pvt->skype_callflow = CALLFLOW_STATUS_ROUTING;
            tech_pvt->interface_state = SKYPIAX_STATE_DIALING;
            skypiax_strncpy(tech_pvt->skype_call_id, id, sizeof(tech_pvt->skype_call_id) - 1);
            DEBUGA_SKYPE("skype_call: %s is now ROUTING\n", SKYPIAX_P_LOG, id);
          } else if (!strcasecmp(value, "UNPLACED")) {
            tech_pvt->skype_callflow = CALLFLOW_STATUS_UNPLACED;
            tech_pvt->interface_state = SKYPIAX_STATE_DIALING;
            skypiax_strncpy(tech_pvt->skype_call_id, id, sizeof(tech_pvt->skype_call_id) - 1);
            DEBUGA_SKYPE("skype_call: %s is now UNPLACED\n", SKYPIAX_P_LOG, id);
          } else if (!strcasecmp(value, "INPROGRESS")) {
            char msg_to_skype[1024];

            if (!strlen(tech_pvt->session_uuid_str) || !strlen(tech_pvt->skype_call_id)
                || !strcasecmp(tech_pvt->skype_call_id, id)) {
              skypiax_strncpy(tech_pvt->skype_call_id, id, sizeof(tech_pvt->skype_call_id) - 1);
              DEBUGA_SKYPE("skype_call: %s is now active\n", SKYPIAX_P_LOG, id);
              if (tech_pvt->skype_callflow != CALLFLOW_STATUS_EARLYMEDIA) {
                tech_pvt->skype_callflow = CALLFLOW_STATUS_INPROGRESS;
                tech_pvt->interface_state = SKYPIAX_STATE_UP;
                sprintf(msg_to_skype, "ALTER CALL %s SET_INPUT PORT=\"%d\"", id,
                        tech_pvt->tcp_cli_port);
                skypiax_signaling_write(tech_pvt, msg_to_skype);
                start_audio_threads(tech_pvt);
                sprintf(msg_to_skype, "ALTER CALL %s SET_OUTPUT PORT=\"%d\"", id,
                        tech_pvt->tcp_srv_port);
                skypiax_signaling_write(tech_pvt, msg_to_skype);
              }
              tech_pvt->skype_callflow = SKYPIAX_STATE_UP;
              if (!strlen(tech_pvt->session_uuid_str)) {
                DEBUGA_SKYPE("New Inbound Channel!\n", SKYPIAX_P_LOG);
                new_inbound_channel(tech_pvt);
              } else {
                DEBUGA_SKYPE("Outbound Channel Answered!\n", SKYPIAX_P_LOG);
                outbound_channel_answered(tech_pvt);
              }
            } else {
              DEBUGA_SKYPE("I'm on %s, skype_call %s is NOT MY call, ignoring\n",
                           SKYPIAX_P_LOG, tech_pvt->skype_call_id, id);
            }
          } else {
            WARNINGA("skype_call: %s, STATUS: %s is not recognized\n", SKYPIAX_P_LOG, id,
                     value);
          }
        }                       //STATUS
      }                         //CALL
      /* the "numbered" messages that follows are used by the directory application, not yet ported */
      if (!strcasecmp(message, "#333")) {
        /* DEBUGA_SKYPE("Skype MSG: message_2: %s, message2[11]: %s\n", SKYPIAX_P_LOG,
         * message_2, &message_2[11]); */
        memset(tech_pvt->skype_friends, 0, 4096);
        skypiax_strncpy(tech_pvt->skype_friends, &message_2[11], 4095);
      }
      if (!strcasecmp(message, "#222")) {
        /* DEBUGA_SKYPE("Skype MSG: message_2: %s, message2[10]: %s\n", SKYPIAX_P_LOG,
         * message_2, &message_2[10]); */
        memset(tech_pvt->skype_fullname, 0, 512);
        skypiax_strncpy(tech_pvt->skype_fullname, &message_2[10], 511);
      }
      if (!strcasecmp(message, "#765")) {
        /* DEBUGA_SKYPE("Skype MSG: message_2: %s, message2[10]: %s\n", SKYPIAX_P_LOG,
         * message_2, &message_2[10]); */
        memset(tech_pvt->skype_displayname, 0, 512);
        skypiax_strncpy(tech_pvt->skype_displayname, &message_2[10], 511);
      }
      a = 0;
    }                           //message end
  }                             //read_from_pipe
  return 0;
}

void *skypiax_do_tcp_srv_thread_func(void *obj)
{
  private_t *tech_pvt = obj;
  int s;
  unsigned int len;
  unsigned int i;
  unsigned int a;
#if defined(WIN32) && !defined(__CYGWIN__)
  int sin_size;
#else /* WIN32 */
  unsigned int sin_size;
#endif /* WIN32 */
  unsigned int fd;
  short srv_in[SAMPLES_PER_FRAME];
  short srv_out[SAMPLES_PER_FRAME / 2];
  struct sockaddr_in my_addr;
  struct sockaddr_in remote_addr;
  int exit = 0;
  unsigned int kill_cli_size;
  short kill_cli_buff[SAMPLES_PER_FRAME];
  short totalbuf[SAMPLES_PER_FRAME];

  memset(&my_addr, 0, sizeof(my_addr));
  my_addr.sin_family = AF_INET;
  my_addr.sin_addr.s_addr = htonl(0x7f000001);  /* use the localhost */
  my_addr.sin_port = htons(tech_pvt->tcp_srv_port);

  if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    ERRORA("socket Error\n", SKYPIAX_P_LOG);
    return NULL;
  }

  if (bind(s, (struct sockaddr *) &my_addr, sizeof(struct sockaddr)) < 0) {
    ERRORA("bind Error\n", SKYPIAX_P_LOG);
    return NULL;
  }
  DEBUGA_SKYPE("started tcp_srv_thread thread.\n", SKYPIAX_P_LOG);

  listen(s, 6);

  sin_size = sizeof(remote_addr);
  while ((fd = accept(s, (struct sockaddr *) &remote_addr, &sin_size)) > 0) {
    DEBUGA_SKYPE("ACCEPTED\n", SKYPIAX_P_LOG);
    if (!running)
      break;
    while (tech_pvt->interface_state != SKYPIAX_STATE_DOWN
           && (tech_pvt->skype_callflow == CALLFLOW_STATUS_INPROGRESS
               || tech_pvt->skype_callflow == CALLFLOW_STATUS_EARLYMEDIA
               || tech_pvt->skype_callflow == SKYPIAX_STATE_UP)) {

      unsigned int fdselect;
      int rt;
      fd_set fs;
      struct timeval to;

      if (!running)
        break;
      exit = 1;

      fdselect = fd;
      FD_ZERO(&fs);
      FD_SET(fdselect, &fs);
      to.tv_usec = 2000000;     //2000 msec
      to.tv_sec = 0;

      rt = select(fdselect + 1, &fs, NULL, NULL, &to);
      if (rt > 0) {

        len = recv(fd, (char *) srv_in, 320, 0);    //seems that Skype only sends 320 bytes at time

        if (len == 320) {
          unsigned int howmany;

          if (samplerate_skypiax == 8000) {
            /* we're downsampling from 16khz to 8khz, srv_out will contain each other sample from srv_in */
            a = 0;
            for (i = 0; i < len / sizeof(short); i++) {
              srv_out[a] = srv_in[i];
              i++;
              a++;
            }
          } else if (samplerate_skypiax == 16000) {
            /* we're NOT downsampling, srv_out will contain ALL samples from srv_in */
            for (i = 0; i < len / sizeof(short); i++) {
              srv_out[i] = srv_in[i];
            }
          } else {
            ERRORA("SAMPLERATE_SKYPIAX can only be 8000 or 16000\n", SKYPIAX_P_LOG);
          }
          /* if not yet done, let's store the half incoming frame */
          if (!tech_pvt->audiobuf_is_loaded) {
            for (i = 0; i < SAMPLES_PER_FRAME / 2; i++) {
              tech_pvt->audiobuf[i] = srv_out[i];
            }
            tech_pvt->audiobuf_is_loaded = 1;
          } else {
            /* we got a stored half frame, build a complete frame in totalbuf using the stored half frame and the current half frame */
            for (i = 0; i < SAMPLES_PER_FRAME / 2; i++) {
              totalbuf[i] = tech_pvt->audiobuf[i];
            }
            for (a = 0; a < SAMPLES_PER_FRAME / 2; a++) {
              totalbuf[i] = srv_out[a];
              i++;
            }
            /* send the complete frame through the pipe to our code waiting for incoming audio */
            howmany =
              skypiax_pipe_write(tech_pvt->audiopipe[1], totalbuf,
                                 SAMPLES_PER_FRAME * sizeof(short));
            if (howmany != SAMPLES_PER_FRAME * sizeof(short)) {
              ERRORA("howmany is %d, but was expected to be %d\n", SKYPIAX_P_LOG, howmany,
                     (int) (SAMPLES_PER_FRAME * sizeof(short)));
            }
            /* done with the stored half frame */
            tech_pvt->audiobuf_is_loaded = 0;
          }

        } else if (len == 0) {
          skypiax_sleep(1000);
        } else {
          ERRORA("len=%d\n", SKYPIAX_P_LOG, len);
          exit = 1;
          break;
        }

      } else {
        if (rt)
          ERRORA("SRV rt=%d\n", SKYPIAX_P_LOG, rt);
        skypiax_sleep(10000);
      }

    }

    /* let's send some frame in the pipes, so both tcp_cli and tcp_srv will have an occasion to die */
    kill_cli_size = SAMPLES_PER_FRAME * sizeof(short);
    len = skypiax_pipe_write(tech_pvt->audiopipe[1], kill_cli_buff, kill_cli_size);
    kill_cli_size = SAMPLES_PER_FRAME * sizeof(short);
    len = skypiax_pipe_write(tech_pvt->audioskypepipe[1], kill_cli_buff, kill_cli_size);
    tech_pvt->interface_state = SKYPIAX_STATE_DOWN;
    kill_cli_size = SAMPLES_PER_FRAME * sizeof(short);
    len = skypiax_pipe_write(tech_pvt->audiopipe[1], kill_cli_buff, kill_cli_size);
    kill_cli_size = SAMPLES_PER_FRAME * sizeof(short);
    len = skypiax_pipe_write(tech_pvt->audioskypepipe[1], kill_cli_buff, kill_cli_size);

    DEBUGA_SKYPE("Skype incoming audio GONE\n", SKYPIAX_P_LOG);
    skypiax_close_socket(fd);
    if (exit)
      break;
  }

  DEBUGA_SKYPE("incoming audio server (I am it) EXITING\n", SKYPIAX_P_LOG);
  skypiax_close_socket(s);
  return NULL;
}

void *skypiax_do_tcp_cli_thread_func(void *obj)
{
  private_t *tech_pvt = obj;
  int s;
  struct sockaddr_in my_addr;
  struct sockaddr_in remote_addr;
  unsigned int got;
  unsigned int len;
  unsigned int i;
  unsigned int a;
  unsigned int fd;
  short cli_out[SAMPLES_PER_FRAME * 2];
  short cli_in[SAMPLES_PER_FRAME];
#ifdef WIN32
  int sin_size;
#else
  unsigned int sin_size;
#endif /* WIN32 */

  memset(&my_addr, 0, sizeof(my_addr));
  my_addr.sin_family = AF_INET;
  my_addr.sin_addr.s_addr = htonl(0x7f000001);  /* use the localhost */
  my_addr.sin_port = htons(tech_pvt->tcp_cli_port);

  if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    ERRORA("socket Error\n", SKYPIAX_P_LOG);
    return NULL;
  }

  if (bind(s, (struct sockaddr *) &my_addr, sizeof(struct sockaddr)) < 0) {
    ERRORA("bind Error\n", SKYPIAX_P_LOG);
    skypiax_close_socket(s);
    return NULL;
  }
  DEBUGA_SKYPE("started tcp_cli_thread thread.\n", SKYPIAX_P_LOG);

  listen(s, 6);

  sin_size = sizeof(remote_addr);
  while ((fd = accept(s, (struct sockaddr *) &remote_addr, &sin_size)) > 0) {
    DEBUGA_SKYPE("ACCEPTED\n", SKYPIAX_P_LOG);
    if (!running)
      break;
    while (tech_pvt->interface_state != SKYPIAX_STATE_DOWN
           && (tech_pvt->skype_callflow == CALLFLOW_STATUS_INPROGRESS
               || tech_pvt->skype_callflow == CALLFLOW_STATUS_EARLYMEDIA
               || tech_pvt->skype_callflow == SKYPIAX_STATE_UP)) {
      unsigned int fdselect;
      int rt;
      fd_set fs;
      struct timeval to;

      if (!running)
        break;
      FD_ZERO(&fs);
      to.tv_usec = 60000;       //60msec
      to.tv_sec = 0;
#if defined(WIN32) && !defined(__CYGWIN__)
/* on win32 we cannot select from the apr "pipe", so we select on socket writability */
      fdselect = fd;
      FD_SET(fdselect, &fs);

      rt = select(fdselect + 1, NULL, &fs, NULL, &to);
#else
/* on *unix and cygwin we select from the real pipe */
      fdselect = tech_pvt->audioskypepipe[0];
      FD_SET(fdselect, &fs);

      rt = select(fdselect + 1, &fs, NULL, NULL, &to);
#endif

      if (rt > 0) {
        /* read from the pipe the audio frame we are supposed to send out */
        got =
          skypiax_pipe_read(tech_pvt->audioskypepipe[0], cli_in,
                            SAMPLES_PER_FRAME * sizeof(short));
        if (got != SAMPLES_PER_FRAME * sizeof(short)) {
          WARNINGA("got is %d, but was expected to be %d\n", SKYPIAX_P_LOG, got,
                   (int) (SAMPLES_PER_FRAME * sizeof(short)));
        }

        if (got == SAMPLES_PER_FRAME * sizeof(short)) {
          if (samplerate_skypiax == 8000) {

            /* we're upsampling from 8khz to 16khz, cli_out will contain two times each sample from cli_in */
            a = 0;
            for (i = 0; i < got / sizeof(short); i++) {
              cli_out[a] = cli_in[i];
              a++;
              cli_out[a] = cli_in[i];
              a++;
            }
            got = got * 2;
          } else if (samplerate_skypiax == 16000) {
            /* we're NOT upsampling, cli_out will contain just ALL samples from cli_in */
            for (i = 0; i < got / sizeof(short); i++) {
              cli_out[i] = cli_in[i];
            }
          } else {
            ERRORA("SAMPLERATE_SKYPIAX can only be 8000 or 16000\n", SKYPIAX_P_LOG);
          }

          /* send the 16khz frame to the Skype client waiting for incoming audio to be sent to the remote party */
          len = send(fd, (char *) cli_out, got, 0);
          skypiax_sleep(5000);  //5 msec

          if (len == -1) {
            break;
          } else if (len != got) {
            ERRORA("len=%d\n", SKYPIAX_P_LOG, len);
            skypiax_sleep(1000);
            break;
          }

        } else {

          WARNINGA("got is %d, but was expected to be %d\n", SKYPIAX_P_LOG, got,
                   (int) (SAMPLES_PER_FRAME * sizeof(short)));
        }
      } else {
        if (rt)
          ERRORA("CLI rt=%d\n", SKYPIAX_P_LOG, rt);
        memset(cli_out, 0, sizeof(cli_out));
        len = send(fd, (char *) cli_out, sizeof(cli_out), 0);
        len = send(fd, (char *) cli_out, sizeof(cli_out) / 2, 0);
        //DEBUGA_SKYPE("sent %d of zeros to keep the Skype client socket busy\n", SKYPIAX_P_LOG, sizeof(cli_out) + sizeof(cli_out)/2);
      }

    }
    DEBUGA_SKYPE("Skype outbound audio GONE\n", SKYPIAX_P_LOG);
    skypiax_close_socket(fd);
    break;
  }

  DEBUGA_SKYPE("outbound audio server (I am it) EXITING\n", SKYPIAX_P_LOG);
  skypiax_close_socket(s);
  return NULL;
}

int skypiax_audio_read(private_t * tech_pvt)
{
  unsigned int samples;

  samples =
    skypiax_pipe_read(tech_pvt->audiopipe[0], tech_pvt->read_frame.data,
                      SAMPLES_PER_FRAME * sizeof(short));

  if (samples != SAMPLES_PER_FRAME * sizeof(short)) {
    if (samples)
      WARNINGA("read samples=%u expected=%u\n", SKYPIAX_P_LOG, samples,
               (int) (SAMPLES_PER_FRAME * sizeof(short)));
    return 0;
  } else {
    /* A real frame */
    tech_pvt->read_frame.datalen = samples;
  }
  return 1;
}

int skypiax_senddigit(private_t * tech_pvt, char digit)
{
  char msg_to_skype[1024];

  DEBUGA_SKYPE("DIGIT received: %c\n", SKYPIAX_P_LOG, digit);
  sprintf(msg_to_skype, "SET CALL %s DTMF %c", tech_pvt->skype_call_id, digit);
  skypiax_signaling_write(tech_pvt, msg_to_skype);

  return 0;
}

int skypiax_call(private_t * tech_pvt, char *rdest, int timeout)
{
  char msg_to_skype[1024];

  skypiax_sleep(5000);
  DEBUGA_SKYPE("Calling Skype, rdest is: %s\n", SKYPIAX_P_LOG, rdest);
  skypiax_signaling_write(tech_pvt, "SET AGC OFF");
  skypiax_sleep(10000);
  skypiax_signaling_write(tech_pvt, "SET AEC OFF");
  skypiax_sleep(10000);

  sprintf(msg_to_skype, "CALL %s", rdest);
  if (skypiax_signaling_write(tech_pvt, msg_to_skype) < 0) {
    ERRORA("failed to communicate with Skype client, now exit\n", SKYPIAX_P_LOG);
    return -1;
  }
  return 0;
}

/***************************/
/* PLATFORM SPECIFIC */
/***************************/
#if defined(WIN32) && !defined(__CYGWIN__)
int skypiax_pipe_read(switch_file_t * pipe, short *buf, int howmany)
{
  switch_size_t quantity;

  quantity = howmany;

  switch_file_read(pipe, buf, &quantity);

  howmany = quantity;

  return howmany;
}
int skypiax_pipe_write(switch_file_t * pipe, short *buf, int howmany)
{
  switch_size_t quantity;

  quantity = howmany;

  switch_file_write(pipe, buf, &quantity);

  howmany = quantity;

  return howmany;
}
int skypiax_close_socket(unsigned int fd)
{
  int res;

  res = closesocket(fd);

  return res;
}

int skypiax_audio_init(private_t * tech_pvt)
{
  switch_status_t rv;
  rv =
    switch_file_pipe_create(&tech_pvt->audiopipe[0], &tech_pvt->audiopipe[1],
                            skypiax_module_pool);
  rv =
    switch_file_pipe_create(&tech_pvt->audioskypepipe[0], &tech_pvt->audioskypepipe[1],
                            skypiax_module_pool);
  return 0;
}
#else /* WIN32 */
int skypiax_pipe_read(int pipe, short *buf, int howmany)
{
  howmany = read(pipe, buf, howmany);
  return howmany;
}
int skypiax_pipe_write(int pipe, short *buf, int howmany)
{
  howmany = write(pipe, buf, howmany);
  return howmany;
}
int skypiax_close_socket(unsigned int fd)
{
  int res;

  res = close(fd);

  return res;
}

int skypiax_audio_init(private_t * tech_pvt)
{
  if (pipe(tech_pvt->audiopipe)) {
    fcntl(tech_pvt->audiopipe[0], F_SETFL, O_NONBLOCK);
    fcntl(tech_pvt->audiopipe[1], F_SETFL, O_NONBLOCK);
  }
  if (pipe(tech_pvt->audioskypepipe)) {
    fcntl(tech_pvt->audioskypepipe[0], F_SETFL, O_NONBLOCK);
    fcntl(tech_pvt->audioskypepipe[1], F_SETFL, O_NONBLOCK);
  }

/* this pipe is the audio fd for asterisk to poll on during a call. FS do not use it */
  tech_pvt->skypiax_sound_capt_fd = tech_pvt->audiopipe[0];

  return 0;
}
#endif /* WIN32 */

#ifdef WIN32

enum {
  SKYPECONTROLAPI_ATTACH_SUCCESS = 0,   /*  Client is successfully 
                                           attached and API window handle can be found
                                           in wParam parameter */
  SKYPECONTROLAPI_ATTACH_PENDING_AUTHORIZATION = 1, /*  Skype has acknowledged
                                                       connection request and is waiting
                                                       for confirmation from the user. */
  /*  The client is not yet attached 
   * and should wait for SKYPECONTROLAPI_ATTACH_SUCCESS message */
  SKYPECONTROLAPI_ATTACH_REFUSED = 2,   /*  User has explicitly
                                           denied access to client */
  SKYPECONTROLAPI_ATTACH_NOT_AVAILABLE = 3, /*  API is not available
                                               at the moment.
                                               For example, this happens when no user
                                               is currently logged in. */
  /*  Client should wait for 
   * SKYPECONTROLAPI_ATTACH_API_AVAILABLE 
   * broadcast before making any further */
  /*  connection attempts. */
  SKYPECONTROLAPI_ATTACH_API_AVAILABLE = 0x8001
};

/* Visual C do not have strsep? */
char
 *strsep(char **stringp, const char *delim)
{
  char *res;

  if (!stringp || !*stringp || !**stringp)
    return (char *) 0;

  res = *stringp;
  while (**stringp && !strchr(delim, **stringp))
    ++(*stringp);

  if (**stringp) {
    **stringp = '\0';
    ++(*stringp);
  }

  return res;
}

int skypiax_signaling_write(private_t * tech_pvt, char *msg_to_skype)
{
  static char acInputRow[1024];
  COPYDATASTRUCT oCopyData;

  DEBUGA_SKYPE("SENDING: |||%s||||\n", SKYPIAX_P_LOG, msg_to_skype);

  sprintf(acInputRow, "%s", msg_to_skype);
  DEBUGA_SKYPE("acInputRow: |||%s||||\n", SKYPIAX_P_LOG, acInputRow);
  /*  send command to skype */
  oCopyData.dwData = 0;
  oCopyData.lpData = acInputRow;
  oCopyData.cbData = strlen(acInputRow) + 1;
  if (oCopyData.cbData != 1) {
    if (SendMessage
        (tech_pvt->SkypiaxHandles.win32_hGlobal_SkypeAPIWindowHandle, WM_COPYDATA,
         (WPARAM) tech_pvt->SkypiaxHandles.win32_hInit_MainWindowHandle,
         (LPARAM) & oCopyData) == FALSE) {
      ERRORA
        ("Sending message failed - probably Skype crashed.\n\nPlease shutdown Skypiax, then launch Skypiax and try again.\n",
         SKYPIAX_P_LOG);
      return -1;
    }
  }

  return 0;

}

LRESULT APIENTRY skypiax_present(HWND hWindow, UINT uiMessage, WPARAM uiParam,
                                 LPARAM ulParam)
{
  LRESULT lReturnCode;
  int fIssueDefProc;
  private_t *tech_pvt = NULL;

  lReturnCode = 0;
  fIssueDefProc = 0;
  tech_pvt = (private_t *) GetWindowLong(hWindow, GWL_USERDATA);
  if (!running)
    return lReturnCode;
  switch (uiMessage) {
  case WM_CREATE:
    tech_pvt = (private_t *) ((LPCREATESTRUCT) ulParam)->lpCreateParams;
    SetWindowLong(hWindow, GWL_USERDATA, (LONG) tech_pvt);
    DEBUGA_SKYPE("got CREATE\n", SKYPIAX_P_LOG);
    break;
  case WM_DESTROY:
    DEBUGA_SKYPE("got DESTROY\n", SKYPIAX_P_LOG);
    tech_pvt->SkypiaxHandles.win32_hInit_MainWindowHandle = NULL;
    PostQuitMessage(0);
    break;
  case WM_COPYDATA:
    if (tech_pvt->SkypiaxHandles.win32_hGlobal_SkypeAPIWindowHandle == (HWND) uiParam) {
      unsigned int howmany;
      char msg_from_skype[2048];

      PCOPYDATASTRUCT poCopyData = (PCOPYDATASTRUCT) ulParam;

      memset(msg_from_skype, '\0', sizeof(msg_from_skype));
      skypiax_strncpy(msg_from_skype, (const char *) poCopyData->lpData,
              sizeof(msg_from_skype) - 2);

      howmany = strlen(msg_from_skype) + 1;
      howmany =
        skypiax_pipe_write(tech_pvt->SkypiaxHandles.fdesc[1], (short *) msg_from_skype,
                           howmany);
      //DEBUGA_SKYPE("From Skype API: %s\n", SKYPIAX_P_LOG, msg_from_skype);
      lReturnCode = 1;
    }
    break;
  default:
    if (tech_pvt && tech_pvt->SkypiaxHandles.win32_uiGlobal_MsgID_SkypeControlAPIAttach) {
      if (uiMessage ==
          tech_pvt->SkypiaxHandles.win32_uiGlobal_MsgID_SkypeControlAPIAttach) {
        switch (ulParam) {
        case SKYPECONTROLAPI_ATTACH_SUCCESS:
          if (!tech_pvt->SkypiaxHandles.currentuserhandle) {
            //DEBUGA_SKYPE("\n\n\tConnected to Skype API!\n", SKYPIAX_P_LOG);
            tech_pvt->SkypiaxHandles.api_connected = 1;
            tech_pvt->SkypiaxHandles.win32_hGlobal_SkypeAPIWindowHandle = (HWND) uiParam;
            tech_pvt->SkypiaxHandles.win32_hGlobal_SkypeAPIWindowHandle =
              tech_pvt->SkypiaxHandles.win32_hGlobal_SkypeAPIWindowHandle;
          }
          break;
        case SKYPECONTROLAPI_ATTACH_PENDING_AUTHORIZATION:
          //DEBUGA_SKYPE ("\n\n\tIf I do not (almost) immediately connect to Skype API,\n\tplease give the Skype client authorization to be connected \n\tby Asterisk and to not ask you again.\n\n", SKYPIAX_P_LOG);
          skypiax_sleep(5000);
          if (!tech_pvt->SkypiaxHandles.currentuserhandle) {
            SendMessage(HWND_BROADCAST,
                        tech_pvt->SkypiaxHandles.
                        win32_uiGlobal_MsgID_SkypeControlAPIDiscover,
                        (WPARAM) tech_pvt->SkypiaxHandles.win32_hInit_MainWindowHandle,
                        0);
          }
          break;
        case SKYPECONTROLAPI_ATTACH_REFUSED:
          ERRORA("Skype client refused to be connected by Skypiax!\n", SKYPIAX_P_LOG);
          break;
        case SKYPECONTROLAPI_ATTACH_NOT_AVAILABLE:
          ERRORA("Skype API not (yet?) available\n", SKYPIAX_P_LOG);
          break;
        case SKYPECONTROLAPI_ATTACH_API_AVAILABLE:
          DEBUGA_SKYPE("Skype API available\n", SKYPIAX_P_LOG);
          skypiax_sleep(5000);
          if (!tech_pvt->SkypiaxHandles.currentuserhandle) {
            SendMessage(HWND_BROADCAST,
                        tech_pvt->SkypiaxHandles.
                        win32_uiGlobal_MsgID_SkypeControlAPIDiscover,
                        (WPARAM) tech_pvt->SkypiaxHandles.win32_hInit_MainWindowHandle,
                        0);
          }
          break;
        default:
          WARNINGA("GOT AN UNKNOWN SKYPE WINDOWS MSG\n", SKYPIAX_P_LOG);
        }
        lReturnCode = 1;
        break;
      }
    }
    fIssueDefProc = 1;
    break;
  }
  if (fIssueDefProc)
    lReturnCode = DefWindowProc(hWindow, uiMessage, uiParam, ulParam);
  return (lReturnCode);
}

int win32_Initialize_CreateWindowClass(private_t * tech_pvt)
{
  unsigned char *paucUUIDString;
  RPC_STATUS lUUIDResult;
  int fReturnStatus;
  UUID oUUID;

  fReturnStatus = 0;
  lUUIDResult = UuidCreate(&oUUID);
  tech_pvt->SkypiaxHandles.win32_hInit_ProcessHandle =
    (HINSTANCE) OpenProcess(PROCESS_DUP_HANDLE, FALSE, GetCurrentProcessId());
  if (tech_pvt->SkypiaxHandles.win32_hInit_ProcessHandle != NULL
      && (lUUIDResult == RPC_S_OK || lUUIDResult == RPC_S_UUID_LOCAL_ONLY)) {
    if (UuidToString(&oUUID, &paucUUIDString) == RPC_S_OK) {
      WNDCLASS oWindowClass;

      strcpy(tech_pvt->SkypiaxHandles.win32_acInit_WindowClassName, "Skype-API-Skypiax-");
      strcat(tech_pvt->SkypiaxHandles.win32_acInit_WindowClassName,
             (char *) paucUUIDString);

      oWindowClass.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
      oWindowClass.lpfnWndProc = (WNDPROC) & skypiax_present;
      oWindowClass.cbClsExtra = 0;
      oWindowClass.cbWndExtra = 0;
      oWindowClass.hInstance = tech_pvt->SkypiaxHandles.win32_hInit_ProcessHandle;
      oWindowClass.hIcon = NULL;
      oWindowClass.hCursor = NULL;
      oWindowClass.hbrBackground = NULL;
      oWindowClass.lpszMenuName = NULL;
      oWindowClass.lpszClassName = tech_pvt->SkypiaxHandles.win32_acInit_WindowClassName;

      if (RegisterClass(&oWindowClass) != 0)
        fReturnStatus = 1;

      RpcStringFree(&paucUUIDString);
    }
  }
  if (fReturnStatus == 0)
    CloseHandle(tech_pvt->SkypiaxHandles.win32_hInit_ProcessHandle);
      tech_pvt->SkypiaxHandles.win32_hInit_ProcessHandle = NULL;
  return (fReturnStatus);
}

void win32_DeInitialize_DestroyWindowClass(private_t * tech_pvt)
{
  UnregisterClass(tech_pvt->SkypiaxHandles.win32_acInit_WindowClassName,
                  tech_pvt->SkypiaxHandles.win32_hInit_ProcessHandle);
  CloseHandle(tech_pvt->SkypiaxHandles.win32_hInit_ProcessHandle);
    tech_pvt->SkypiaxHandles.win32_hInit_ProcessHandle = NULL;
}

int win32_Initialize_CreateMainWindow(private_t * tech_pvt)
{
  tech_pvt->SkypiaxHandles.win32_hInit_MainWindowHandle =
    CreateWindowEx(WS_EX_APPWINDOW | WS_EX_WINDOWEDGE,
                   tech_pvt->SkypiaxHandles.win32_acInit_WindowClassName, "",
                   WS_BORDER | WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT,
                   128, 128, NULL, 0, tech_pvt->SkypiaxHandles.win32_hInit_ProcessHandle,
                   tech_pvt);
  return (tech_pvt->SkypiaxHandles.win32_hInit_MainWindowHandle != NULL ? 1 : 0);
}

void win32_DeInitialize_DestroyMainWindow(private_t * tech_pvt)
{
  if (tech_pvt->SkypiaxHandles.win32_hInit_MainWindowHandle != NULL)
    DestroyWindow(tech_pvt->SkypiaxHandles.win32_hInit_MainWindowHandle),
      tech_pvt->SkypiaxHandles.win32_hInit_MainWindowHandle = NULL;
}

void *skypiax_do_skypeapi_thread_func(void *obj)
{
  private_t *tech_pvt = obj;
#if defined(WIN32) && !defined(__CYGWIN__)
  switch_status_t rv;

  switch_file_pipe_create(&tech_pvt->SkypiaxHandles.fdesc[0],
                          &tech_pvt->SkypiaxHandles.fdesc[1], skypiax_module_pool);
  rv =
    switch_file_pipe_create(&tech_pvt->SkypiaxHandles.fdesc[0],
                            &tech_pvt->SkypiaxHandles.fdesc[1], skypiax_module_pool);
#else /* WIN32 */
  if (pipe(tech_pvt->SkypiaxHandles.fdesc)) {
    fcntl(tech_pvt->SkypiaxHandles.fdesc[0], F_SETFL, O_NONBLOCK);
    fcntl(tech_pvt->SkypiaxHandles.fdesc[1], F_SETFL, O_NONBLOCK);
  }
#endif /* WIN32 */

  tech_pvt->SkypiaxHandles.win32_uiGlobal_MsgID_SkypeControlAPIAttach =
    RegisterWindowMessage("SkypeControlAPIAttach");
  tech_pvt->SkypiaxHandles.win32_uiGlobal_MsgID_SkypeControlAPIDiscover =
    RegisterWindowMessage("SkypeControlAPIDiscover");

  skypiax_sleep(2000000);

  if (tech_pvt->SkypiaxHandles.win32_uiGlobal_MsgID_SkypeControlAPIAttach != 0
      && tech_pvt->SkypiaxHandles.win32_uiGlobal_MsgID_SkypeControlAPIDiscover != 0) {
    if (win32_Initialize_CreateWindowClass(tech_pvt)) {
      if (win32_Initialize_CreateMainWindow(tech_pvt)) {
        if (SendMessage
            (HWND_BROADCAST,
             tech_pvt->SkypiaxHandles.win32_uiGlobal_MsgID_SkypeControlAPIDiscover,
             (WPARAM) tech_pvt->SkypiaxHandles.win32_hInit_MainWindowHandle, 0) != 0) {
          tech_pvt->SkypiaxHandles.win32_hInit_MainWindowHandle =
            tech_pvt->SkypiaxHandles.win32_hInit_MainWindowHandle;
          while (running) {
            MSG oMessage;
            if (!running)
              break;
            while (GetMessage(&oMessage, 0, 0, 0)) {
              TranslateMessage(&oMessage);
              DispatchMessage(&oMessage);
            }
          }
        }
        win32_DeInitialize_DestroyMainWindow(tech_pvt);
      }
      win32_DeInitialize_DestroyWindowClass(tech_pvt);
    }
  }

  return NULL;
}

#else /* NOT WIN32 */
int X11_errors_handler(Display * dpy, XErrorEvent * err)
{
  (void) dpy;
  private_t *tech_pvt = NULL;

  xerror = err->error_code;
  ERRORA("Received error code %d from X Server\n\n", SKYPIAX_P_LOG, xerror);
  running = 0;
  return 0;                     /*  ignore the error */
}

static void X11_errors_trap(void)
{
  xerror = 0;
  old_handler = XSetErrorHandler(X11_errors_handler);
}

static int X11_errors_untrap(void)
{
  XSetErrorHandler(old_handler);
  return (xerror != BadValue) && (xerror != BadWindow);
}

int skypiax_send_message(struct SkypiaxHandles *SkypiaxHandles, const char *message_P)
{

  Window w_P;
  Display *disp;
  Window handle_P;
  int ok;
  private_t *tech_pvt = NULL;

  w_P = SkypiaxHandles->skype_win;
  disp = SkypiaxHandles->disp;
  handle_P = SkypiaxHandles->win;

  Atom atom1 = XInternAtom(disp, "SKYPECONTROLAPI_MESSAGE_BEGIN", False);
  Atom atom2 = XInternAtom(disp, "SKYPECONTROLAPI_MESSAGE", False);
  unsigned int pos = 0;
  unsigned int len = strlen(message_P);
  XEvent e;

  memset(&e, 0, sizeof(e));
  e.xclient.type = ClientMessage;
  e.xclient.message_type = atom1;   /*  leading message */
  e.xclient.display = disp;
  e.xclient.window = handle_P;
  e.xclient.format = 8;

  X11_errors_trap();
  //XLockDisplay(disp);
  do {
    unsigned int i;
    for (i = 0; i < 20 && i + pos <= len; ++i)
      e.xclient.data.b[i] = message_P[i + pos];
    XSendEvent(disp, w_P, False, 0, &e);

    e.xclient.message_type = atom2; /*  following messages */
    pos += i;
  } while (pos <= len);

  XSync(disp, False);
  ok = X11_errors_untrap();

  if (!ok)
    ERRORA("Sending message failed with status %d\n", SKYPIAX_P_LOG, xerror);
  //XUnlockDisplay(disp);

  return 1;
}
int skypiax_signaling_write(private_t * tech_pvt, char *msg_to_skype)
{
  struct SkypiaxHandles *SkypiaxHandles;

  DEBUGA_SKYPE("SENDING: |||%s||||\n", SKYPIAX_P_LOG, msg_to_skype);

  SkypiaxHandles = &tech_pvt->SkypiaxHandles;

  if (!skypiax_send_message(SkypiaxHandles, msg_to_skype)) {
    ERRORA
      ("Sending message failed - probably Skype crashed.\n\nPlease shutdown Skypiax, then restart Skype, then launch Skypiax and try again.\n",
       SKYPIAX_P_LOG);
    return -1;
  }

  return 0;

}

int skypiax_present(struct SkypiaxHandles *SkypiaxHandles)
{
  Atom skype_inst = XInternAtom(SkypiaxHandles->disp, "_SKYPE_INSTANCE", True);

  Atom type_ret;
  int format_ret;
  unsigned long nitems_ret;
  unsigned long bytes_after_ret;
  unsigned char *prop;
  int status;
  private_t *tech_pvt = NULL;

  X11_errors_trap();
  //XLockDisplay(disp);
  status =
    XGetWindowProperty(SkypiaxHandles->disp, DefaultRootWindow(SkypiaxHandles->disp),
                       skype_inst, 0, 1, False, XA_WINDOW, &type_ret, &format_ret,
                       &nitems_ret, &bytes_after_ret, &prop);
  //XUnlockDisplay(disp);
  X11_errors_untrap();

  /*  sanity check */
  if (status != Success || format_ret != 32 || nitems_ret != 1) {
    SkypiaxHandles->skype_win = (Window) - 1;
    DEBUGA_SKYPE("Skype instance not found\n", SKYPIAX_P_LOG);
    running = 0;
    SkypiaxHandles->api_connected = 0;
    return 0;
  }

  SkypiaxHandles->skype_win = *(const unsigned long *) prop & 0xffffffff;
  DEBUGA_SKYPE("Skype instance found with id #%d\n", SKYPIAX_P_LOG,
               (unsigned int) SkypiaxHandles->skype_win);
  SkypiaxHandles->api_connected = 1;
  return 1;
}

void skypiax_clean_disp(void *data)
{

  int *dispptr;
  int disp;
  private_t *tech_pvt = NULL;

  dispptr = data;
  disp = *dispptr;

  if (disp) {
    DEBUGA_SKYPE("to be destroyed disp %d\n", SKYPIAX_P_LOG, disp);
    close(disp);
    DEBUGA_SKYPE("destroyed disp\n", SKYPIAX_P_LOG);
  } else {
    DEBUGA_SKYPE("NOT destroyed disp\n", SKYPIAX_P_LOG);
  }
  DEBUGA_SKYPE("OUT destroyed disp\n", SKYPIAX_P_LOG);
  skypiax_sleep(1000);
}

void *skypiax_do_skypeapi_thread_func(void *obj)
{

  private_t *tech_pvt = obj;
  struct SkypiaxHandles *SkypiaxHandles;
  char buf[512];
  Display *disp = NULL;
  Window root = -1;
  Window win = -1;

  if (!strlen(tech_pvt->X11_display))
    strcpy(tech_pvt->X11_display, getenv("DISPLAY"));

  if (!tech_pvt->tcp_srv_port)
    tech_pvt->tcp_srv_port = 10160;

  if (!tech_pvt->tcp_cli_port)
    tech_pvt->tcp_cli_port = 10161;

  if (pipe(tech_pvt->SkypiaxHandles.fdesc)) {
    fcntl(tech_pvt->SkypiaxHandles.fdesc[0], F_SETFL, O_NONBLOCK);
    fcntl(tech_pvt->SkypiaxHandles.fdesc[1], F_SETFL, O_NONBLOCK);
  }
  SkypiaxHandles = &tech_pvt->SkypiaxHandles;
  disp = XOpenDisplay(tech_pvt->X11_display);
  if (!disp) {
    ERRORA("Cannot open X Display '%s', exiting skype thread\n", SKYPIAX_P_LOG,
           tech_pvt->X11_display);
    running = 0;
    return NULL;
  } else {
    DEBUGA_SKYPE("X Display '%s' opened\n", SKYPIAX_P_LOG, tech_pvt->X11_display);
  }

  int xfd;
  xfd = XConnectionNumber(disp);
  fcntl(xfd, F_SETFD, FD_CLOEXEC);

  SkypiaxHandles->disp = disp;

  if (skypiax_present(SkypiaxHandles)) {
    root = DefaultRootWindow(disp);
    win =
      XCreateSimpleWindow(disp, root, 0, 0, 1, 1, 0,
                          BlackPixel(disp, DefaultScreen(disp)), BlackPixel(disp,
                                                                            DefaultScreen
                                                                            (disp)));

    SkypiaxHandles->win = win;

    snprintf(buf, 512, "NAME skypiax");

    if (!skypiax_send_message(SkypiaxHandles, buf)) {
      ERRORA
        ("Sending message failed - probably Skype crashed. Please run/restart Skype manually and launch Skypiax again\n",
         SKYPIAX_P_LOG);
      running = 0;
      return NULL;
    }

    snprintf(buf, 512, "PROTOCOL 7");
    if (!skypiax_send_message(SkypiaxHandles, buf)) {
      ERRORA
        ("Sending message failed - probably Skype crashed. Please run/restart Skype manually and launch Skypiax again\n",
         SKYPIAX_P_LOG);
      running = 0;
      return NULL;
    }

    /* perform an events loop */
    XEvent an_event;
    char buf[21];               /*  can't be longer */
    char buffer[17000];
    char *b;
    int i;

    b = buffer;

    while (1) {
      XNextEvent(disp, &an_event);
      if (!running)
        break;
      switch (an_event.type) {
      case ClientMessage:

        if (an_event.xclient.format != 8)
          break;

        for (i = 0; i < 20 && an_event.xclient.data.b[i] != '\0'; ++i)
          buf[i] = an_event.xclient.data.b[i];

        buf[i] = '\0';

        strcat(buffer, buf);

        if (i < 20) {           /* last fragment */
          unsigned int howmany;

          howmany = strlen(b) + 1;

          howmany = write(SkypiaxHandles->fdesc[1], b, howmany);
          memset(buffer, '\0', 17000);
        }

        break;
      default:
        break;
      }
    }
  } else {
    ERRORA
      ("Skype is not running, maybe crashed. Please run/restart Skype and relaunch Skypiax\n",
       SKYPIAX_P_LOG);
    running = 0;
    return NULL;
  }
  running = 0;
  return NULL;

}
#endif // WIN32
