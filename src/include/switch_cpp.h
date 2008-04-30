#ifndef SWITCH_CPP_H
#define SWITCH_CPP_H


#ifdef __cplusplus
extern "C" {
#endif
#ifdef DOH
}
#endif

#include <switch.h>

#define sanity_check(x) do { if (!(session && allocated)) { switch_log_printf(SWITCH_CHANNEL_LOG,SWITCH_LOG_ERROR, "session is not initalized\n"); return x;}} while(0)
#define sanity_check_noreturn do { if (!(session && allocated)) { switch_log_printf(SWITCH_CHANNEL_LOG,SWITCH_LOG_ERROR, "session is not initalized\n"); return;}} while(0)
#define init_vars() do { session = NULL; channel = NULL; uuid = NULL; tts_name = NULL; voice_name = NULL; memset(&args, 0, sizeof(args)); ap = NULL; caller_profile.source = "mod_unknown";  caller_profile.dialplan = ""; caller_profile.context = ""; caller_profile.caller_id_name = ""; caller_profile.caller_id_number = ""; caller_profile.network_addr = ""; caller_profile.ani = ""; caller_profile.aniii = ""; caller_profile.rdnis = "";  caller_profile.username = ""; on_hangup = NULL; memset(&cb_state, 0, sizeof(cb_state)); hook_state = CS_NEW; } while(0)


//
// C++ Interface: switch_to_cpp_mempool
//
// Description: This class allows for overloading the new operator to allocate from a switch_memory_pool_t
//
// Author: Yossi Neiman <freeswitch@cartissolutions.com>, (C) 2007
//
// Copyright: See COPYING file that comes with this distribution
//


#ifndef SWITCHTOMEMPOOL
#define SWITCHTOMEMPOOL
class SwitchToMempool {
	public:
		SwitchToMempool() { }
		SwitchToMempool(switch_memory_pool_t *mem) { memorypool = mem; }
		void *operator new(switch_size_t num_bytes, switch_memory_pool_t *mem)
		{
			void *ptr = switch_core_alloc(mem, (switch_size_t) num_bytes);
			return ptr;
		}
	protected:
		switch_memory_pool_t *memorypool;
};
#endif


/*

Overview: once you create an object that inherits this class, since
          the memory pool is then a class data member, you can continue to
          allocate objects from the memory pool.
          objects from within the class

Notes on usage:

1.  The derived class will need to also overload the ctor so that it accepts a memory pool object as a parameter.
2.  Instantiation of a class would then look something like this:   Foo *bar = new(memory_pool) Foo(memory_pool);

Note that the first parameter to the new operator is implicitly handled by c++...  not sure I like that but it's how it is...

*/


typedef struct input_callback_state {
    void *function;           // pointer to the language specific callback function
                              // eg, PyObject *pyfunc
    void *threadState;        // pointer to the language specific thread state
                              // eg, PyThreadState *threadState
    void *extra;              // currently used to store a switch_file_handle_t
    char *funcargs;           // extra string that will be passed to callback function 
} input_callback_state_t;

typedef enum {
	S_HUP = (1 << 0),
	S_FREE = (1 << 1),
	S_RDLOCK = (1 << 2)
} session_flag_t;

class Stream {
 protected: 
	switch_stream_handle_t mystream;
	switch_stream_handle_t *stream_p;
	int mine;
 public: 
	SWITCH_DECLARE_CONSTRUCTOR Stream(void);
	SWITCH_DECLARE_CONSTRUCTOR Stream(switch_stream_handle_t *);
	virtual SWITCH_DECLARE_CONSTRUCTOR ~Stream();
	SWITCH_DECLARE(void) write(const char *data);	
	SWITCH_DECLARE(const char *)get_data(void);
};

class Event {
 protected:
 public:
	switch_event_t *event;
	char *serialized_string;
	int mine;

