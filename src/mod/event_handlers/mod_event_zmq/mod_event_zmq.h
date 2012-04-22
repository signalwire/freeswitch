#ifndef MOD_EVENT_ZMQ_H
#define MOD_EVENT_ZMQ_H

namespace mod_event_zmq {
static const char MODULE_TERM_REQ_MESSAGE = 1;
static const char MODULE_TERM_ACK_MESSAGE = 2;

static const char *TERM_URI = "inproc://mod_event_zmq_term";

SWITCH_MODULE_LOAD_FUNCTION(load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(shutdown);
SWITCH_MODULE_RUNTIME_FUNCTION(runtime);

extern "C" {
SWITCH_MODULE_DEFINITION(mod_event_zmq, load, shutdown, runtime);
};

}

#endif // MOD_EVENT_ZMQ_H
