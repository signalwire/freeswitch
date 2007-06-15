#ifndef SWITCH_CPP_H
#define SWITCH_CPP_H


#ifdef __cplusplus
extern "C" {
#endif
#ifdef DOH
}
#endif

#include <switch.h>


void console_log(char *level_str, char *msg);
void console_clean_log(char *msg);
char *api_execute(char *cmd, char *arg);
void api_reply_delete(char *reply);
switch_status_t process_callback_result(char *raw_result,
										struct input_callback_state *cb_state,
										switch_core_session_t *session);


typedef struct input_callback_state {
    void *function;           // pointer to the language specific callback function
                              // eg, PyObject *pyfunc
    void *threadState;        // pointer to the language specific thread state
                              // eg, PyThreadState *threadState
    void *extra;              // currently used to store a switch_file_handle_t
    char *funcargs;           // extra string that will be passed to callback function 
};


class CoreSession {
 protected:
	switch_input_args_t args;
	switch_input_args_t *ap;
	char *uuid;
	char *tts_name;
	char *voice_name;
	void store_file_handle(switch_file_handle_t *fh);
 public:
	CoreSession(char *uuid);
	CoreSession(switch_core_session_t *new_session);
	virtual ~CoreSession();
	switch_core_session_t *session;
	switch_channel_t *channel;
	input_callback_state cb_state;
	int answer();
	int preAnswer();
	void hangup(char *cause);
	void setVariable(char *var, char *val);
	char *getVariable(char *var);

	/** \brief Play a file that resides on disk into the channel
	 *
	 * \param file - the path to the .wav/.mp3 to be played
	 * \param timer_name - ?? does not seem to be used, what is this?
	 * \return an int status code indicating success or failure
	 *
	 * NOTE: if a dtmf callback is installed before calling this 
     *       function, that callback will be called upon receiving any
     *       dtmfs 
	 */
	int playFile(char *file, char *timer_name);


	/** \brief set a DTMF callback function
     * 
     * The DTMF callback function will be set and persist
     * for the life of the session, and be called when a dtmf
     * is pressed by user during playFile(), streamfile(), and 
     * certain other methods are executing.
     *
     * Note that language specific sessions might need to create
     * their own version of this with a slightly different signature
     * (as done in freeswitch_python.h)
	 */
	void setDTMFCallback(switch_input_callback_function_t cb, 
						 void *buf, 
						 uint32_t buflen);

	int speak(char *text);
	void set_tts_parms(char *tts_name, char *voice_name);

	int getDigits(char *dtmf_buf, 
				  int len, 
				  char *terminators, 
				  char *terminator, 
				  int timeout);

	int transfer(char *extensions, char *dialplan, char *context);

	/** \brief Play a file into channel and collect dtmfs
	 * 
     * See API docs in switch_ivr.h: switch_play_and_get_digits(..)
     *
     * NOTE: this does not call any dtmf callbacks set by 
     *       setDTMFCallback(..) as it uses its own internal callback
     *       handler.
     */
	int playAndGetDigits(int min_digits, 
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
	int streamfile(char *file, int starting_sample_count);

	bool ready();

	void execute(char *app, char *data);
	virtual void begin_allow_threads();
	virtual void end_allow_threads();


};


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

