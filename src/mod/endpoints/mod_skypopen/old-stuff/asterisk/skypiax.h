//indent -gnu -ts4 -br -brs -cdw -lp -ce -nbfda -npcs -nprs -npsl -nbbo -saf -sai -saw -cs -bbo -nhnl -nut -sob -l90 
#ifndef _SKYPIAX_H_
#define _SKYPIAX_H_

#ifndef SKYPIAX_SVN_VERSION
#define SKYPIAX_SVN_VERSION "????NO_REVISION???"
#endif

#include <asterisk/version.h>	/* needed here for conditional compilation on version.h */
  /* the following #defs are for LINUX */
#ifndef __CYGWIN__
#ifndef ASTERISK_VERSION_1_6
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
#endif /* ASTERISK_VERSION_1_6 */
#ifdef ASTERISK_VERSION_1_2
#undef ASTERISK_VERSION_1_4
#endif /* ASTERISK_VERSION_1_2 */
#ifdef ASTERISK_VERSION_1_6
#define ASTERISK_VERSION_1_4
#endif /* ASTERISK_VERSION_1_6 */
#define SKYPIAX_SKYPE
#define WANT_SKYPE_X11
#endif /* NOT __CYGWIN__ */
  /* the following #defs are for WINDOWS */
#ifdef __CYGWIN__
#undef ASTERISK_VERSION_1_4
#undef ASTERISK_VERSION_1_6
#define SKYPIAX_SKYPE
#undef WANT_SKYPE_X11
#endif /* __CYGWIN__ */

/* INCLUDES */
#ifdef ASTERISK_VERSION_1_6
#include <asterisk.h>			/* some asterisk-devel package do not contains asterisk.h, but seems that is needed for the 1.6 series, at least from trunk */
#endif /* ASTERISK_VERSION_1_6 */
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
#include <asterisk/frame.h>
#include <asterisk/channel.h>
#include <asterisk/module.h>
#include <asterisk/options.h>
#include <asterisk/pbx.h>
#include <asterisk/config.h>
#include <asterisk/cli.h>
#include <asterisk/causes.h>
#include <asterisk/endian.h>
#include <asterisk/lock.h>
#include <asterisk/devicestate.h>
#include <asterisk/file.h>
#include <asterisk/say.h>
#ifdef ASTERISK_VERSION_1_6
#include <asterisk/astobj2.h>
#include <asterisk/paths.h>
#endif /* ASTERISK_VERSION_1_6 */
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
//#include "skypiax_spandsp.h"
#ifdef __CYGWIN__
#include <windows.h>
#endif /* __CYGWIN__ */
#ifdef WANT_SKYPE_X11
#include <X11/Xlib.h>
#include <X11/Xlibint.h>
#include <X11/Xatom.h>
#endif /*  WANT_SKYPE_X11 */
#ifndef AST_DIGIT_ANYDIG
#define AST_DIGIT_ANYDIG "0123456789*#"
#else
#warning Please review Skypiax AST_DIGIT_ANYDIG
#endif
#ifndef _ASTERISK_H
#define AST_CONFIG_MAX_PATH 255	/* defined in asterisk.h, but some asterisk-devel package do not contains asterisk.h */
extern char ast_config_AST_CONFIG_DIR[AST_CONFIG_MAX_PATH];
int ast_register_atexit(void (*func) (void));	/* in asterisk.h, but some asterisk-devel package do not contains asterisk.h */
void ast_unregister_atexit(void (*func) (void));	/* in asterisk.h, but some asterisk-devel package do not contains asterisk.h */
#endif

/* DEFINITIONS */
#define SAMPLERATE_SKYPIAX 8000
#define SAMPLES_PER_FRAME SAMPLERATE_SKYPIAX/50
#define SKYPIAX_DIR_CONFIG "directoriax.conf"

/* LUIGI RIZZO's magic */
/* boost support. BOOST_SCALE * 10 ^(BOOST_MAX/20) must
 * be representable in 16 bits to avoid overflows.
 */