	SWITCH_DECLARE_CONSTRUCTOR Event(const char *type, const char *subclass_name = NULL);
	SWITCH_DECLARE_CONSTRUCTOR Event(switch_event_t *wrap_me, int free_me=0);
	virtual SWITCH_DECLARE_CONSTRUCTOR ~Event();
	SWITCH_DECLARE(const char *)serialize(const char *format=NULL);
	SWITCH_DECLARE(bool) set_priority(switch_priority_t priority = SWITCH_PRIORITY_NORMAL);
	SWITCH_DECLARE(char *)get_header(char *header_name);
	SWITCH_DECLARE(char *)get_body(void);
	SWITCH_DECLARE(bool) add_body(const char *value);
	SWITCH_DECLARE(bool) add_header(const char *header_name, const char *value);
	SWITCH_DECLARE(bool) del_header(const char *header_name);
	SWITCH_DECLARE(bool) fire(void);
};


class CoreSession {
 protected:
	switch_input_args_t args; // holds ptr to cb function and input_callback_state struct
                              // which has a language specific callback function
	switch_input_args_t *ap;  // ptr to args .. (is this really needed?)
	switch_caller_profile_t caller_profile; // avoid passing so many args to originate, 
	                                        // instead set them here first
	char *uuid;
	char *tts_name;
	char *voice_name;
	void store_file_handle(switch_file_handle_t *fh);
	void *on_hangup; // language specific callback function, cast as void * 
	switch_file_handle_t local_fh;
	switch_file_handle_t *fhp;
	SWITCH_DECLARE(switch_status_t) process_callback_result(char *ret);
 public:
	SWITCH_DECLARE_CONSTRUCTOR CoreSession();
	SWITCH_DECLARE_CONSTRUCTOR CoreSession(char *uuid);
	SWITCH_DECLARE_CONSTRUCTOR CoreSession(switch_core_session_t *new_session);
	virtual SWITCH_DECLARE_CONSTRUCTOR ~CoreSession();
	switch_core_session_t *session;
	switch_channel_t *channel;
	unsigned int flags;
	int allocated;
	input_callback_state cb_state; // callback state, always pointed to by the buf
                                   // field in this->args
	switch_channel_state_t hook_state; // store hookstate for on_hangup callback

	SWITCH_DECLARE(int) answer();
	SWITCH_DECLARE(int) preAnswer();
	virtual SWITCH_DECLARE(void) hangup(char *cause = "normal_clearing");
	SWITCH_DECLARE(void) setVariable(char *var, char *val);
	SWITCH_DECLARE(void) setPrivate(char *var, void *val);
	SWITCH_DECLARE(void *)getPrivate(char *var);
	SWITCH_DECLARE(const char *)getVariable(char *var);
	

	/** \brief Record to a file
	 * \param filename 
	 * \param <[max_len]> maximum length of the recording in seconds
     * \param <[silence_threshold]> energy level audio must fall below 
	 *        to be considered silence (500 is a good starting point).
	 * \param <[silence_secs]> seconds of silence to interrupt the record.
	 */
	SWITCH_DECLARE(int) recordFile(char *file_name, int max_len=0, int silence_threshold=0, int silence_secs=0);


	/** \brief Set attributes of caller data for purposes of outgoing calls
	 * \param var - the variable name, eg, "caller_id_name"
	 * \param val - the data to set, eg, "bob"
	 */
	SWITCH_DECLARE(void) setCallerData(char *var, char *val);

	/** \brief Originate a call to a destination
	 *
	 * \param a_leg_sessoin - the session where the call is originating from 
     *                        and also the session in which _this_ session was 
     *                        created
	 * \param dest - a string representing destination, eg, sofia/mydomain.com/foo@bar.com
	 * \return an int status code indicating success or failure
     *
	 */
	SWITCH_DECLARE(int) originate(CoreSession *a_leg_session, 
				  char *dest, 
				  int timeout=60);


	/** \brief set a DTMF callback function
     * 
     * The DTMF callback function will be set and persist
     * for the life of the session, and be called when a dtmf
     * is pressed by user during streamfile(), collectDigits(), and 
     * certain other methods are executing.
     *
	 */
	SWITCH_DECLARE(void) setDTMFCallback(void *cbfunc, char *funcargs);

	SWITCH_DECLARE(int) speak(char *text);
	SWITCH_DECLARE(void) set_tts_parms(char *tts_name, char *voice_name);

	/**
     * For timeout milliseconds, call the dtmf function set previously
	 * by setDTMFCallback whenever a dtmf or event is received
	 */
	SWITCH_DECLARE(int) collectDigits(int timeout);

