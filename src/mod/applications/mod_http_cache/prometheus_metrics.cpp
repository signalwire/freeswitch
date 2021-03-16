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
		switch_mutex_t* _mutex;
	};
	
	prometheus_metrics(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t* pool) :
		_pool(pool),
		_module_interface(module_interface),
		_download_fail_count(0),
		_download_buckets{
			{300, 500, 800, 1300, 2100, 3400, 5500, BUCKET_INFINITY},
			{0,   0,   0,   0,    0,    0,    0,    0}
		},
		_download_bucket_sum(0),
		_download_bucket_count(0)
	{
		switch_mutex_init(&_mutex, SWITCH_MUTEX_NESTED, _pool);
	}
	
	~prometheus_metrics()
	{
		switch_mutex_destroy(_mutex);
	}

	void increment_download_duration(unsigned int duration)
	{
		auto_lock lock(_mutex);
		_download_bucket_sum += duration;
		_download_bucket_count++;

		for (int i = 0; i < MAX_BUCKET_LEN; i++) {
			if (duration < _download_buckets[BUCKET_BOUND][i]) {
				_download_buckets[BUCKET_COUNT][i]++;
				break;
			}
		}
	}

	void increment_download_fail_count(void)
	{
		auto_lock lock(_mutex);
		_download_fail_count++;
	}
		
	void generate_metrics(switch_stream_handle_t *stream)
	{
		auto_lock lock(_mutex);

		stream->write_function(stream, "# HELP mod_http_cache_download_fail_count\n");
		stream->write_function(stream, "# TYPE mod_http_cache_download_fail_count counter\n");
		stream->write_function(stream, "mod_http_cache_download_fail_count %u\n", _download_fail_count);

		stream->write_function(stream, "# HELP mod_http_cache_download_duration\n");
		stream->write_function(stream, "# TYPE mod_http_cache_download_duration histogram\n");
		for (int i = 0; i < MAX_BUCKET_LEN - 1; i++) {
			stream->write_function(stream, "mod_http_cache_download_duration_bucket{le=\"%u\"} %u\n", _download_buckets[BUCKET_BOUND][i], _download_buckets[BUCKET_COUNT][i]);
		}
		stream->write_function(stream, "mod_http_cache_download_duration_bucket{le=\"+Inf\"} %u\n", _download_buckets[BUCKET_COUNT][MAX_BUCKET_LEN - 1]);
		stream->write_function(stream, "mod_http_cache_download_duration_sum %llu\n", _download_bucket_sum);
		stream->write_function(stream, "mod_http_cache_download_duration_count %lu\n", _download_bucket_count);
	}
	

private:
	switch_memory_pool_t* _pool;
	switch_loadable_module_interface_t **_module_interface;
	switch_mutex_t* _mutex;

	unsigned int _download_fail_count;
	unsigned int _download_buckets[2][MAX_BUCKET_LEN];
	unsigned long long int _download_bucket_sum;
	unsigned long int _download_bucket_count;
};

static prometheus_metrics* instance = 0;


SWITCH_STANDARD_API(http_cache_prometheus_metrics)
{
	instance->generate_metrics(stream);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_BEGIN_EXTERN_C

void prometheus_init(switch_loadable_module_interface_t **module_interface, switch_api_interface_t *api_interface, switch_memory_pool_t* pool)
{
	SWITCH_ADD_API(api_interface, "http_cache_prometheus_metrics", "http_cache_prometheus_metrics", http_cache_prometheus_metrics, "");
	
	delete instance;
	instance = new prometheus_metrics(module_interface, pool);
}

void prometheus_destroy()
{
	delete instance;
	instance = 0;
}

void prometheus_increment_download_duration(unsigned int duration)
{
	instance->increment_download_duration(duration);
}

void prometheus_increment_download_fail_count()
{
	instance->increment_download_fail_count();
}

SWITCH_END_EXTERN_C


