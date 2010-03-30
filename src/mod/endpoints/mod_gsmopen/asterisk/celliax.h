//indent -gnu -ts4 -br -brs -cdw -lp -ce -nbfda -npcs -nprs -npsl -nbbo -saf -sai -saw -cs -bbo -nhnl -nut -sob -l90 
#undef GIOVA48
#define CELLIAX_ALSA
#ifndef _CELLIAX_H_
#define _CELLIAX_H_

#ifndef CELLIAX_SVN_VERSION
#define CELLIAX_SVN_VERSION "????NO_REVISION???"
#endif

#include <asterisk/version.h>   /* needed here for conditional compilation on version.h */
  /* the following #defs are for LINUX */
#ifndef __CYGWIN__
#ifndef ASTERISK_VERSION_1_6_0
#ifndef ASTERISK_VERSION_1_4
#ifndef ASTERISK_VERSION_1_2
#define ASTERISK_VERSION_1_4
#if(ASTERISK_VERSION_NUM == 999999)
#undef ASTERISK_VERSION_1_4
#elif(ASTERISK_VERSION_NUM < 10400)
#undef ASTERISK_VERSION_1_4
#endif /* ASTERISK_VERSION_NUM == 999999 || ASTERISK_VERSION_NUM < 10400 */
#endif /* ASTERISK_VERSION_1_2 */
#endif /* ASTERISK_VERSION_1_4 */
#endif /* ASTERISK_VERSION_1_6_0 */
#ifdef ASTERISK_VERSION_1_2
#undef ASTERISK_VERSION_1_4
#endif /* ASTERISK_VERSION_1_2 */
#ifdef ASTERISK_VERSION_1_6_0
#define ASTERISK_VERSION_1_4
#endif /* ASTERISK_VERSION_1_6_0 */
#define CELLIAX_DIR
#undef CELLIAX_LIBCSV
#endif /* NOT __CYGWIN__ */
  /* the following #defs are for WINDOWS */
#ifdef __CYGWIN__
#undef ASTERISK_VERSION_1_4
#undef ASTERISK_VERSION_1_6_0
#define CELLIAX_DIR
#undef CELLIAX_LIBCSV
#endif /* __CYGWIN__ */

/* CELLIAX_CVM */
#undef CELLIAX_CVM
/* CELLIAX_CVM */

#undef CELLIAX_FBUS2
#define CELLIAX_DIR
#define CELLIAX_LIBCSV

/* INCLUDES */
#ifdef ASTERISK_VERSION_1_6_0
#include <asterisk.h>           /* some asterisk-devel package do not contains asterisk.h, but seems that is needed for the 1.6 series, at least from trunk */
#endif /* ASTERISK_VERSION_1_6_0 */
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <ctype.h>
#ifndef ASTERISK_VERSION_1_4
#include <stdlib.h>
#include <stdio.h>
#endif /* ASTERISK_VERSION_1_4 */
#ifndef CELLIAX_ALSA
#include "pablio.h"
#endif /* CELLIAX_ALSA */
//#include <asterisk/frame.h>
#include <asterisk/channel.h>
#include <asterisk/module.h>
#include <asterisk/pbx.h>
#include <asterisk/causes.h>
#include <asterisk/cli.h>
#include <asterisk/options.h>
#include <asterisk/config.h>
#include <asterisk/endian.h>
#include <asterisk/dsp.h>
#include <asterisk/lock.h>
#include <asterisk/devicestate.h>
#include <asterisk/file.h>
#include <asterisk/say.h>
#include <asterisk/manager.h>
#ifdef ASTERISK_VERSION_1_6_0
#include <asterisk/astobj2.h>
#include <asterisk/paths.h>
#endif /* ASTERISK_VERSION_1_6_0 */
#ifdef ASTERISK_VERSION_1_4
#include <asterisk/stringfields.h>
#include <asterisk/abstract_jb.h>
#include <asterisk/logger.h>
#include <asterisk/utils.h>
#endif /* ASTERISK_VERSION_1_4 */
#ifdef ASTERISK_VERSION_1_2
#include <asterisk/utils.h>
#include <asterisk/logger.h>
#endif /* ASTERISK_VERSION_1_2 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "celliax_spandsp.h"
#ifdef CELLIAX_LIBCSV
#include "celliax_libcsv.h"
#endif /* CELLIAX_LIBCSV */
#ifdef __CYGWIN__
#include <windows.h>
#endif /* __CYGWIN__ */
#ifndef AST_DIGIT_ANYDIG
#define AST_DIGIT_ANYDIG "0123456789*#"
#else
#warning Please review Celliax AST_DIGIT_ANYDIG
#endif
#ifndef _ASTERISK_H
#define AST_CONFIG_MAX_PATH 255 /* defined in asterisk.h, but some asterisk-devel package do not contains asterisk.h */
extern char ast_config_AST_CONFIG_DIR[AST_CONFIG_MAX_PATH];
int ast_register_atexit(void (*func) (void));   /* in asterisk.h, but some asterisk-devel package do not contains asterisk.h */
void ast_unregister_atexit(void (*func) (void));    /* in asterisk.h, but some asterisk-devel package do not contains asterisk.h */
#endif
#ifdef CELLIAX_ALSA
#define ALSA_PCM_NEW_HW_PARAMS_API
#define ALSA_PCM_NEW_SW_PARAMS_API
#include <alsa/asoundlib.h>
#endif /* CELLIAX_ALSA */

/* DEFINITIONS */
/* LUIGI RIZZO's magic */
/* boost support. BOOST_SCALE * 10 ^(BOOST_MAX/20) must
 * be representable in 16 bits to avoid overflows.
 */
#define	BOOST_SCALE	(1<<9)
#define	BOOST_MAX	40          /* slightly less than 7 bits */
/* call flow from the device */
#define 	FBUS2_OUTGOING_ACK   999
#define 	FBUS2_SECURITY_COMMAND_ON   444
#define 	CALLFLOW_CALL_IDLE  AST_STATE_DOWN
#define 	CALLFLOW_INCOMING_RING  AST_STATE_RING
#define 	CALLFLOW_CALL_DIALING   AST_STATE_DIALING
#define 	CALLFLOW_CALL_LINEBUSY   AST_STATE_BUSY
#define 	CALLFLOW_CALL_ACTIVE   300
#define 	CALLFLOW_INCOMING_HANGUP   100
#define 	CALLFLOW_CALL_RELEASED   101
#define 	CALLFLOW_CALL_NOCARRIER   102
#define 	CALLFLOW_CALL_INFLUX   103
#define 	CALLFLOW_CALL_INCOMING   104
#define 	CALLFLOW_CALL_FAILED   105
#define 	CALLFLOW_CALL_NOSERVICE   106
#define 	CALLFLOW_CALL_OUTGOINGRESTRICTED   107
#define 	CALLFLOW_CALL_SECURITYFAIL   108
#define 	CALLFLOW_CALL_NOANSWER   109
#define 	CALLFLOW_CALL_HANGUP_REQUESTED   110
  //fixme CALLFLOW_GOT_IMEI to be removed
#define 	CALLFLOW_GOT_IMEI   1009
  //fixme CALLFLOW_INCOMING_CALLID to be removed