	/** 
     * Collect up to maxdigits digits worth of digits
     * and store them in dtmf_buf.  In the case of mod_python, the 
	 * dtmf_buf parameter is configured to act as a _return_ value,
	 * (see mod_python.i).  This does NOT call any callbacks upon
	 * receiving dtmf digits.  For that, use collectDigits.
	 */
	SWITCH_DECLARE(int) getDigits(char *dtmf_buf, 
				  switch_size_t buflen, 
				  switch_size_t maxdigits, 
				  char *terminators, 
				  char *terminator, 
				  int timeout);

	SWITCH_DECLARE(int) transfer(char *extensions, char *dialplan, char *context);

	/** \brief Play a file into channel and collect dtmfs
	 * 
     * See API docs in switch_ivr.h: switch_play_and_get_digits(..)
     *
     * NOTE: this does not call any dtmf callbacks set by 
     *       setDTMFCallback(..) as it uses its own internal callback
     *       handler.
     */
	SWITCH_DECLARE(int) playAndGetDigits(int min_digits, 
						 int max_digits, 
						 int max_tries, 
						 int timeout, 
						 char *terminators,
						 char *audio_files, 
						 char *bad_input_audio_files, 
						 char *dtmf_buf, 
						 char *digits_regex);

	/** \brief Play a file that resides on disk into the channel
	 *
	 * \param file - the path to the .wav/.mp3 to be played
	 * \param starting_sample_count - the index of the sample to 
     *                                start playing from
	 * \return an int status code indicating success or failure
	 *
	 */
	SWITCH_DECLARE(int) streamFile(char *file, int starting_sample_count=0);

	/** \brief flush any pending events
	 */
	SWITCH_DECLARE(int) flushEvents();

	/** \brief flush any pending digits
	 */
	SWITCH_DECLARE(int) flushDigits();

	SWITCH_DECLARE(int) setAutoHangup(bool val);

	/** \brief Set the hangup callback function
	 * \param hangup_func - language specific function ptr cast into void *
	 */
	SWITCH_DECLARE(void) setHangupHook(void *hangup_func);

	SWITCH_DECLARE(bool) ready();

	SWITCH_DECLARE(void) execute(char *app, char *data);

	SWITCH_DECLARE(void) sendEvent(Event *sendME);

	virtual bool begin_allow_threads() = 0;
	virtual bool end_allow_threads() = 0;

	/** \brief Get the uuid of this session	
	 * \return the uuid of this session
	 */
	char* get_uuid() const { return uuid; };

	/** \brief Get the callback function arguments associated with this session
	 * \return a const reference to the callback function arguments
	 */
	const switch_input_args_t& get_cb_args() const { return args; };

	/** \brief Callback to the language specific hangup callback
	 */
	virtual void check_hangup_hook() = 0;

	virtual switch_status_t run_dtmf_callback(void *input, 
											  switch_input_type_t itype) = 0;

};


/* ---- functions not bound to CoreSession instance ----- */

SWITCH_DECLARE(void) console_log(char *level_str, char *msg);
SWITCH_DECLARE(void) console_clean_log(char *msg);
SWITCH_DECLARE(char *)api_execute(char *cmd, char *arg);
SWITCH_DECLARE(void) api_reply_delete(char *reply);

/** \brief bridge the audio of session_b into session_a
 * 
 * NOTE: the stuff regarding the dtmf callback might be completely
 *       wrong and has not been reviewed or tested
 */
SWITCH_DECLARE(void) bridge(CoreSession &session_a, CoreSession &session_b);


/** \brief the actual hangup hook called back by freeswitch core
 *         which in turn gets the session and calls the appropriate
 *         instance method to complete the callback.
 */
SWITCH_DECLARE_NONSTD(switch_status_t) hanguphook(switch_core_session_t *session);

SWITCH_DECLARE_NONSTD(switch_status_t) dtmf_callback(switch_core_session_t *session, 
							  void *input, 
							  switch_input_type_t itype, 
							  void *buf,  
							  unsigned int buflen);


#ifdef __cplusplus
}
#endif

#endif
/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */

