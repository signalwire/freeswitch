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
		_rx_success(0),
		_rx_failure(0),
		_tx_success(0),
		_tx_failure(0),
		_gateway_success(0),
		_gateway_failure(0)
	{
		switch_mutex_init(&_mutex, SWITCH_MUTEX_NESTED, _pool);
	}
	
	~prometheus_metrics()
	{
		switch_mutex_destroy(_mutex);
	}
	
	void increment_tx_fax_success()
	{
		auto_lock lock(_mutex);
		++_tx_success;
	}

	void increment_tx_fax_failure()
	{
		auto_lock lock(_mutex);
		++_tx_failure;
	}

	void increment_rx_fax_success()
	{
		auto_lock lock(_mutex);
		++_rx_success;
	}

	void increment_rx_fax_failure()
	{
		auto_lock lock(_mutex);
		++_rx_failure;
	}
	
	void increment_gateway_fax_failure()
	{
		auto_lock lock(_mutex);
		++_gateway_failure;
	}

	void increment_gateway_fax_success()
	{
		auto_lock lock(_mutex);
		++_gateway_success;
	}
	
	void generate_metrics(switch_stream_handle_t *stream)
	{
		auto_lock lock(_mutex);
		
		stream->write_function(stream, "# HELP spandsp_fax_incoming Incoming fax count\n");
		stream->write_function(stream, "# TYPE spandsp_fax_incoming counter\n");
		stream->write_function(stream, "spandsp_fax_incoming{status=\"success\"} %u\n", _rx_success);
		stream->write_function(stream, "spandsp_fax_incoming{status=\"failed\"} %u\n", _rx_failure);
		
		stream->write_function(stream, "# HELP spandsp_fax_outgoing Outgoing fax count\n");
		stream->write_function(stream, "# TYPE spandsp_fax_outgoing counter\n");
		stream->write_function(stream, "spandsp_fax_outgoing{status=\"success\"} %u\n", _tx_success);
		stream->write_function(stream, "spandsp_fax_outgoing{status=\"failed\"} %u\n", _tx_failure);
		
		stream->write_function(stream, "# HELP spandsp_fax_gateway Gateway fax count\n");
		stream->write_function(stream, "# TYPE spandsp_fax_gateway counter\n");
		stream->write_function(stream, "spandsp_fax_gateway{status=\"success\"} %u\n", _gateway_success);
		stream->write_function(stream, "spandsp_fax_gateway{status=\"failed\"} %u\n", _gateway_failure);
	}
	

private:
	switch_memory_pool_t* _pool;
	switch_loadable_module_interface_t **_module_interface;
	switch_mutex_t* _mutex;

	ssize_t _rx_success;
	ssize_t _rx_failure;
	ssize_t _tx_success;
	ssize_t _tx_failure;
	ssize_t _gateway_success;
	ssize_t _gateway_failure;
	
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
	SWITCH_ADD_API(api_interface, "spandsp_prometheus_metrics", "spandsp_prometheus_metrics", xml_rpc_prometheus_metrics, "");
	
	delete instance;
	instance = new prometheus_metrics(module_interface, pool);
}

void prometheus_destroy()
{
	delete instance;
	instance = 0;
}

void prometheus_increment_tx_fax_success()
{
	instance->increment_tx_fax_success();
}

void prometheus_increment_tx_fax_failure()
{
	instance->increment_tx_fax_failure();
}

void prometheus_increment_rx_fax_success()
{
	instance->increment_rx_fax_success();
}

void prometheus_increment_rx_fax_failure()
{
	instance->increment_rx_fax_failure();
}

void prometheus_increment_gateway_fax_failure()
{
	instance->increment_gateway_fax_failure();
}

void prometheus_increment_gateway_fax_success()
{
	instance->increment_gateway_fax_success();
}


SWITCH_END_EXTERN_C