#define 	CALLFLOW_INCOMING_CALLID   1019
#define 	AT_OK   0
#define 	AT_ERROR   1
/* FBUS2 (old Nokia phones) undocumented proprietary protocol */
#define 	FBUS2_ACK_BYTE   0x7f
#define 	FBUS2_CALL_CALLID   0x05
#define 	FBUS2_CALL_HANGUP   0x04
#define 	FBUS2_CALL_STATUS_OFF   0x01
#define 	FBUS2_CALL_STATUS_ON   0x02
#define 	FBUS2_COMMAND_BYTE_1   0x00
#define 	FBUS2_COMMAND_BYTE_2   0x01
#define 	FBUS2_DEVICE_PC   0x0c
#define 	FBUS2_DEVICE_PHONE   0x00
#define 	FBUS2_IRDA_FRAME_ID   0x1c
#define 	FBUS2_IS_LAST_FRAME   0x01
#define 	FBUS2_MAX_TRANSMIT_LENGTH   120
#define 	FBUS2_NETWORK_STATUS_REGISTERED   0x71
#define 	FBUS2_SECURIY_CALL_COMMAND_ANSWER   0x02
#define 	FBUS2_SECURIY_CALL_COMMAND_CALL   0x01
#define 	FBUS2_SECURIY_CALL_COMMAND_RELEASE   0x03
#define 	FBUS2_SECURIY_CALL_COMMANDS   0x7c
#define 	FBUS2_SECURIY_EXTENDED_COMMAND_ON   0x01
#define 	FBUS2_SECURIY_EXTENDED_COMMANDS   0x64
#define 	FBUS2_SECURIY_IMEI_COMMAND_GET   0x00
#define 	FBUS2_SECURIY_IMEI_COMMANDS   0x66
#define 	FBUS2_SEQNUM_MAX   0x47
#define 	FBUS2_SEQNUM_MIN   0x40
#define 	FBUS2_SERIAL_FRAME_ID   0x1e
#define 	FBUS2_SMS_INCOMING   0x10
#define 	FBUS2_TYPE_CALL   0x01
#define 	FBUS2_TYPE_CALL_DIVERT   0x06
#define 	FBUS2_TYPE_CALL_STATUS   0x0d
#define 	FBUS2_TYPE_NETWORK_STATUS   0x0a
#define 	FBUS2_TYPE_SECURITY   0x40
#define 	FBUS2_TYPE_SMS   0x02
#define 	FBUS2_TYPE_MODEL_ASK   0xd1
#define   FBUS2_TYPE_MODEL_ANSWER   0xd2
//#define   FBUS2_TYPE_MODEL_ANSWER   0xffffffd2
#ifdef CELLIAX_CVM
#define   CVM_BUSMAIL_SEQNUM_MAX   0x7
#endif /* CELLIAX_CVM */
/* debugging bitmask */
#define DEBUG_SOUND 1
#define DEBUG_SERIAL 2
#define DEBUG_SKYPE 4
#define DEBUG_AT 8
#define DEBUG_FBUS2 16
#define DEBUG_CALL 32
#define DEBUG_LOCKS 64
#define DEBUG_PBX 128
#define DEBUG_MONITORLOCKS 256
#ifndef CELLIAX_CVM
#define DEBUG_ALL DEBUG_SOUND|DEBUG_SERIAL|DEBUG_SKYPE|DEBUG_AT|DEBUG_FBUS2|DEBUG_CALL|DEBUG_PBX|DEBUG_LOCKS|DEBUG_MONITORLOCKS
#else
#define DEBUG_CVM 512
#define DEBUG_ALL DEBUG_SOUND|DEBUG_SERIAL|DEBUG_SKYPE|DEBUG_AT|DEBUG_FBUS2|DEBUG_CALL|DEBUG_PBX|DEBUG_LOCKS|DEBUG_MONITORLOCKS|DEBUG_CVM
#endif /* CELLIAX_CVM */
/* wrappers for ast_log */
#define DEBUGA_SOUND(...)  if (celliax_debug & DEBUG_SOUND) ast_log(LOG_DEBUG, 		"rev "CELLIAX_SVN_VERSION "[%p|%-7lx][DEBUG_SOUND  %-5d][%-10s][%2d,%2d,%2d] " __VA_ARGS__ );
#define DEBUGA_SERIAL(...)  if (celliax_debug & DEBUG_SERIAL) ast_log(LOG_DEBUG, 	"rev "CELLIAX_SVN_VERSION "[%p|%-7lx][DEBUG_SERIAL %-5d][%-10s][%2d,%2d,%2d] " __VA_ARGS__ );
#define DEBUGA_SKYPE(...)  if (celliax_debug & DEBUG_SKYPE) ast_log(LOG_DEBUG, 		"rev "CELLIAX_SVN_VERSION "[%p|%-7lx][DEBUG_SKYPE  %-5d][%-10s][%2d,%2d,%2d] " __VA_ARGS__ );
#define DEBUGA_AT(...)  if (celliax_debug & DEBUG_AT) ast_log(LOG_DEBUG, 		"rev "CELLIAX_SVN_VERSION "[%p|%-7lx][DEBUG_AT     %-5d][%-10s][%2d,%2d,%2d] " __VA_ARGS__ );
#define DEBUGA_FBUS2(...)  if (celliax_debug & DEBUG_FBUS2) ast_log(LOG_DEBUG, 		"rev "CELLIAX_SVN_VERSION "[%p|%-7lx][DEBUG_FBUS2  %-5d][%-10s][%2d,%2d,%2d] " __VA_ARGS__ );
#define DEBUGA_CALL(...)  if (celliax_debug & DEBUG_CALL) ast_log(LOG_DEBUG, 		"rev "CELLIAX_SVN_VERSION "[%p|%-7lx][DEBUG_CALL   %-5d][%-10s][%2d,%2d,%2d] " __VA_ARGS__ );
#define DEBUGA_PBX(...)  if (celliax_debug & DEBUG_PBX) ast_log(LOG_DEBUG, 		"rev "CELLIAX_SVN_VERSION "[%p|%-7lx][DEBUG_PBX    %-5d][%-10s][%2d,%2d,%2d] " __VA_ARGS__ );
#ifdef CELLIAX_CVM
#define DEBUGA_CVM(...)  if (celliax_debug & DEBUG_CVM) ast_log(LOG_DEBUG,  "rev "CELLIAX_SVN_VERSION "[%p|%-7lx][DEBUG_CVM    %-5d][%-10s][%2d,%2d,%2d] " __VA_ARGS__ );
#endif /* CELLIAX_CVM */
#define ERRORA(...)  ast_log(LOG_ERROR, 						"rev "CELLIAX_SVN_VERSION "[%p|%-7lx][ERROR        %-5d][%-10s][%2d,%2d,%2d] " __VA_ARGS__ );
#define NOTICA(...)  ast_log(LOG_NOTICE, 						"rev "CELLIAX_SVN_VERSION "[%p|%-7lx][NOTICE      %-5d][%-10s][%2d,%2d,%2d] " __VA_ARGS__ );
#define WARNINGA(...)  ast_log(LOG_WARNING, 						"rev "CELLIAX_SVN_VERSION "[%p|%-7lx][WARNING    %-5d][%-10s][%2d,%2d,%2d] " __VA_ARGS__ );
/* macros for logging */
#define CELLIAX_P_LOG p ? p->owner : NULL, (unsigned long)pthread_self(), __LINE__, p ? p->name ? p->name : "none" : "none", p ? p->owner ? p->owner->_state : -1 : -1,  p ? p->interface_state : -1, p ? p->phone_callflow : -1
#define CELLIAX_TMP_LOG tmp ? tmp->owner : NULL, (unsigned long)pthread_self(), __LINE__, tmp ? tmp->name ? tmp->name : "none" : "none", tmp ? tmp->owner ? tmp->owner->_state : -1 : -1,  tmp ? tmp->interface_state : -1, tmp ? tmp->phone_callflow : -1
/* logging wrappers for ast_mutex_lock and ast_mutex_unlock */
#define LOKKA(x)  if (celliax_debug & DEBUG_LOCKS) ast_log(LOG_DEBUG, "rev "CELLIAX_SVN_VERSION "[%p|%-7lx][DEBUG_LOCKS  %-5d][%-10s][%2d,%2d,%2d] going to lock %p (%s)\n", CELLIAX_P_LOG, x, x == &celliax_monlock ? "MONLOCK" : x == &celliax_iflock ? "IFLOCK" : x == &celliax_usecnt_lock ? "USECNT_LOCK" : x == &p->controldev_lock ? "CONTROLDEV_LOCK" :  x == &p->fbus2_outgoing_list_lock ? "FBUS2_OUTGOING_LIST_LOCK" : "?????"); if (ast_mutex_lock(x)) ast_log(LOG_ERROR, "ast_mutex_lock failed, BAD\n");   if (celliax_debug & DEBUG_LOCKS) ast_log(LOG_DEBUG, "rev "CELLIAX_SVN_VERSION "[%p|%-7lx][DEBUG_LOCKS  %-5d][%-10s][%2d,%2d,%2d] locked %p (%s)\n", CELLIAX_P_LOG, x, x == &celliax_monlock ? "MONLOCK" : x == &celliax_iflock ? "IFLOCK" : x == &celliax_usecnt_lock ? "USECNT_LOCK" : x == &p->controldev_lock ? "CONTROLDEV_LOCK" : x == &p->fbus2_outgoing_list_lock ? "FBUS2_OUTGOING_LIST_LOCK" : "?????");
#define UNLOCKA(x)  if (celliax_debug & DEBUG_LOCKS)  ast_log(LOG_DEBUG, "rev "CELLIAX_SVN_VERSION "[%p|%-7lx][DEBUG_LOCKS  %-5d][%-10s][%2d,%2d,%2d] going to unlock %p (%s)\n",  CELLIAX_P_LOG, x, x == &celliax_monlock ? "MONLOCK" : x == &celliax_iflock ? "IFLOCK" : x == &celliax_usecnt_lock ? "USECNT_LOCK" : x == &p->controldev_lock ? "CONTROLDEV_LOCK" : x == &p->fbus2_outgoing_list_lock ? "FBUS2_OUTGOING_LIST_LOCK" : "?????"); if (ast_mutex_unlock(x)) ast_log(LOG_ERROR, "ast_mutex_lock failed, BAD\n");   if (celliax_debug & DEBUG_LOCKS) ast_log(LOG_DEBUG, "rev "CELLIAX_SVN_VERSION "[%p|%-7lx][DEBUG_LOCKS  %-5d][%-10s][%2d,%2d,%2d] unlocked %p (%s)\n", CELLIAX_P_LOG, x, x == &celliax_monlock ? "MONLOCK" : x == &celliax_iflock ? "IFLOCK" : x == &celliax_usecnt_lock ? "USECNT_LOCK" : x == &p->controldev_lock ? "CONTROLDEV_LOCK" : x == &p->fbus2_outgoing_list_lock ? "FBUS2_OUTGOING_LIST_LOCK" : "?????");
#define CVM_LOKKA(x)  if (celliax_debug & DEBUG_LOCKS) ast_log(LOG_DEBUG, "rev "CELLIAX_SVN_VERSION "[%p|%-7lx][DEBUG_LOCKS  %-5d][%-10s][%2d,%2d,%2d] going to lock %p (%s)\n", CELLIAX_P_LOG, x, x == &celliax_monlock ? "MONLOCK" : x == &celliax_iflock ? "IFLOCK" : x == &celliax_usecnt_lock ? "USECNT_LOCK" : x == &p->controldev_lock ? "CONTROLDEV_LOCK" :  x == &p->cvm_busmail_outgoing_list_lock ? "FBUS2_OUTGOING_LIST_LOCK" : "?????"); if (ast_mutex_lock(x)) ast_log(LOG_ERROR, "ast_mutex_lock failed, BAD\n");   if (celliax_debug & DEBUG_LOCKS) ast_log(LOG_DEBUG, "rev "CELLIAX_SVN_VERSION "[%p|%-7lx][DEBUG_LOCKS  %-5d][%-10s][%2d,%2d,%2d] locked %p (%s)\n", CELLIAX_P_LOG, x, x == &celliax_monlock ? "MONLOCK" : x == &celliax_iflock ? "IFLOCK" : x == &celliax_usecnt_lock ? "USECNT_LOCK" : x == &p->controldev_lock ? "CONTROLDEV_LOCK" : x == &p->cvm_busmail_outgoing_list_lock ? "CVM_BUSMAIL_OUTGOING_LIST_LOCK" : "?????");
#define CVM_UNLOCKA(x)  if (celliax_debug & DEBUG_LOCKS)  ast_log(LOG_DEBUG, "rev "CELLIAX_SVN_VERSION "[%p|%-7lx][DEBUG_LOCKS  %-5d][%-10s][%2d,%2d,%2d] going to unlock %p (%s)\n",  CELLIAX_P_LOG, x, x == &celliax_monlock ? "MONLOCK" : x == &celliax_iflock ? "IFLOCK" : x == &celliax_usecnt_lock ? "USECNT_LOCK" : x == &p->controldev_lock ? "CONTROLDEV_LOCK" : x == &p->cvm_busmail_outgoing_list_lock ? "FBUS2_OUTGOING_LIST_LOCK" : "?????"); if (ast_mutex_unlock(x)) ast_log(LOG_ERROR, "ast_mutex_lock failed, BAD\n");   if (celliax_debug & DEBUG_LOCKS) ast_log(LOG_DEBUG, "rev "CELLIAX_SVN_VERSION "[%p|%-7lx][DEBUG_LOCKS  %-5d][%-10s][%2d,%2d,%2d] unlocked %p (%s)\n", CELLIAX_P_LOG, x, x == &celliax_monlock ? "MONLOCK" : x == &celliax_iflock ? "IFLOCK" : x == &celliax_usecnt_lock ? "USECNT_LOCK" : x == &p->controldev_lock ? "CONTROLDEV_LOCK" : x == &p->cvm_busmail_outgoing_list_lock ? "CVM_BUSMAIL_OUTGOING_LIST_LOCK" : "?????");
#define PUSHA_UNLOCKA(x)    pthread_cleanup_push(celliax_unlocka_log, (void *) x);
#define POPPA_UNLOCKA(x)    pthread_cleanup_pop(0);
#define MONITORLOKKA(x)  if (celliax_debug & DEBUG_MONITORLOCKS) ast_log(LOG_DEBUG, "rev "CELLIAX_SVN_VERSION "[%p|%-7lx][DEBUG_MONITORLOCKS  %-5d][%-10s][%2d,%2d,%2d] going to lock %p (%s)\n", CELLIAX_P_LOG, x, x == &celliax_monlock ? "MONLOCK" : x == &celliax_iflock ? "IFLOCK" : x == &celliax_usecnt_lock ? "USECNT_LOCK" : x == &p->controldev_lock ? "CONTROLDEV_LOCK" :  x == &p->fbus2_outgoing_list_lock ? "FBUS2_OUTGOING_LIST_LOCK" : "?????"); if (ast_mutex_lock(x)) ast_log(LOG_ERROR, "ast_mutex_lock failed, BAD\n");   if (celliax_debug & DEBUG_MONITORLOCKS) ast_log(LOG_DEBUG, "rev "CELLIAX_SVN_VERSION "[%p|%-7lx][DEBUG_MONITORLOCKS  %-5d][%-10s][%2d,%2d,%2d] locked %p (%s)\n", CELLIAX_P_LOG, x, x == &celliax_monlock ? "MONLOCK" : x == &celliax_iflock ? "IFLOCK" : x == &celliax_usecnt_lock ? "USECNT_LOCK" : x == &p->controldev_lock ? "CONTROLDEV_LOCK" : x == &p->fbus2_outgoing_list_lock ? "FBUS2_OUTGOING_LIST_LOCK" : "?????");
#define MONITORUNLOCKA(x)  if (celliax_debug & DEBUG_MONITORLOCKS)  ast_log(LOG_DEBUG, "rev "CELLIAX_SVN_VERSION "[%p|%-7lx][DEBUG_MONITORLOCKS  %-5d][%-10s][%2d,%2d,%2d] going to unlock %p (%s)\n",  CELLIAX_P_LOG, x, x == &celliax_monlock ? "MONLOCK" : x == &celliax_iflock ? "IFLOCK" : x == &celliax_usecnt_lock ? "USECNT_LOCK" : x == &p->controldev_lock ? "CONTROLDEV_LOCK" : x == &p->fbus2_outgoing_list_lock ? "FBUS2_OUTGOING_LIST_LOCK" : "?????"); if (ast_mutex_unlock(x)) ast_log(LOG_ERROR, "ast_mutex_lock failed, BAD\n");   if (celliax_debug & DEBUG_MONITORLOCKS) ast_log(LOG_DEBUG, "rev "CELLIAX_SVN_VERSION "[%p|%-7lx][DEBUG_MONITORLOCKS  %-5d][%-10s][%2d,%2d,%2d] unlocked %p (%s)\n", CELLIAX_P_LOG, x, x == &celliax_monlock ? "MONLOCK" : x == &celliax_iflock ? "IFLOCK" : x == &celliax_usecnt_lock ? "USECNT_LOCK" : x == &p->controldev_lock ? "CONTROLDEV_LOCK" : x == &p->fbus2_outgoing_list_lock ? "FBUS2_OUTGOING_LIST_LOCK" : "?????");
/* macros used for config file parsing */
#define 	M_BOOL(tag, dst)   M_F(tag, (dst) = ast_true(__val) )
#define 	M_END(x)   x;
#define 	M_F(tag, f)   if (!strcasecmp((__s), tag)) { f; } else
#ifdef ASTERISK_VERSION_1_6_0
#define 	M_START(var, val)   const char *__s = var; const char *__val = val;
#else
#define 	M_START(var, val)   char *__s = var; char *__val = val;
#endif /* ASTERISK_VERSION_1_6_0 */
#define 	M_STR(tag, dst)   M_F(tag, ast_copy_string(dst, __val, sizeof(dst)))
#define 	M_UINT(tag, dst)   M_F(tag, (dst) = strtoul(__val, NULL, 0) )
/* which protocol we use to control the phone through serial device */
#ifdef CELLIAX_CVM
#define   PROTOCOL_CVM_BUSMAIL   5
#endif /* CELLIAX_CVM */
#define 	PROTOCOL_ALSA_VOICEMODEM   4
#define 	PROTOCOL_AT   2
#define 	PROTOCOL_FBUS2   1
#define 	PROTOCOL_NO_SERIAL   3
#ifndef GIOVA48
#define 	CELLIAX_FRAME_SIZE   160
#else //GIOVA48
#define 	CELLIAX_FRAME_SIZE   960
#endif //GIOVA48
#define		AT_BUFSIZ 8192
#define AT_MESG_MAX_LENGTH 2048 /* much more than 10 SMSs */
#define AT_MESG_MAX_LINES 256   /* 256 lines, so it can contains the results of AT+CLAC, that gives all the AT commands the phone supports */

