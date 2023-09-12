#include "prometheus_metrics.h"


class prometheus_metrics
{
public:
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
		_asr_counter(0),
		_asr_success(0),
		_asr_failure(0),
		_mrcp_timeout(0)
	{
		switch_mutex_init(&_mutex, SWITCH_MUTEX_NESTED, _pool);
	}
	
	~prometheus_metrics()
	{
		switch_mutex_destroy(_mutex);
	}
	
	void increment_asr_counter()
	{
		auto_lock lock(_mutex);
		++_asr_counter;
	}

	void increment_asr_success()
	{
		auto_lock lock(_mutex);
		++_asr_success;
	}

	void increment_asr_failure()
	{
		auto_lock lock(_mutex);
		++_asr_failure;
	}

	void increment_mrcp_timeout()
	{
		auto_lock lock(_mutex);
		++_mrcp_timeout;
	}
	
	void generate_metrics(switch_stream_handle_t *stream)
	{
		auto_lock lock(_mutex);
		stream->write_function(stream, "# HELP unimrcp_asr_counter Speech Recognition Requests count\n");
		stream->write_function(stream, "# TYPE unimrcp_asr_counter counter\n");
		stream->write_function(stream, "unimrcp_asr_counter %u\n", _asr_counter);

		stream->write_function(stream, "# HELP unimrcp_asr_success Speech Successfully Recognized count\n");
		stream->write_function(stream, "# TYPE unimrcp_asr_success counter\n");
		stream->write_function(stream, "unimrcp_asr_success %u\n", _asr_success);

		stream->write_function(stream, "# HELP unimrcp_asr_failure Speech Recognition Failures count\n");
		stream->write_function(stream, "# TYPE unimrcp_asr_failure counter\n");
		stream->write_function(stream, "unimrcp_asr_failure %u\n", _asr_failure);
		
		stream->write_function(stream, "# HELP mrcp_timeout Timeout requests to MRCP server - Lumenvox\n");
		stream->write_function(stream, "# TYPE mrcp_timeout counter\n");
		stream->write_function(stream, "mrcp_timeout %u\n", _mrcp_timeout);
	}
	

private:
	switch_memory_pool_t* _pool;
	switch_loadable_module_interface_t **_module_interface;
	switch_mutex_t* _mutex;
	ssize_t _asr_counter;
	ssize_t _asr_success;
	ssize_t _asr_failure;
	ssize_t _mrcp_timeout;
};

static prometheus_metrics* instance = 0;


SWITCH_STANDARD_API(unimrcp_prometheus_metrics)
{
	instance->generate_metrics(stream);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_BEGIN_EXTERN_C

void prometheus_init(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t* pool)
{
	switch_api_interface_t *api_interface;
	SWITCH_ADD_API(api_interface, "unimrcp_prometheus_metrics", "unimrcp_prometheus_metrics", unimrcp_prometheus_metrics, "");
	
	delete instance;
	instance = new prometheus_metrics(module_interface, pool);
}

void prometheus_destroy()
{
	delete instance;
	instance = 0;
}

void prometheus_increment_asr_counter()
{
	instance->increment_asr_counter();
}

void prometheus_increment_asr_success()
{
	instance->increment_asr_success();
}

void prometheus_increment_asr_failure()
{
	instance->increment_asr_failure();
}

void prometheus_increment_mrcp_timeout()
{
	instance->increment_mrcp_timeout();
}

SWITCH_END_EXTERN_C