#define	BOOST_SCALE	(1<<9)
#define	BOOST_MAX	40			/* slightly less than 7 bits */
/* call flow from the device */
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
#define 	CALLFLOW_STATUS_FINISHED   110
#define 	CALLFLOW_STATUS_CANCELLED   111
#define 	CALLFLOW_STATUS_FAILED   112
#define 	CALLFLOW_STATUS_REFUSED   113
#define 	CALLFLOW_STATUS_RINGING   114
#define 	CALLFLOW_STATUS_INPROGRESS   115
#define 	CALLFLOW_STATUS_UNPLACED   116
#define 	CALLFLOW_STATUS_ROUTING   117
#define 	CALLFLOW_STATUS_EARLYMEDIA   118
#define 	AST_STATE_HANGUP_REQUESTED   200
  //FIXME CALLFLOW_INCOMING_CALLID to be removed
#define 	CALLFLOW_INCOMING_CALLID   1019
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
#define DEBUG_ALL DEBUG_SOUND|DEBUG_SERIAL|DEBUG_SKYPE|DEBUG_AT|DEBUG_FBUS2|DEBUG_CALL|DEBUG_PBX|DEBUG_LOCKS|DEBUG_MONITORLOCKS
/* wrappers for ast_log */
#define DEBUGA_SOUND(...)  if (skypiax_debug & DEBUG_SOUND) ast_log(LOG_DEBUG, 		"rev "SKYPIAX_SVN_VERSION "[%p|%-7lx][DEBUG_SOUND  %-5d][%-10s][%2d,%2d,%2d] " __VA_ARGS__ );
#define DEBUGA_SERIAL(...)  if (skypiax_debug & DEBUG_SERIAL) ast_log(LOG_DEBUG, 	"rev "SKYPIAX_SVN_VERSION "[%p|%-7lx][DEBUG_SERIAL %-5d][%-10s][%2d,%2d,%2d] " __VA_ARGS__ );
#define DEBUGA_SKYPE(...)  if (skypiax_debug & DEBUG_SKYPE) ast_log(LOG_DEBUG, 		"rev "SKYPIAX_SVN_VERSION "[%p|%-7lx][DEBUG_SKYPE  %-5d][%-10s][%2d,%2d,%2d] " __VA_ARGS__ );
#define DEBUGA_AT(...)  if (skypiax_debug & DEBUG_AT) ast_log(LOG_DEBUG, 		"rev "SKYPIAX_SVN_VERSION "[%p|%-7lx][DEBUG_AT     %-5d][%-10s][%2d,%2d,%2d] " __VA_ARGS__ );
#define DEBUGA_FBUS2(...)  if (skypiax_debug & DEBUG_FBUS2) ast_log(LOG_DEBUG, 		"rev "SKYPIAX_SVN_VERSION "[%p|%-7lx][DEBUG_FBUS2  %-5d][%-10s][%2d,%2d,%2d] " __VA_ARGS__ );
#define DEBUGA_CALL(...)  if (skypiax_debug & DEBUG_CALL) ast_log(LOG_DEBUG, 		"rev "SKYPIAX_SVN_VERSION "[%p|%-7lx][DEBUG_CALL   %-5d][%-10s][%2d,%2d,%2d] " __VA_ARGS__ );
#define DEBUGA_PBX(...)  if (skypiax_debug & DEBUG_PBX) ast_log(LOG_DEBUG, 		"rev "SKYPIAX_SVN_VERSION "[%p|%-7lx][DEBUG_PBX    %-5d][%-10s][%2d,%2d,%2d] " __VA_ARGS__ );
#define ERRORA(...)  ast_log(LOG_ERROR, 						"rev "SKYPIAX_SVN_VERSION "[%p|%-7lx][ERROR        %-5d][%-10s][%2d,%2d,%2d] " __VA_ARGS__ );
#define NOTICA(...)  ast_log(LOG_NOTICE, 						"rev "SKYPIAX_SVN_VERSION "[%p|%-7lx][NOTICE      %-5d][%-10s][%2d,%2d,%2d] " __VA_ARGS__ );
#define WARNINGA(...)  ast_log(LOG_WARNING, 						"rev "SKYPIAX_SVN_VERSION "[%p|%-7lx][WARNING    %-5d][%-10s][%2d,%2d,%2d] " __VA_ARGS__ );
/* macros for logging */
#define SKYPIAX_P_LOG p ? p->owner : NULL, (unsigned long)pthread_self(), __LINE__, p ? p->name ? p->name : "none" : "none", p ? p->owner ? p->owner->_state : -1 : -1,  p ? p->interface_state : -1, p ? p->skype_callflow : -1
#define SKYPIAX_TMP_LOG tmp ? tmp->owner : NULL, (unsigned long)pthread_self(), __LINE__, tmp ? tmp->name ? tmp->name : "none" : "none", tmp ? tmp->owner ? tmp->owner->_state : -1 : -1,  tmp ? tmp->interface_state : -1, tmp ? tmp->skype_callflow : -1
/* logging wrappers for ast_mutex_lock and ast_mutex_unlock */
#define LOKKA(x)  if (skypiax_debug & DEBUG_LOCKS) ast_log(LOG_DEBUG, "rev "SKYPIAX_SVN_VERSION "[%p|%-7lx][DEBUG_LOCKS  %-5d][%-10s][%2d,%2d,%2d] going to lock %p (%s)\n", SKYPIAX_P_LOG, x, x == &skypiax_monlock ? "MONLOCK" : x == &skypiax_iflock ? "IFLOCK" : x == &skypiax_usecnt_lock ? "USECNT_LOCK" : "?????"); if (ast_mutex_lock(x)) ast_log(LOG_ERROR, "ast_mutex_lock failed, BAD\n");   if (skypiax_debug & DEBUG_LOCKS) ast_log(LOG_DEBUG, "rev "SKYPIAX_SVN_VERSION "[%p|%-7lx][DEBUG_LOCKS  %-5d][%-10s][%2d,%2d,%2d] locked %p (%s)\n", SKYPIAX_P_LOG, x, x == &skypiax_monlock ? "MONLOCK" : x == &skypiax_iflock ? "IFLOCK" : x == &skypiax_usecnt_lock ? "USECNT_LOCK" : "?????");
#define UNLOCKA(x)  if (skypiax_debug & DEBUG_LOCKS)  ast_log(LOG_DEBUG, "rev "SKYPIAX_SVN_VERSION "[%p|%-7lx][DEBUG_LOCKS  %-5d][%-10s][%2d,%2d,%2d] going to unlock %p (%s)\n",  SKYPIAX_P_LOG, x, x == &skypiax_monlock ? "MONLOCK" : x == &skypiax_iflock ? "IFLOCK" : x == &skypiax_usecnt_lock ? "USECNT_LOCK" : "?????"); if (ast_mutex_unlock(x)) ast_log(LOG_ERROR, "ast_mutex_lock failed, BAD\n");   if (skypiax_debug & DEBUG_LOCKS) ast_log(LOG_DEBUG, "rev "SKYPIAX_SVN_VERSION "[%p|%-7lx][DEBUG_LOCKS  %-5d][%-10s][%2d,%2d,%2d] unlocked %p (%s)\n", SKYPIAX_P_LOG, x, x == &skypiax_monlock ? "MONLOCK" : x == &skypiax_iflock ? "IFLOCK" : x == &skypiax_usecnt_lock ? "USECNT_LOCK" : "?????");
#define PUSHA_UNLOCKA(x)    pthread_cleanup_push(skypiax_unlocka_log, (void *) x);
#define POPPA_UNLOCKA(x)    pthread_cleanup_pop(0);
#define MONITORLOKKA(x)  if (skypiax_debug & DEBUG_MONITORLOCKS) ast_log(LOG_DEBUG, "rev "SKYPIAX_SVN_VERSION "[%p|%-7lx][DEBUG_MONITORLOCKS  %-5d][%-10s][%2d,%2d,%2d] going to lock %p (%s)\n", SKYPIAX_P_LOG, x, x == &skypiax_monlock ? "MONLOCK" : x == &skypiax_iflock ? "IFLOCK" : x == &skypiax_usecnt_lock ? "USECNT_LOCK" :  "?????"); if (ast_mutex_lock(x)) ast_log(LOG_ERROR, "ast_mutex_lock failed, BAD\n");   if (skypiax_debug & DEBUG_MONITORLOCKS) ast_log(LOG_DEBUG, "rev "SKYPIAX_SVN_VERSION "[%p|%-7lx][DEBUG_MONITORLOCKS  %-5d][%-10s][%2d,%2d,%2d] locked %p (%s)\n", SKYPIAX_P_LOG, x, x == &skypiax_monlock ? "MONLOCK" : x == &skypiax_iflock ? "IFLOCK" : x == &skypiax_usecnt_lock ? "USECNT_LOCK" : "?????");
#define MONITORUNLOCKA(x)  if (skypiax_debug & DEBUG_MONITORLOCKS)  ast_log(LOG_DEBUG, "rev "SKYPIAX_SVN_VERSION "[%p|%-7lx][DEBUG_MONITORLOCKS  %-5d][%-10s][%2d,%2d,%2d] going to unlock %p (%s)\n",  SKYPIAX_P_LOG, x, x == &skypiax_monlock ? "MONLOCK" : x == &skypiax_iflock ? "IFLOCK" : x == &skypiax_usecnt_lock ? "USECNT_LOCK" : "?????"); if (ast_mutex_unlock(x)) ast_log(LOG_ERROR, "ast_mutex_lock failed, BAD\n");   if (skypiax_debug & DEBUG_MONITORLOCKS) ast_log(LOG_DEBUG, "rev "SKYPIAX_SVN_VERSION "[%p|%-7lx][DEBUG_MONITORLOCKS  %-5d][%-10s][%2d,%2d,%2d] unlocked %p (%s)\n", SKYPIAX_P_LOG, x, x == &skypiax_monlock ? "MONLOCK" : x == &skypiax_iflock ? "IFLOCK" : x == &skypiax_usecnt_lock ? "USECNT_LOCK" : "?????");
/* macros used for config file parsing (luigi rizzo)*/
#define 	M_BOOL(tag, dst)   M_F(tag, (dst) = ast_true(__val) )
#define 	M_END(x)   x;
#define 	M_F(tag, f)   if (!strcasecmp((__s), tag)) { f; } else
#ifdef ASTERISK_VERSION_1_6
#define 	M_START(var, val)   const char *__s = var; const char *__val = val;
#else
#define 	M_START(var, val)   char *__s = var; char *__val = val;
#endif /* ASTERISK_VERSION_1_6 */
#define 	M_STR(tag, dst)   M_F(tag, ast_copy_string(dst, __val, sizeof(dst)))
#define 	M_UINT(tag, dst)   M_F(tag, (dst) = strtoul(__val, NULL, 0) )