#ifdef CELLIAX_CVM
/* MAIL PRIMITIVES */
/* CVM -> CELLIAX */
#define API_PP_LOCKED_IND 0x8558    //PP locked with FP
#define API_PP_UNLOCKED_IND 0x8559  //PP out of service, unlocked from FP

#define API_PP_SETUP_IND 0x8574 //Incoming call to PP

#define API_PP_SETUP_IND_CALL_TYPE_OFFSET 0x0
#define API_PP_SETUP_IND_RING_TYPE_OFFSET 0x1

#define API_PP_SETUP_IND_CALL_EXT 0x0
#define API_PP_SETUP_IND_CALL_INT 0x1

#define API_PP_SETUP_IND_RING_INT_CALL 0x40
#define API_PP_SETUP_IND_RING_PAGE_ALL 0x46

#define API_PP_SETUP_ACK_IND 0x857F //internal connection established with FPs, waiting for handsetnumber

#define API_PP_CONNECT_IND 0x8576   //air-link established with FPs
#define API_PP_CONNECT_CFM 0x8578   //PP answered incoming call

#define API_PP_ALERT_IND 0x8581
#define API_PP_ALERT_ON_IND 0x857D
#define API_PP_ALERT_OFF_IND 0x857E

#define API_PP_SIGNAL_ON_IND 0x2F9C
#define API_PP_SIGNAL_OFF_IND 0x2F9D

