#include <switch.h>
#include <zmq.hpp>
#include <exception>
#include <stdexcept>
#include <memory>

#include "mod_event_zmq.h"

namespace mod_event_zmq {

// Handles publishing events out to clients
class ZmqEventPublisher {
public:
	ZmqEventPublisher(zmq::context_t &context) :
		_publisher(context, ZMQ_PUB)
	{
		_publisher.bind("tcp://*:5556");

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Listening for clients\n");
	}

	void PublishEvent(const switch_event_t *event) {
		// Serialize the event into a JSON string
		char* pjson;
		switch_event_serialize_json(const_cast<switch_event_t*>(event), &pjson);

		// Use the JSON string as the message body
		zmq::message_t msg(pjson, strlen(pjson), free_message_data, NULL);
		
		// Send the message
		_publisher.send(msg);
	}

private:
	static void free_message_data(void *data, void *hint) {
		free (data);
	}

	zmq::socket_t _publisher;
};

class char_msg : public zmq::message_t {
public:
	char_msg() : zmq::message_t(sizeof(char)) { }
	char_msg(char data) : zmq::message_t(sizeof(char)) {
		*char_data() = data;
	}

	char* char_data() {
		return static_cast<char*>(this->data());
	}
};

// Handles global inititalization and teardown of the module
class ZmqModule {
public:
	ZmqModule(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool) :
		_context(1), _term_rep(_context, ZMQ_REP), _term_req(_context, ZMQ_REQ), _publisher(_context) {

		// Set up the term messaging connection
		_term_rep.bind(TERM_URI);
		_term_req.connect(TERM_URI);

		// Subscribe to all switch events of any subclass
		// Store a pointer to ourself in the user data
		if (switch_event_bind_removable(modname, SWITCH_EVENT_ALL, SWITCH_EVENT_SUBCLASS_ANY, event_handler, static_cast<void*>(&_publisher), &_node)
				!= SWITCH_STATUS_SUCCESS) {
			throw std::runtime_error("Couldn't bind to switch events.");
		}
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Subscribed to events\n");

		// Create our module interface registration
		*module_interface = switch_loadable_module_create_module_interface(pool, modname);

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Module loaded\n");
	}

	void Listen() {
		// All we do is sit here and block the run loop thread so it doesn't return
		// it seems that if you want to keep your module running you can't return from the run loop
		
		char_msg msg;
		while(true) {
			// Listen for term message
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Entered run loop, waiting for term message\n");
			_term_rep.recv(&msg);
			if(*msg.char_data() == MODULE_TERM_REQ_MESSAGE) {
				// Ack term message
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Got term message, sending ack and leaving run loop\n");

				*msg.char_data() = MODULE_TERM_ACK_MESSAGE;
				_term_rep.send(msg);

				break;
			}
		}
	}

	void Shutdown() {
		// Send term message
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Shutdown requested, sending term message to runloop\n");
		char_msg msg(MODULE_TERM_REQ_MESSAGE);
		_term_req.send(msg);

		while(true) {
			// Wait for the term ack message
			_term_req.recv(&msg);
			if(*msg.char_data() == MODULE_TERM_ACK_MESSAGE) {
				// Continue shutdown
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Got term ack message, continuing shutdown\n");
				break;
			}
		}
	}

	~ZmqModule() {
		// Unsubscribe from the switch events
		switch_event_unbind(&_node);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Module shut down\n");
	}

private:
	// Dispatches events to the publisher
	static void event_handler(switch_event_t *event) {
		try {
			ZmqEventPublisher *publisher = static_cast<ZmqEventPublisher*>(event->bind_user_data);
			publisher->PublishEvent(event);
		} catch(std::exception ex) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Error publishing event via 0MQ: %s\n", ex.what());
		} catch(...) { // Exceptions must not propogate to C caller
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Unknown error publishing event via 0MQ\n");
		}
	}

	switch_event_node_t *_node;

	zmq::context_t _context;
	zmq::socket_t _term_rep;
	zmq::socket_t _term_req;

	ZmqEventPublisher _publisher;
};

//*****************************//
//           GLOBALS           //
//*****************************//
std::auto_ptr<ZmqModule> module;


//*****************************//
//  Module interface funtions  //
//*****************************//
SWITCH_MODULE_LOAD_FUNCTION(load) {
	try {
		module.reset(new ZmqModule(module_interface, pool));
		return SWITCH_STATUS_SUCCESS;
	} catch(...) { // Exceptions must not propogate to C caller
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error loading 0MQ module\n");
		return SWITCH_STATUS_GENERR;
	}

}

SWITCH_MODULE_RUNTIME_FUNCTION(runtime) {
	try {
		// Begin listening for clients
		module->Listen();
	} catch(std::exception &ex) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error listening for clients: %s\n", ex.what());
	} catch(...) { // Exceptions must not propogate to C caller
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unknown error listening for clients\n");
	}

	// Tell the switch to stop calling this runtime loop
	return SWITCH_STATUS_TERM;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(shutdown) {
	try {
		// Tell the module to shutdown
		module->Shutdown();

		// Free the module object
		module.reset();
	} catch(std::exception &ex) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error shutting down module: %s\n", ex.what());
	} catch(...) { // Exceptions must not propogate to C caller
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unknown error shutting down module\n");
	}
	return SWITCH_STATUS_SUCCESS;
}

}
