%{
#include "esl.h"
#include "esl_oop.h"
%}

class ESLevent {
 public:
	esl_event_t *event;
	char *serialized_string;
	int mine;
	ESLevent(const char *type, const char *subclass_name = NULL);
	virtual ~ESLevent();
	const char *serialize(const char *format = NULL);
	bool setPriority(esl_priority_t priority = ESL_PRIORITY_NORMAL);
	const char *getHeader(char *header_name);
	char *getBody(void);
	const char *getType(void);
	bool addBody(const char *value);
	bool addHeader(const char *header_name, const char *value);
	bool delHeader(const char *header_name);
};



class ESLconnection {
 private:
	esl_handle_t handle;
	ESLevent *last_event_obj;
 public:
	ESLconnection(const char *host, const char *port, const char *password);
	ESLconnection(int socket);
	virtual ~ESLconnection();
	int connected();
	ESLevent *getInfo();
	esl_status_t send(const char *cmd);
	ESLevent *sendRecv(const char *cmd);
	esl_status_t sendEvent(ESLevent *send_me);
	ESLevent *recvEvent();
	ESLevent *recvEventTimed(int ms);
	esl_status_t filter(const char *header, const char *value);
	esl_status_t events(const char *etype, const char *value);
	esl_status_t execute(const char *app, const char *arg = NULL, const char *uuid = NULL);
	int setBlockingExecute(const char *val);
	int setEventLock(const char *val);
};

void eslSetLogLevel(int level);