#define API_PP_RELEASE_IND 0x857B
#define API_PP_RELEASE_CFM 0x857A
#define API_PP_REJECT_IND 0x8564

#define API_PP_ACCESS_RIGHTS_CFM 0x8568
#define API_PP_ACCESS_RIGHTS_REJ 0x8569

#define API_PP_DELETE_SUBS_CFM 0x8561
#define API_PP_REMOTE_DELETE_SUBS_CFM 0x2F9F

#define API_PP_CLIP_IND 0x2F93
#define API_PP_SW_STATUS_IND 0x2FC2
#define API_PP_MESSAGE_WAITING_IND 0x2FA1

#define CVM_PP_PLUG_STATUS_IND 0x2F4F
#define CVM_PP_LINE_STATUS_IND 0x2F53
#define CVM_PP_ON_KEY_IND 0x2F64

#define API_PP_READ_RSSI_CFM 0x2FC7
#define API_PP_ALERT_BROADCAST_IND 0x2FA3

/* CELLIAX -> CVM */
#define API_PP_LOCK_REQ 0x8554  //select FP to lock once
#define API_PP_SETUP_REQ 0x8571 //setup air-link PP<->FP

#define API_PP_KEYPAD_REQ 0x858A    //send string for dialing

#define API_PP_CONNECT_REQ 0x8577   //answer incoming call

#define API_PP_ALERT_REQ 0x2F8D //inform FP that alering is started

#define API_PP_RELEASE_REQ 0x8579   //release connection
#define API_PP_RELEASE_RES 0x857C   //confirm FP initiated release of connection
#define API_PP_REJECT_REQ 0x8565    //PP reject incoming call

#define API_PP_ACCESS_RIGHTS_REQ 0x8566 //init registration to FP
#define API_PP_DELETE_SUBS_REQ 0x8560   //deregister from FP (locally only in PP)
#define API_PP_REMOTE_DELETE_SUBS_REQ 0x2F9E    //remotly deregister from FP

#define API_PP_STOP_PROTOCOL_REQ 0x2FC4 //stop protocol from running  (even registration)

