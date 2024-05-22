#include <map>
#include <string>
#include "prometheus_metrics.h"

#define MAX_BUCKET_LEN 8
#define BUCKET_BOUND 0
#define BUCKET_COUNT 1
#define BUCKET_INFINITY UINT_MAX	// not perfect, but good enough

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

	private:
		switch_mutex_t* _mutex;
	};
	
	prometheus_metrics(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t* pool) :
		_pool(pool),
		_module_interface(module_interface),
		_sessions(0),
		_detectors(0)
	{
		switch_mutex_init(&_mutex, SWITCH_MUTEX_NESTED, _pool);
	}
	
	~prometheus_metrics()
	{
		switch_mutex_destroy(_mutex);
	}

	void update_active_sessions(unsigned int sessions)
	{
		auto_lock lock(_mutex);
		_sessions = sessions;
	}

	void update_detectors(unsigned int detectors)
	{
		auto_lock lock(_mutex);
		_detectors = detectors;
	}
		
	void generate_metrics(switch_stream_handle_t *stream)
	{
		auto_lock lock(_mutex);

		stream->write_function(stream, "# HELP mod_avmd_active_sessions_gauge\n");
		stream->write_function(stream, "# TYPE mod_avmd_active_sessions_gauge gauge\n");
		stream->write_function(stream, "mod_avmd_active_sessions %u\n", _sessions);

		stream->write_function(stream, "# HELP mod_avmd_active_detectors_gauge\n");
		stream->write_function(stream, "# TYPE mod_avmd_active_detectors_gauge gauge\n");
		stream->write_function(stream, "mod_avmd_active_detectors %u\n", _detectors);
	}
	

private:
	switch_memory_pool_t* _pool;
	switch_loadable_module_interface_t **_module_interface;
	switch_mutex_t* _mutex;

	unsigned int _sessions;
	unsigned int _detectors;
};

static prometheus_metrics* instance = 0;


SWITCH_STANDARD_API(avmd_prometheus_metrics)
{
	instance->generate_metrics(stream);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_BEGIN_EXTERN_C

void prometheus_init(switch_loadable_module_interface_t **module_interface, switch_api_interface_t *api_interface, switch_memory_pool_t* pool)
{
	SWITCH_ADD_API(api_interface, "avmd_prometheus_metrics", "avmd_prometheus_metrics", avmd_prometheus_metrics, "");
	
	delete instance;
	instance = new prometheus_metrics(module_interface, pool);
}

void prometheus_destroy()
{
	if (instance) {
		delete instance;
		instance = 0;
	}
}

void prometheus_update_active_sessions(unsigned int sessions)
{
	instance->update_active_sessions(sessions);
}

void prometheus_update_detectors(unsigned int detectors)
{
	instance->update_detectors(detectors);
}

SWITCH_END_EXTERN_C


