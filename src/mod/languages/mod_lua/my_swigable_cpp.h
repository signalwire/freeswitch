typedef struct input_callback_state {
	void *function;

	void *threadState;

	void *extra;
	char *funcargs;
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
	    Stream(void);
	     Stream(switch_stream_handle_t *);
	                       virtual ~ Stream();
	string read();
	void write(const char *data);
	void raw_write(void *data, int len);
	const char *get_data(void);
};

class Event {
  protected:
  public:
	switch_event_t *event;
	char *serialized_string;
	int mine;

	    Event(const char *type, const char *subclass_name = NULL);
	     Event(switch_event_t *wrap_me, int free_me = 0);
	    virtual ~ Event();
	const char *serialize(const char *format = NULL);
	bool setPriority(switch_priority_t priority = SWITCH_PRIORITY_NORMAL);
	char *getHeader(char *header_name);
	char *getBody(void);
	char *getType(void);
	bool addBody(const char *value);
	bool addHeader(const char *header_name, const char *value);
	bool delHeader(const char *header_name);
	bool fire(void);

};


class Dbh {
  protected:
    switch_cache_db_handle_t *dbh;
    bool connected;
    static int query_callback(void *pArg, int argc, char **argv, char **cargv);
  public:
    Dbh(char *dsn, char *user = NULL, char *pass = NULL);
    ~Dbh();
    bool release();
    bool query(char *sql, SWIGLUA_FN lua_fun);
};


class CoreSession {
  protected:
	switch_input_args_t args;

	switch_input_args_t *ap;
	switch_caller_profile_t caller_profile;

	char *uuid;
	char *tts_name;
	char *voice_name;
	void store_file_handle(switch_file_handle_t *fh);
	void *on_hangup;
	switch_file_handle_t local_fh;
	switch_file_handle_t *fhp;
	switch_status_t process_callback_result(char *ret);
  public:
	     CoreSession();
	     CoreSession(char *uuid);
	     CoreSession(switch_core_session_t *new_session);
	                      virtual ~ CoreSession();
	switch_core_session_t *session;
	switch_channel_t *channel;
	unsigned int flags;
	int allocated;
	input_callback_state cb_state;

	switch_channel_state_t hook_state;

	int answer();
	int preAnswer();
	virtual void hangup(char *cause = "normal_clearing");
	void setVariable(char *var, char *val);
	void setPrivate(char *var, void *val);
	void *getPrivate(char *var);
	const char *getVariable(char *var);
	int recordFile(char *file_name, int max_len = 0, int silence_threshold = 0, int silence_secs = 0);






	void setCallerData(char *var, char *val);
	int originate(CoreSession * a_leg_session, char *dest, int timeout = 60);
	void setDTMFCallback(void *cbfunc, char *funcargs);

	int speak(char *text);
	void set_tts_parms(char *tts_name, char *voice_name);





	int collectDigits(int timeout);
	int getDigits(char *dtmf_buf, switch_size_t buflen, switch_size_t maxdigits, char *terminators, char *terminator, int timeout);

	int transfer(char *extensions, char *dialplan, char *context);
	int playAndGetDigits(int min_digits,
						 int max_digits,
						 int max_tries,
						 int timeout, char *terminators, char *audio_files, char *bad_input_audio_files, char *dtmf_buf, char *digits_regex);
	int streamFile(char *file, int starting_sample_count = 0);



	int flushEvents();



	int flushDigits();

	int setAutoHangup(bool val);




	void setHangupHook(void *hangup_func);

	bool ready();

	void execute(char *app, char *data);

	void sendEvent(Event * sendME);

	virtual bool begin_allow_threads() = 0;
	virtual bool end_allow_threads() = 0;




	char *get_uuid() const {
		return uuid;
	};




	const switch_input_args_t &get_cb_args() const {
		return args;
	};



	virtual void check_hangup_hook() = 0;

	virtual switch_status_t run_dtmf_callback(void *input, switch_input_type_t itype) = 0;

};




void console_log(char *level_str, char *msg);
void console_clean_log(char *msg);
char *api_execute(char *cmd, char *arg);
void api_reply_delete(char *reply);






void bridge(CoreSession & session_a, CoreSession & session_b);






switch_status_t hanguphook(switch_core_session_t *session);

switch_status_t dtmf_callback(switch_core_session_t *session, void *input, switch_input_type_t itype, void *buf, unsigned int buflen);