#define API_PP_READ_RSSI_REQ 0x2FC6 //RSSI readout request
#define API_PP_READ_RSSI_CFM 0x2FC7 //RSSI readout result

#define CVM_PP_AUDIO_OPEN_REQ 0x2F0E    //Enable audio
#define CVM_PP_AUDIO_CLOSE_REQ 0x2F0F   //Disable audio

#define CVM_PP_AUDIO_SET_VOLUME_REQ 0x2F1D  //set volume

#define CVM_PP_AUDIO_UNMUTE_MIC_REQ 0x2F1A  //unmute mic
#define CVM_PP_AUDIO_MUTE_MIC_REQ 0x2F19    //mute mic

#define CVM_PP_AUDIO_HS_PLUG_IND 0x2F1C //mute mic

#define CVM_PP_AUDIO_OPEN_ADPCM_OFF_REQ 0x2F68  //open audio even before making connection

/* END OF MAIL PRIMITIVES */

enum CvmLockState {
  CVM_UNKNOWN_LOCK_STATE = 0,
  CVM_UNLOCKED_TO_FP,
  CVM_LOCKED_TO_FP
};

enum CvmRegisterState {
  CVM_UNKNOWN_REGISTER_STATE = 0,
  CVM_UNREGISTERED_TO_FP,
  CVM_REGISTERED_TO_FP
};

#define BUSMAIL_MAIL_MAX_PARAMS_LENGTH 128
#define BUSMAIL_MAX_FRAME_LENGTH (BUSMAIL_MAIL_MAX_PARAMS_LENGTH + 9)

#define BUSMAIL_OFFSET_SOF 0
#define BUSMAIL_OFFSET_LEN_MSB 1
#define BUSMAIL_OFFSET_LEN_LSB 2
#define BUSMAIL_OFFSET_HEADER 3
#define BUSMAIL_OFFSET_MAIL BUSMAIL_OFFSET_MAIL_PROGRAM_ID
#define BUSMAIL_OFFSET_MAIL_PROGRAM_ID 4
#define BUSMAIL_OFFSET_MAIL_TASK_ID 5
#define BUSMAIL_OFFSET_MAIL_PRIMITIVE_MSB 7
#define BUSMAIL_OFFSET_MAIL_PRIMITIVE_LSB 6
#define BUSMAIL_OFFSET_MAIL_PARAMS 8

#define BUSMAIL_MAIL_PRIMITIVE_MSB 1
#define BUSMAIL_MAIL_PRIMITIVE_LSB 0
#define BUSMAIL_LEN_MSB 0
#define BUSMAIL_LEN_LSB 1

#define BUSMAIL_SOF 0x10

#define BUSMAIL_MAIL_PROGRAM_ID 0x0
#define BUSMAIL_MAIL_USERTASK_TASK_ID 0x0f
#define BUSMAIL_MAIL_TBHANDLE_TASK_ID 0x0
#define BUSMAIL_MAIL_TASK_ID BUSMAIL_MAIL_USERTASK_TASK_ID

#define BUSMAIL_HEADER_IC_BIT_MASK 0x80
#define BUSMAIL_HEADER_SU_BIT_MASK 0x40
#define BUSMAIL_HEADER_PF_BIT_MASK 0x08
#define BUSMAIL_HEADER_TXSEQ_MASK 0x70
#define BUSMAIL_HEADER_RXSEQ_MASK 0x07
#define BUSMAIL_HEADER_SUID_MASK 0x30
#define BUSMAIL_HEADER_UNID_MASK BUSMAIL_HEADER_SUID_MASK

#define BUSMAIL_HEADER_INFO_FRAME 0x0
#define BUSMAIL_HEADER_CTRL_FRAME 0x80
#define BUSMAIL_HEADER_CTRL_SU_FRAME 0x0
#define BUSMAIL_HEADER_CTRL_UN_FRAME 0x40

#define BUSMAIL_HEADER_UNID_SABM 0x0
#define BUSMAIL_HEADER_SUID_RR 0x0
#define BUSMAIL_HEADER_SUID_REJ 0x10
#define BUSMAIL_HEADER_SUID_RNR 0x20

#define BUSMAIL_HEADER_SABM_MASK (BUSMAIL_HEADER_IC_BIT_MASK | BUSMAIL_HEADER_SU_BIT_MASK | BUSMAIL_HEADER_UNID_MASK)
#define BUSMAIL_HEADER_SABM (BUSMAIL_HEADER_CTRL_FRAME | BUSMAIL_HEADER_CTRL_UN_FRAME | BUSMAIL_HEADER_UNID_SABM)

#define BUSMAIL_HEADER_REJ_MASK (BUSMAIL_HEADER_IC_BIT_MASK | BUSMAIL_HEADER_SU_BIT_MASK | BUSMAIL_HEADER_SUID_MASK)
#define BUSMAIL_HEADER_REJ (BUSMAIL_HEADER_CTRL_FRAME | BUSMAIL_HEADER_CTRL_SU_FRAME | BUSMAIL_HEADER_SUID_REJ)

#define BUSMAIL_HEADER_SU_FRAME_MASK (BUSMAIL_HEADER_IC_BIT_MASK | BUSMAIL_HEADER_SU_BIT_MASK)
#define BUSMAIL_HEADER_SU_FRAME (BUSMAIL_HEADER_CTRL_FRAME | BUSMAIL_HEADER_CTRL_SU_FRAME)

/*! 
 * \brief structure holding raw data to be send throught serial
 */
struct cvm_busmail_msg {
  int valid;
  unsigned char busmail_msg_buffer[BUSMAIL_MAX_FRAME_LENGTH];
  unsigned int busmail_msg_len;

  unsigned int tv_sec;
  unsigned int tv_usec;

  unsigned char txseqno;
  int acknowledged;
  int how_many_sent;
  int sent;

  struct cvm_busmail_msg *next;
  struct cvm_busmail_msg *previous;
};

/*! 
 * \brief structure holding busmail frame, for internal use
 */
struct cvm_busmail_frame {
  unsigned char busmail_sof;
  unsigned char busmail_len[2];
  unsigned char busmail_header;
  unsigned char busmail_mail_program_id;
  unsigned char busmail_mail_task_id;
  unsigned char busmail_mail_primitive[2];
  unsigned char busmail_mail_params_buffer[BUSMAIL_MAIL_MAX_PARAMS_LENGTH];
  unsigned int busmail_mail_params_buffer_len;
  unsigned char busmail_crc;
};
#endif /* CELLIAX_CVM */

/* CELLIAX INTERNAL STRUCTS */

/*! 
 * \brief structure for the linked list of FBUS2 Nokia proprietary protocol messages
 */
struct fbus2_msg {
  int msg;
  int seqnum;
  int len;
  int acknowledged;
  int how_many_sent;
  int sent;
  unsigned int tv_sec;
  unsigned int tv_usec;
  unsigned char buffer[FBUS2_MAX_TRANSMIT_LENGTH + 10];
  struct fbus2_msg *next;
  struct fbus2_msg *previous;
};

/*! 
 * \brief structure for storing the results of AT commands, in an array of AT_MESG_MAX_LINES * AT_MESG_MAX_LENGTH chars
 */
struct s_result {
  int elemcount;
  char result[AT_MESG_MAX_LINES][AT_MESG_MAX_LENGTH];
};

/*! 
 * \brief PVT structure for a celliax interface (channel), created by celliax_mkif
 */
