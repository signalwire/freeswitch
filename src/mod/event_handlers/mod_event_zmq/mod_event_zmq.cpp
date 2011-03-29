#include <switch.h>
#include <zmq.hpp>
#include <exception>
#include <stdexcept>
#include <memory>

namespace mod_event_zmq {

SWITCH_MODULE_LOAD_FUNCTION(load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(shutdown);
SWITCH_MODULE_RUNTIME_FUNCTION(runtime);

extern "C" {
SWITCH_MODULE_DEFINITION(mod_event_zmq, load, shutdown, runtime);
};

// Handles publishing events out to clients
class ZmqEventPublisher {
public:
	ZmqEventPublisher() :
		context(1),
		event_publisher(context, ZMQ_PUB)
	{
		event_publisher.bind("tcp://*:5556");

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Listening for clients\n");
	}

	void PublishEvent(const switch_event_t *event) {
		// Serialize the event into a JSON string
		char* pjson;
		switch_event_serialize_json(const_cast<switch_event_t*>(event), &pjson);

		// Use the JSON string as the message body
		zmq::message_t msg(pjson, strlen(pjson), free_message_data, NULL);
		
		// Send the message
		event_publisher.send(msg);
	}

private:
	static void free_message_data(void *data, void *hint) {
		free (data);
	}

	zmq::context_t context;
	zmq::socket_t event_publisher;
};

// Handles global inititalization and teardown of the module
class ZmqModule {
public:
	ZmqModule(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool) :
		_running(false) {
		// Subscribe to all switch events of any subclass
		// Store a pointer to ourself in the user data
		if (switch_event_bind_removable(modname, SWITCH_EVENT_ALL, SWITCH_EVENT_SUBCLASS_ANY, event_handler, (void*)this, &_node)
				!= SWITCH_STATUS_SUCCESS) {
			throw std::runtime_error("Couldn't bind to switch events.");
		}
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Subscribed to events\n");

		// Create our module interface registration
		*module_interface = switch_loadable_module_create_module_interface(pool, modname);

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Module loaded\n");
	}

	void Listen() {
		if(_running)
			return;

		_publisher.reset(new ZmqEventPublisher());
		_running = true;

		while(_running) {
			switch_yield(100000);
		}
	}

	~ZmqModule() {
		// Unsubscribe from the switch events
		_running = false;
		switch_event_unbind(&_node);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Module shut down\n");
	}

private:
	// Dispatches events to the publisher
	static void event_handler(switch_event_t *event) {
		try {
			ZmqModule *module = (ZmqModule*)event->bind_user_data;
			if(module->_publisher.get())
				module->_publisher->PublishEvent(event);
		} catch(std::exception ex) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Error publishing event via 0MQ: %s\n", ex.what());
		} catch(...) { // Exceptions must not propogate to C caller
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Unknown error publishing event via 0MQ\n");
		}
	}

	switch_event_node_t *_node;
	std::auto_ptr<ZmqEventPublisher> _publisher;
	bool _running;
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
		// Free the module object
		module.reset();
	} catch(...) { // Exceptions must not propogate to C caller
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error shutting down module\n");
	}
}

}
