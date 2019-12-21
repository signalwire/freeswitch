#include <map>
#include <string>
#include "prometheus_metrics.h"


class prometheus_metrics
{
public:
	typedef std::map<std::string, double> api_counter;
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
		_module_interface(module_interface)
	{
		switch_mutex_init(&_mutex, SWITCH_MUTEX_NESTED, _pool);
	}
	
	~prometheus_metrics()
	{
		switch_mutex_destroy(_mutex);
	}
	
	void increment_api_counter(const char* command)
	{
		auto_lock lock(_mutex);
		++_api_calls;
		api_counter::iterator found = _api_counter.find(command);
		if (found != _api_counter.end()) {
			++found->second;
		} else {
			_api_counter[command] = 1;
		}
	}

	void decrement_current_api_call()
	{
		auto_lock lock(_mutex);
		--_api_calls;
	}
	
	void generate_metrics(switch_stream_handle_t *stream)
	{
		auto_lock lock(_mutex);
		bool write_header = true;
		for (api_counter::iterator iter = _api_counter.begin(); iter != _api_counter.end(); iter++) {
			if (write_header) {
				stream->write_function(stream, "# HELP mod_xml_rpc_api_calls XML RPC API counter\n");
				stream->write_function(stream, "# TYPE mod_xml_rpc_api_calls counter\n");
				write_header = 0;
			}
			stream->write_function(stream, "mod_xml_rpc_api_calls{command=\"%s\"} %i\n", iter->first.c_str(), (int)iter->second);
		}
		
		stream->write_function(stream, "# HELP mod_xml_rpc_current_api_calls XML RPC current API call count\n");
		stream->write_function(stream, "# TYPE mod_xml_rpc_current_api_calls gauge\n");
		stream->write_function(stream, "mod_xml_rpc_current_api_calls %i\n", (int)_api_calls);
	}
	

private:
	switch_memory_pool_t* _pool;
	switch_loadable_module_interface_t **_module_interface;
	switch_mutex_t* _mutex;
	api_counter _api_counter;
	double _api_calls;
};

static prometheus_metrics* instance = 0;


SWITCH_STANDARD_API(xml_rpc_prometheus_metrics)
{
	instance->generate_metrics(stream);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_BEGIN_EXTERN_C

void prometheus_init(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t* pool)
{
	switch_api_interface_t *api_interface;
	SWITCH_ADD_API(api_interface, "xml_rpc_prometheus_metrics", "xml_rpc_prometheus_metrics", xml_rpc_prometheus_metrics, "");
	
	delete instance;
	instance = new prometheus_metrics(module_interface, pool);
}

void prometheus_destroy()
{
	delete instance;
	instance = 0;
}

void prometheus_increment_api_counter(const char* command)
{
	instance->increment_api_counter(command);
}

void prometheus_decrement_current_api_call()
{
	instance->decrement_current_api_call();
}

SWITCH_END_EXTERN_C