struct celliax_pvt {
  char *name;                   /*!< \brief 'name' of the interface (channel) */
  int interface_state;          /*!< \brief 'state' of the interface (channel) */
  int phone_callflow;           /*!< \brief 'callflow' of the phone interface (as opposed to skype interface) */
  struct ast_channel *owner;    /*!< \brief channel we belong to, possibly NULL */
  struct celliax_pvt *next;     /*!< \brief Next interface (channel) in list */
  int readpos;                  /*!< \brief read position above */
  struct ast_frame read_f;      /*!< \brief returned by oss_read */
  char context[AST_MAX_EXTENSION];  /*!< \brief default Asterisk dialplan context for this interface */
  char language[MAX_LANGUAGE];  /*!< \brief default Asterisk dialplan language for this interface */
  char exten[AST_MAX_EXTENSION];    /*!< \brief default Asterisk dialplan extension for this interface */
  struct ast_dsp *dsp;          /*!< \brief Used for in-band DTMF detection */
  int celliax_sound_rate;       /*!< \brief rate of the sound device, in Hz, eg: 8000 */
  int celliax_sound_capt_fd;    /*!< \brief file descriptor for sound capture dev */
  char controldevice_name[50];  /*!< \brief name of the serial device controlling the interface, possibly none */
  int controldevprotocol;       /*!< \brief which protocol is used for serial control of this interface */
  char controldevprotocolname[50];  /*!< \brief name of the serial device controlling protocol, one of "at" "fbus2" "no_serial" "alsa_voicemodem" */
  int controldevfd;             /*!< \brief serial controlling file descriptor for this interface */
  char callid_name[50];
  char callid_number[50];
  unsigned char rxm[255];       /*!< \brief read buffer for FBUS2 serial protocol controlling Nokia phones */
  unsigned char array[255];     /*!< \brief read buffer for FBUS2 serial protocol controlling Nokia phones */
  int arraycounter;             /*!< \brief position in the 'array' read buffer for FBUS2 serial protocol controlling Nokia phones */
  int seqnumfbus;               /*!< \brief sequential number of FBUS2 messages, hex, revolving */
  pthread_t controldev_thread;  /*!< \brief serial control thread for this interface, running during the call */
  struct fbus2_msg *fbus2_outgoing_list;    /*!< \brief list used to track FBUS2 traffic acknowledgement and resending */
  ast_mutex_t fbus2_outgoing_list_lock;
  int dsp_silence_threshold;
  int need_acoustic_ring;       /*!< \brief bool, this interface get the incoming ring from soundcard, not serial */
  char oss_write_buf[CELLIAX_FRAME_SIZE * 2];
  int oss_write_dst;
  char oss_read_buf[CELLIAX_FRAME_SIZE * 2 + AST_FRIENDLY_OFFSET];  /*!< in bytes */
  time_t celliax_serial_synced_timestamp;
  time_t celliax_serial_sync_period;
  time_t audio_play_reset_timestamp;
  time_t audio_capture_reset_timestamp;
  speed_t controldevice_speed;
  struct s_result line_array;
  struct timeval ringtime;
  struct timeval call_incoming_time;
  int at_result;

  char at_dial_pre_number[64];
  char at_dial_post_number[64];
  char at_dial_expect[64];
  unsigned int at_early_audio;
  char at_hangup[64];
  char at_hangup_expect[64];
  char at_answer[64];
  char at_answer_expect[64];
  unsigned int at_initial_pause;
  char at_preinit_1[64];
  char at_preinit_1_expect[64];
  char at_preinit_2[64];
  char at_preinit_2_expect[64];
  char at_preinit_3[64];
  char at_preinit_3_expect[64];
  char at_preinit_4[64];
  char at_preinit_4_expect[64];
  char at_preinit_5[64];
  char at_preinit_5_expect[64];
  unsigned int at_after_preinit_pause;

  char at_postinit_1[64];
  char at_postinit_1_expect[64];
  char at_postinit_2[64];
  char at_postinit_2_expect[64];
  char at_postinit_3[64];
  char at_postinit_3_expect[64];
  char at_postinit_4[64];
  char at_postinit_4_expect[64];
  char at_postinit_5[64];
  char at_postinit_5_expect[64];

  char at_send_dtmf[64];

  char at_query_battchg[64];
  char at_query_battchg_expect[64];
  char at_query_signal[64];
  char at_query_signal_expect[64];
  char at_call_idle[64];
  char at_call_incoming[64];
  char at_call_active[64];
  char at_call_failed[64];
  char at_call_calling[64];

#define CIEV_STRING_SIZE 64
  char at_indicator_noservice_string[64];
  char at_indicator_nosignal_string[64];
  char at_indicator_lowsignal_string[64];
  char at_indicator_lowbattchg_string[64];
  char at_indicator_nobattchg_string[64];
  char at_indicator_callactive_string[64];
  char at_indicator_nocallactive_string[64];
  char at_indicator_nocallsetup_string[64];
  char at_indicator_callsetupincoming_string[64];
  char at_indicator_callsetupoutgoing_string[64];
  char at_indicator_callsetupremoteringing_string[64];

  int at_indicator_callp;
  int at_indicator_callsetupp;
  int at_indicator_roamp;
  int at_indicator_battchgp;
  int at_indicator_servicep;
  int at_indicator_signalp;

  int at_has_clcc;
  int at_has_ecam;

  double playback_boost;
  double capture_boost;
  int stripmsd;
  int controldev_dead;
  ast_mutex_t controldev_lock;
  struct timeval fbus2_list_tv;
  struct timezone fbus2_list_tz;
  dtmf_rx_state_t dtmf_state;
  int dtmf_inited;
  pthread_t sync_thread;
  pthread_t celliax_sound_monitor_thread;
  pthread_t celliax_serial_monitor_thread;
  int celliax_serial_monitoring;
  int skype;                    /*!< \brief config flag, bool, Skype support on this interface (0 if false, -1 if true) */
  int phonebook_listing;
  int phonebook_querying;
  int phonebook_listing_received_calls;

  int phonebook_first_entry;
  int phonebook_last_entry;
  int phonebook_number_lenght;
  int phonebook_text_lenght;
  FILE *phonebook_writing_fp;
  int celliax_dir_entry_extension_prefix;
#ifdef CELLIAX_CVM
  char cvm_subsc_1_pin[20];
  char cvm_subsc_2_pin[20];
  int cvm_subsc_no;
  int cvm_lock_state;
  int cvm_register_state;
  int cvm_volume_level;
  int cvm_celliax_serial_delay;
  unsigned char cvm_handset_no;
  unsigned char cvm_fp_is_cvm;
  unsigned char cvm_rssi;

  unsigned char busmail_rxseq_cvm_last; /*!< \brief sequential number of BUSMAIL messages, (0-7) */
  unsigned char busmail_txseq_celliax_last; /*!< \brief sequential number of BUSMAIL messages, (0-7) */

  struct cvm_busmail_msg *cvm_busmail_outgoing_list;    /*!< \brief list used to track CVM BUSMAIL traffic acknowledgement and resending */
  ast_mutex_t cvm_busmail_outgoing_list_lock;

  struct timeval cvm_busmail_list_tv;
  struct timezone cvm_busmail_list_tz;
#endif                          /* CELLIAX_CVM */
#ifdef CELLIAX_LIBCSV
  int csv_separator_is_semicolon;
  int csv_complete_name_pos;
  int csv_email_pos;
  int csv_home_phone_pos;
  int csv_mobile_phone_pos;
  int csv_business_phone_pos;
  int csv_first_row_is_title;
  int csv_fields;
  int csv_rows;
  char csv_complete_name[256];
  char csv_email[256];
  char csv_home_phone[256];
  char csv_mobile_phone[256];
  char csv_business_phone[256];
#endif                          /* CELLIAX_LIBCSV */
  int audio_play_reset_period;