#define 	SKYPIAX_FRAME_SIZE   160

/* SKYPIAX INTERNAL STRUCTS */
/*! 
 * \brief structure for exchanging messages with the skype client
 */
#ifdef WANT_SKYPE_X11
struct AsteriskHandles {
	Window skype_win;
	Display *disp;
	Window win;
	int fdesc[2];
};
#else /* WANT_SKYPE_X11 */
struct AsteriskHandles {
	HWND win32_hInit_MainWindowHandle;
	HWND win32_hGlobal_SkypeAPIWindowHandle;
	int fdesc[2];
};
#endif /* WANT_SKYPE_X11 */

#ifndef WIN32
struct SkypiaxHandles {
	Window skype_win;
	Display *disp;
	Window win;
	int api_connected;
	int fdesc[2];
};
#else //WIN32

struct SkypiaxHandles {
	HWND win32_hInit_MainWindowHandle;
	HWND win32_hGlobal_SkypeAPIWindowHandle;
	HINSTANCE win32_hInit_ProcessHandle;
	char win32_acInit_WindowClassName[128];
	UINT win32_uiGlobal_MsgID_SkypeControlAPIAttach;
	UINT win32_uiGlobal_MsgID_SkypeControlAPIDiscover;
	int api_connected;
	int fdesc[2];
};

