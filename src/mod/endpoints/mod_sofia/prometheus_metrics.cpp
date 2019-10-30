#include <map>
#include <string>
#include "prometheus_metrics.h"


class prometheus_metrics
{
public:
	typedef std::map<int, ssize_t> terminated_counter;
	typedef std::map<std::string, ssize_t> request_counter;
	class auto_lock
	{
	public:
		auto_lock(switch_mutex_t* mutex) : _mutex(mutex)
		{
			switch_mutex_lock(_mutex);
		}
		~auto_lock()
		{
			switch_mutex_unlock(_mutex);
		}
		switch_mutex_t* _mutex;
	};
	
	prometheus_metrics(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t* pool) :
		_pool(pool),
		_module_interface(module_interface),
		_call_counter(0),
		_outgoing_invite_couter(0)
	{
		switch_mutex_init(&_mutex, SWITCH_MUTEX_NESTED, _pool);
	}
	
	~prometheus_metrics()
	{
		switch_mutex_destroy(_mutex);
	}
	
	void increment_call_counter()
	{
		auto_lock lock(_mutex);
		++_call_counter;
	}

	void increment_terminated_counter(int status)
	{
		auto_lock lock(_mutex);
		terminated_counter::iterator found = _terminated_counter.find(status);
		if (found != _terminated_counter.end()) {
			++found->second;
		} else {
			_terminated_counter[status] = 1;
		}
		
	}
	
	void increment_request_method(const char* method)
	{
		auto_lock lock(_mutex);
		request_counter::iterator found = _request_counter.find(method);
		if (found != _request_counter.end()) {
			++found->second;
		} else {
			_request_counter[method] = 1;
		}
	}
	
	void increment_outgoing_invite()
	{
		auto_lock lock(_mutex);
		++_outgoing_invite_couter;
	}
	
	void generate_metrics(switch_stream_handle_t *stream)
	{
		auto_lock lock(_mutex);
		stream->write_function(stream, "# HELP sofia_connected_calls Sofia incoming call count\n");
		stream->write_function(stream, "# TYPE sofia_connected_calls counter\n");
		stream->write_function(stream, "sofia_connected_calls %u\n", _call_counter);
		
		stream->write_function(stream, "# HELP sofia_outgoing_invite Sofia outgoing INVITE count\n");
		stream->write_function(stream, "# TYPE sofia_outgoing_invite counter\n");
		stream->write_function(stream, "sofia_outgoing_invite %u\n", _outgoing_invite_couter);
		
		bool write_header = true;
		for (terminated_counter::iterator iter = _terminated_counter.begin(); iter != _terminated_counter.end(); iter++) {
			if (write_header) {
				stream->write_function(stream, "# HELP sofia_rejected_calls Sofia incoming rejected call counter\n");
				stream->write_function(stream, "# TYPE sofia_rejected_calls counter\n");
				write_header = false;
			}
			stream->write_function(stream, "sofia_rejected_calls{status=\"%i\"} %u\n", iter->first, iter->second);
		}
		
		write_header = true;
		for (request_counter::iterator iter = _request_counter.begin(); iter != _request_counter.end(); iter++) {
			if (write_header) {
				stream->write_function(stream, "# HELP sofia_request_counter Sofia incoming requests counter\n");
				stream->write_function(stream, "# TYPE sofia_request_counter counter\n");
				write_header = false;
			}
			stream->write_function(stream, "sofia_request_counter{method=\"%s\"} %u\n", iter->first.c_str(), iter->second);
		}
	}
	

private:
	switch_memory_pool_t* _pool;
	switch_loadable_module_interface_t **_module_interface;
	switch_mutex_t* _mutex;
	terminated_counter _terminated_counter;
	ssize_t _call_counter;
	ssize_t _outgoing_invite_couter;
	request_counter _request_counter;
};

static prometheus_metrics* instance = 0;


SWITCH_STANDARD_API(xml_rpc_prometheus_metrics)
{
	instance->generate_metrics(stream);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_BEGIN_EXTERN_C

void prometheus_init(switch_loadable_module_interface_t **module_interface, switch_api_interface_t *api_interface, switch_memory_pool_t* pool)
{
	SWITCH_ADD_API(api_interface, "sofia_prometheus_metrics", "sofia_prometheus_metrics", xml_rpc_prometheus_metrics, "");
	
	delete instance;
	instance = new prometheus_metrics(module_interface, pool);
}

void prometheus_destroy()
{
	delete instance;
	instance = 0;
}

void prometheus_increment_call_counter()
{
	instance->increment_call_counter();
}

void prometheus_increment_terminated_counter(int status)
{
	instance->increment_terminated_counter(status);
}

void prometheus_increment_request_method(const char* method)
{
	instance->increment_request_method(method);
}

void prometheus_increment_outgoing_invite()
{
	instance->increment_outgoing_invite();
}


SWITCH_END_EXTERN_C