  char at_cmgw[16];

  int isInputInterleaved;
  int isOutputInterleaved;
  int numInputChannels;
  int numOutputChannels;
  int framesPerCallback;
#ifndef CELLIAX_ALSA
  PABLIO_Stream *stream;
#endif /* CELLIAX_ALSA */
  int audiopipe[2];
  int speexecho;
  int speexpreprocess;
  int portaudiocindex;          /*!< \brief Index of the Portaudio capture audio device */
  int portaudiopindex;          /*!< \brief Index of the Portaudio playback audio device */
  int control_to_send;
  int unread_sms_msg_id;
  int reading_sms_msg;
  char sms_message[4800];
  int sms_cnmi_not_supported;
  char sms_receiving_program[256];
  int celliax_dir_prefix;
  int no_ucs2;
#ifdef CELLIAX_ALSA
  snd_pcm_t *alsac;             /*!< \brief handle of the ALSA capture audio device */
  snd_pcm_t *alsap;             /*!< \brief handle of the ALSA playback audio device */
  char alsacname[50];           /*!< \brief name of the ALSA capture audio device */
  char alsapname[50];           /*!< \brief name of the ALSA playback audio device */
  int alsa_period_size;         /*!< \brief ALSA period_size, in byte */
  int alsa_periods_in_buffer;   /*!< \brief how many periods in ALSA buffer, to calculate buffer_size */
  unsigned long int alsa_buffer_size;   /*!< \brief ALSA buffer_size, in byte */
  int alsawrite_filled;
  int alsa_capture_is_mono;
  int alsa_play_is_mono;
  struct pollfd pfd;
#endif                          /* CELLIAX_ALSA */

  struct timeval dtmf_timestamp;
};

/* LOCKS */
/*! \brief Protect the celliax_usecnt */
AST_MUTEX_DEFINE_STATIC(celliax_usecnt_lock);
/*! \brief Protect the monitoring thread, so only one process can kill or start it, and not
 *    when it's doing something critical. */
AST_MUTEX_DEFINE_STATIC(celliax_monlock);
/*! \brief Protect the interfaces list */
AST_MUTEX_DEFINE_STATIC(celliax_iflock);

/* FUNCTIONS */

/* module helpers functions */
int load_module(void);
int unload_module(void);
int usecount(void);
char *description(void);
char *key(void);

/* chan_celliax internal functions */
void celliax_unlocka_log(void *x);
#ifdef CELLIAX_FBUS2
int celliax_serial_sync_FBUS2(struct celliax_pvt *p);
int celliax_serial_answer_FBUS2(struct celliax_pvt *p);
int celliax_serial_call_FBUS2(struct celliax_pvt *p, char *dstr);
int celliax_serial_hangup_FBUS2(struct celliax_pvt *p);
int celliax_serial_config_FBUS2(struct celliax_pvt *p);
int celliax_serial_read_FBUS2(struct celliax_pvt *p);
int celliax_serial_getstatus_FBUS2(struct celliax_pvt *p);
int celliax_serial_get_seqnum_FBUS2(struct celliax_pvt *p);
int celliax_serial_security_command_FBUS2(struct celliax_pvt *p);
int celliax_serial_send_FBUS2(struct celliax_pvt *p, int len, unsigned char *buffer2);
int celliax_serial_list_acknowledge_FBUS2(struct celliax_pvt *p, int seqnum);
int celliax_serial_send_if_time_FBUS2(struct celliax_pvt *p);
int celliax_serial_write_FBUS2(struct celliax_pvt *p, unsigned char *MsgBuffer,
                               int MsgLength, unsigned char MsgType);
int celliax_serial_send_ack_FBUS2(struct celliax_pvt *p, unsigned char MsgType,
                                  unsigned char MsgSequence);
struct fbus2_msg *celliax_serial_list_init_FBUS2(struct celliax_pvt *p);
int celliax_serial_list_print_FBUS2(struct celliax_pvt *p, struct fbus2_msg *list);

#endif /* CELLIAX_FBUS2 */

#ifdef CELLIAX_CVM
int celliax_serial_sync_CVM_BUSMAIL(struct celliax_pvt *p);
int celliax_serial_answer_CVM_BUSMAIL(struct celliax_pvt *p);
int celliax_serial_call_CVM_BUSMAIL(struct celliax_pvt *p, char *dstr);
int celliax_serial_hangup_CVM_BUSMAIL(struct celliax_pvt *p);
int celliax_serial_config_CVM_BUSMAIL(struct celliax_pvt *p);
int celliax_serial_read_CVM_BUSMAIL(struct celliax_pvt *p);
int celliax_serial_getstatus_CVM_BUSMAIL(struct celliax_pvt *p);
int celliax_serial_send_CVM_BUSMAIL(struct celliax_pvt *p, int len,
                                    unsigned char *mesg_ptr);
int celliax_serial_list_acknowledge_CVM_BUSMAIL(struct celliax_pvt *p,
                                                unsigned char TxSeqNo);
int celliax_serial_send_if_time_CVM_BUSMAIL(struct celliax_pvt *p);
int celliax_serial_write_CVM_BUSMAIL(struct celliax_pvt *p,
                                     struct cvm_busmail_frame *busmail_frame);
int celliax_serial_send_ctrl_frame_CVM_BUSMAIL(struct celliax_pvt *p,
                                               unsigned char FrameType);
int celliax_serial_send_info_frame_CVM_BUSMAIL(struct celliax_pvt *p, int FrameType,
                                               unsigned char ParamsLen,
                                               unsigned char *Params);
struct cvm_busmail_msg *celliax_serial_list_init_CVM_BUSMAIL(struct celliax_pvt *p);
int celliax_serial_list_print_CVM_BUSMAIL(struct celliax_pvt *p,
                                          struct cvm_busmail_msg *list);
int celliax_serial_lists_free_CVM_BUSMAIL(struct celliax_pvt *p);

#endif /* CELLIAX_CVM */

/* CHAN_CELLIAX.C */
int celliax_queue_control(struct ast_channel *chan, int control);
struct celliax_pvt *celliax_console_find_desc(char *dev);
int celliax_serial_call(struct celliax_pvt *p, char *dstr);

/* FUNCTIONS */
/* PBX interface functions */
struct ast_channel *celliax_request(const char *type, int format, void *data, int *cause);
int celliax_answer(struct ast_channel *c);
int celliax_hangup(struct ast_channel *c);
int celliax_call(struct ast_channel *c, char *idest, int timeout);
struct ast_frame *celliax_read(struct ast_channel *chan);
int celliax_write(struct ast_channel *c, struct ast_frame *f);
int celliax_fixup(struct ast_channel *oldchan, struct ast_channel *newchan);
#ifndef ASTERISK_VERSION_1_4
int celliax_indicate(struct ast_channel *c, int cond);
#else
int celliax_indicate(struct ast_channel *c, int cond, const void *data, size_t datalen);
#endif
int celliax_devicestate(void *data);
#ifdef ASTERISK_VERSION_1_4
int celliax_senddigit_begin(struct ast_channel *ast, char digit);
int celliax_senddigit_end(struct ast_channel *ast, char digit, unsigned int duration);
#else /* ASTERISK_VERSION_1_4 */
int celliax_senddigit(struct ast_channel *ast, char digit);
#endif /* ASTERISK_VERSION_1_4 */