#endif //WIN32

/*! 
 * \brief PVT structure for a skypiax interface (channel), created by skypiax_mkif
 */
struct skypiax_pvt {
	char *name;					/*!< \brief 'name' of the interface (channel) */
	int interface_state;		/*!< \brief 'state' of the interface (channel) */
	struct ast_channel *owner;	/*!< \brief channel we belong to, possibly NULL */
	struct skypiax_pvt *next;	/*!< \brief Next interface (channel) in list */
	char context[AST_MAX_EXTENSION];	/*!< \brief default Asterisk dialplan context for this interface */
	char language[MAX_LANGUAGE];	/*!< \brief default Asterisk dialplan language for this interface */
	char exten[AST_MAX_EXTENSION];	/*!< \brief default Asterisk dialplan extension for this interface */
	int skypiax_sound_rate;		/*!< \brief rate of the sound device, in Hz, eg: 8000 */
	int skypiax_sound_capt_fd;	/*!< \brief file descriptor for sound capture dev */
	char callid_name[50];
	char callid_number[50];
	pthread_t controldev_thread;	/*!< \brief serial control thread for this interface, running during the call */
	double playback_boost;
	double capture_boost;
	int stripmsd;
	pthread_t skype_thread;
	struct AsteriskHandles AsteriskHandlesAst;
	struct SkypiaxHandles SkypiaxHandles;
	char skype_call_id[512];
	int skype_call_ongoing;
	char skype_friends[4096];
	char skype_fullname[512];
	char skype_displayname[512];
	int skype_callflow;			/*!< \brief 'callflow' of the skype interface (as opposed to phone interface) */
	int skype;					/*!< \brief config flag, bool, Skype support on this interface (0 if false, -1 if true) */
	int control_to_send;
	int audiopipe[2];
	int audioskypepipe[2];
	pthread_t tcp_srv_thread;
	pthread_t tcp_cli_thread;
	short audiobuf[160];
	int audiobuf_is_loaded;

	//int phonebook_listing;
	//int phonebook_querying;
	//int phonebook_listing_received_calls;

	//int phonebook_first_entry;
	//int phonebook_last_entry;
	//int phonebook_number_lenght;
	//int phonebook_text_lenght;
	FILE *phonebook_writing_fp;
	int skypiax_dir_entry_extension_prefix;
#ifdef WIN32
	unsigned short tcp_cli_port;
	unsigned short tcp_srv_port;
#else
	int tcp_cli_port;
	int tcp_srv_port;
#endif
	char X11_display[256];

	struct ast_frame read_frame;

	char skype_user[256];
	char skype_password[256];
	char destination[256];
	char session_uuid_str[512 + 1];
	pthread_t signaling_thread;
};

typedef struct skypiax_pvt private_t;
/* FUNCTIONS */

/* module helpers functions */
int load_module(void);
int unload_module(void);
int usecount(void);
char *description(void);
char *key(void);

/* chan_skypiax internal functions */
void skypiax_unlocka_log(void *x);

void *do_skypeapi_thread(void *data);
//int skypiax2skype(struct ast_channel *c, void *data);
//int skype2skypiax(struct ast_channel *c, void *data);
//void skypiax_disconnect(void);
int skypiax_signaling_write(struct skypiax_pvt *p, char *msg_to_skype);
int skypiax_signaling_read(struct skypiax_pvt *p);
int skypiax_console_skype(int fd, int argc, char *argv[]);
#ifdef WANT_SKYPE_X11
int X11_errors_handler(Display * dpy, XErrorEvent * err);
int skypiax_send_message(struct SkypiaxHandles *SkypiaxHandles, const char *message_P);
int skypiax_present(struct SkypiaxHandles *SkypiaxHandles);
void skypiax_clean_disp(void *data);
#endif /* WANT_SKYPE_X11 */
#ifdef __CYGWIN__

int win32_Initialize_CreateWindowClass(private_t * tech_pvt);
void win32_DeInitialize_DestroyWindowClass(private_t * tech_pvt);
int win32_Initialize_CreateMainWindow(private_t * tech_pvt);
void win32_DeInitialize_DestroyMainWindow(private_t * tech_pvt);
#endif /* __CYGWIN__ */

/* CHAN_SKYPIAX.C */
int skypiax_queue_control(struct ast_channel *chan, int control);
struct skypiax_pvt *skypiax_console_find_desc(char *dev);
int skypiax_serial_call(struct skypiax_pvt *p, char *dstr);

/* FUNCTIONS */
/* PBX interface functions */
struct ast_channel *skypiax_request(const char *type, int format, void *data, int *cause);
int skypiax_answer(struct ast_channel *c);
int skypiax_hangup(struct ast_channel *c);
int skypiax_originate_call(struct ast_channel *c, char *idest, int timeout);
struct ast_frame *skypiax_read(struct ast_channel *chan);
int skypiax_write(struct ast_channel *c, struct ast_frame *f);
int skypiax_fixup(struct ast_channel *oldchan, struct ast_channel *newchan);
#ifndef ASTERISK_VERSION_1_4
int skypiax_indicate(struct ast_channel *c, int cond);
#else
int skypiax_indicate(struct ast_channel *c, int cond, const void *data, size_t datalen);
#endif
int skypiax_devicestate(void *data);
#ifdef ASTERISK_VERSION_1_4
int skypiax_digitsend_begin(struct ast_channel *ast, char digit);
int skypiax_digitsend_end(struct ast_channel *ast, char digit, unsigned int duration);
#else /* ASTERISK_VERSION_1_4 */
int skypiax_digitsend(struct ast_channel *ast, char digit);
#endif /* ASTERISK_VERSION_1_4 */