/* chan_celliax internal functions */

struct celliax_pvt *celliax_mkif(struct ast_config *cfg, char *ctg,
                                 int is_first_category);
struct ast_channel *celliax_new(struct celliax_pvt *p, int state, char *context);
int celliax_restart_monitor(void);
void *celliax_do_monitor(void *data);
void *celliax_do_audio_monitor(void *data);
int celliax_sound_boost(struct ast_frame *f, double boost);
int celliax_sound_init(struct celliax_pvt *p);
int celliax_sound_shutdown(struct celliax_pvt *p);
struct ast_frame *celliax_sound_dsp_analize(struct celliax_pvt *p, struct ast_frame *f,
                                            int dsp_silence_threshold);
int celliax_sound_dsp_set(struct celliax_pvt *p, int dsp_silence_threshold,
                          int silence_suppression);
struct ast_frame *celliax_sound_read(struct celliax_pvt *p);
int celliax_sound_write(struct celliax_pvt *p, struct ast_frame *f);
int celliax_sound_monitor(struct celliax_pvt *p);
void *celliax_do_controldev_thread(void *data);
int celliax_serial_init(struct celliax_pvt *p, speed_t controldevice_speed);
int celliax_serial_monitor(struct celliax_pvt *p);
int celliax_serial_read(struct celliax_pvt *p);
int celliax_serial_sync(struct celliax_pvt *p);
int celliax_serial_getstatus(struct celliax_pvt *p);
int celliax_serial_config(struct celliax_pvt *p);
int celliax_serial_hangup(struct celliax_pvt *p);
int celliax_serial_answer(struct celliax_pvt *p);

#define celliax_serial_write_AT_expect(P, D, S) celliax_serial_write_AT_expect1(P, D, S, 1, 2)
#define celliax_serial_write_AT_expect_noexpcr(P, D, S) celliax_serial_write_AT_expect1(P, D, S, 0, 2)
#define celliax_serial_write_AT_expect_noexpcr_tout(P, D, S, T) celliax_serial_write_AT_expect1(P, D, S, 0, T)
// 20.5 sec timeout, used for querying the SIM and sending SMSs
#define celliax_serial_write_AT_expect_longtime(P, D, S) celliax_serial_write_AT_expect1(P, D, S, 1, 20)
#define celliax_serial_write_AT_expect_longtime_noexpcr(P, D, S) celliax_serial_write_AT_expect1(P, D, S, 0, 20)
int celliax_serial_write_AT(struct celliax_pvt *p, const char *data);
int celliax_serial_write_AT_nocr(struct celliax_pvt *p, const char *data);
int celliax_serial_write_AT_ack(struct celliax_pvt *p, const char *data);
int celliax_serial_write_AT_ack_nocr_longtime(struct celliax_pvt *p, const char *data);
int celliax_serial_write_AT_noack(struct celliax_pvt *p, const char *data);
int celliax_serial_write_AT_expect1(struct celliax_pvt *p, const char *data,
                                    const char *expected_string, int expect_crlf,
                                    int seconds);
int celliax_serial_AT_expect(struct celliax_pvt *p, const char *expected_string,
                             int expect_crlf, int seconds);
int celliax_serial_read_AT(struct celliax_pvt *p, int look_for_ack, int timeout_usec,
                           int timeout_sec, const char *expected_string, int expect_crlf);
int celliax_serial_answer_AT(struct celliax_pvt *p);
int celliax_serial_hangup_AT(struct celliax_pvt *p);
int celliax_serial_config_AT(struct celliax_pvt *p);
int celliax_serial_call_AT(struct celliax_pvt *p, char *dstr);
int celliax_serial_sync_AT(struct celliax_pvt *p);
int celliax_serial_getstatus_AT(struct celliax_pvt *p);

#ifdef ASTERISK_VERSION_1_6_0
void celliax_store_boost(const char *s, double *boost);
#else
void celliax_store_boost(char *s, double *boost);
#endif /* ASTERISK_VERSION_1_6_0 */
int celliax_console_set_active(int fd, int argc, char *argv[]);
#ifndef ASTERISK_VERSION_1_6_0
int celliax_console_hangup(int fd, int argc, char *argv[]);
#else /* ASTERISK_VERSION_1_6_0 */
char *celliax_console_hangup(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a);
#endif /*  ASTERISK_VERSION_1_6_0 */
int celliax_console_playback_boost(int fd, int argc, char *argv[]);
int celliax_console_capture_boost(int fd, int argc, char *argv[]);
int celliax_console_celliax(int fd, int argc, char *argv[]);
#ifdef CELLIAX_DIR
int celliax_console_celliax_dir_import(int fd, int argc, char *argv[]);
int celliax_console_celliax_dir_export(int fd, int argc, char *argv[]);
#endif /* CELLIAX_DIR */
int celliax_console_dial(int fd, int argc, char *argv[]);
int celliax_console_sendsms(int fd, int argc, char *argv[]);
int celliax_portaudio_init(struct celliax_pvt *p);
int celliax_portaudio_shutdown(struct celliax_pvt *p);
struct ast_frame *celliax_portaudio_read(struct celliax_pvt *p);
int celliax_portaudio_write(struct celliax_pvt *p, struct ast_frame *f);
int celliax_portaudio_devlist(struct celliax_pvt *p);
#ifdef CELLIAX_DIR
int celliax_dir_exec(struct ast_channel *chan, void *data);
int celliax_dir_create_extensions(void);
int celliax_dir_play_mailbox_owner(struct ast_channel *chan, char *context,
                                   char *dialcontext, char *ext, char *name);
struct ast_config *celliax_dir_realtime(char *context);
int celliax_dir_do(struct ast_channel *chan, struct ast_config *cfg, char *context,
                   char *dialcontext, char digit, int last);
#endif /* CELLIAX_DIR */
#ifdef CELLIAX_LIBCSV
void celliax_cb1(char *s, size_t len, void *data);
void celliax_cb2(char c, void *data);
#endif /* CELLIAX_LIBCSV */
int celliax_sendsms(struct ast_channel *c, void *data);
int celliax_console_echo(int fd, int argc, char *argv[]);
int celliax_console_at(int fd, int argc, char *argv[]);
#ifdef ASTERISK_VERSION_1_2
int celliax_manager_sendsms(struct mansession *s, struct message *m);
#endif //ASTERISK_VERSION_1_2
#ifdef ASTERISK_VERSION_1_4
int celliax_manager_sendsms(struct mansession *s, const struct message *m);
#endif //ASTERISK_VERSION_1_4
int utf_to_ucs2(struct celliax_pvt *p, char *utf_in, size_t inbytesleft, char *ucs2_out,
                size_t outbytesleft);
int ucs2_to_utf8(struct celliax_pvt *p, char *ucs2_in, char *utf8_out,
                 size_t outbytesleft);
#endif /* _CELLIAX_H_ */
#ifdef CELLIAX_ALSA
int console_alsa_period(int fd, int argc, char *argv[]);
#endif /* CELLIAX_ALSA */
#ifdef CELLIAX_ALSA
int alsa_init(struct celliax_pvt *p);
int alsa_shutdown(struct celliax_pvt *p);
snd_pcm_t *alsa_open_dev(struct celliax_pvt *p, snd_pcm_stream_t stream);
struct ast_frame *alsa_read(struct celliax_pvt *p);
int alsa_write(struct celliax_pvt *p, struct ast_frame *f);
#endif /* CELLIAX_ALSA */