/* chan_skypiax internal functions */

struct skypiax_pvt *skypiax_mkif(struct ast_config *cfg, char *ctg, int is_first_category);
struct ast_channel *skypiax_new(struct skypiax_pvt *p, int state, char *context);
int skypiax_restart_monitor(void);
void *skypiax_do_monitor(void *data);
int skypiax_sound_boost(struct ast_frame *f, double boost);
int skypiax_sound_init(struct skypiax_pvt *p);
int skypiax_sound_shutdown(struct skypiax_pvt *p);
struct ast_frame *skypiax_sound_read(struct skypiax_pvt *p);
int skypiax_sound_write(struct skypiax_pvt *p, struct ast_frame *f);
void *skypiax_do_controldev_thread(void *data);
#ifdef ASTERISK_VERSION_1_6
void skypiax_store_boost(const char *s, double *boost);
#else
void skypiax_store_boost(char *s, double *boost);
#endif /* ASTERISK_VERSION_1_6 */
int skypiax_console_set_active(int fd, int argc, char *argv[]);
int skypiax_console_hangup(int fd, int argc, char *argv[]);
int skypiax_console_playback_boost(int fd, int argc, char *argv[]);
int skypiax_console_capture_boost(int fd, int argc, char *argv[]);
int skypiax_console_skypiax(int fd, int argc, char *argv[]);
int skypiax_console_dial(int fd, int argc, char *argv[]);
int skypiax_audio_init(struct skypiax_pvt *p);
//struct ast_frame *skypiax_audio_read(struct skypiax_pvt *p);
int skypiax_audio_read(struct skypiax_pvt *p);
void *skypiax_do_tcp_srv_thread(void *data);
int skypiax_audio_write(struct skypiax_pvt *p, struct ast_frame *f);
void *skypiax_do_tcp_cli_thread(void *data);
int skypiax_call(struct skypiax_pvt *p, char *idest, int timeout);
int skypiax_console_skypiax_dir_import(int fd, int argc, char *argv[]);

void *skypiax_do_tcp_srv_thread_func(void *obj);
void *skypiax_do_tcp_cli_thread_func(void *obj);
void *skypiax_do_skypeapi_thread_func(void *obj);
int dtmf_received(private_t * tech_pvt, char *value);
int start_audio_threads(private_t * tech_pvt);
int new_inbound_channel(private_t * tech_pvt);
int outbound_channel_answered(private_t * tech_pvt);
int skypiax_senddigit(struct skypiax_pvt *p, char digit);
int skypiax_signaling_write(private_t * tech_pvt, char *msg_to_skype);
#if defined(WIN32) && !defined(__CYGWIN__)
int skypiax_pipe_read(switch_file_t * pipe, short *buf, int howmany);
int skypiax_pipe_write(switch_file_t * pipe, short *buf, int howmany);
/* Visual C do not have strsep ? */
char *strsep(char **stringp, const char *delim);
#else
int skypiax_pipe_read(int pipe, short *buf, int howmany);
int skypiax_pipe_write(int pipe, short *buf, int howmany);
#endif /* WIN32 */
int skypiax_close_socket(unsigned int fd);
private_t *find_available_skypiax_interface(void);
int remote_party_is_ringing(private_t * tech_pvt);
int remote_party_is_early_media(private_t * tech_pvt);
#define		SKYPIAX_STATE_DOWN		AST_STATE_DOWN
#define		SKYPIAX_STATE_RING		AST_STATE_RING
#define		SKYPIAX_STATE_DIALING	AST_STATE_DIALING
#define		SKYPIAX_STATE_BUSY		AST_STATE_BUSY
#define		SKYPIAX_STATE_UP		AST_STATE_UP
#define		SKYPIAX_STATE_RINGING	AST_STATE_RINGING
#define		SKYPIAX_STATE_PRERING	AST_STATE_PRERING
#define		SKYPIAX_STATE_RESERVED	AST_STATE_RESERVED
#define 	SKYPIAX_STATE_HANGUP_REQUESTED   200
#endif /* _SKYPIAX_H_ */
