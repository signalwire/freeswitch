#include <string>
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
		_cdr_counter(0),
		_cdr_success(0),
		_cdr_error(0),
		_tmpcdr_success(0),
		_tmpcdr_error(0),
		_backup_cdr_success(0),
		_backup_cdr_error(0)
	{
		switch_mutex_init(&_mutex, SWITCH_MUTEX_NESTED, _pool);
	}
	
	~prometheus_metrics()
	{
		switch_mutex_destroy(_mutex);
	}

	void increment_cdr_counter()
	{
		auto_lock lock(_mutex);
		_cdr_counter++;
	}

	void increment_cdr_success()
	{
		auto_lock lock(_mutex);
		_cdr_success++;
	}

	void increment_cdr_error()
	{
		auto_lock lock(_mutex);
		_cdr_error++;
	}

	void increment_tmpcdr_move_success()
	{
		auto_lock lock(_mutex);
		_tmpcdr_success++;
	}
	
	void increment_tmpcdr_move_error()
	{
		auto_lock lock(_mutex);
		_tmpcdr_error++;
	}

	void increment_backup_cdr_success()
	{
		auto_lock lock(_mutex);
		_backup_cdr_success++;
	}

	void increment_backup_cdr_error()
	{
		auto_lock lock(_mutex);
		_backup_cdr_error++;
	}
		
	void generate_metrics(switch_stream_handle_t *stream)
	{
		auto_lock lock(_mutex);

		stream->write_function(stream, "# HELP mod_json_cdr_counter Generated CDRs\n");
		stream->write_function(stream, "# TYPE mod_json_cdr_counter counter\n");
		stream->write_function(stream, "mod_json_cdr_counter %u\n", _cdr_counter);

		stream->write_function(stream, "# HELP mod_json_cdr_success CDRs written to disk\n");
		stream->write_function(stream, "# TYPE mod_json_cdr_success counter\n");
		stream->write_function(stream, "mod_json_cdr_success %u\n", _cdr_success);

		stream->write_function(stream, "# HELP mod_json_cdr_error CDR errors\n");
		stream->write_function(stream, "# TYPE mod_json_cdr_error counter\n");
		stream->write_function(stream, "mod_json_cdr_error %u\n", _cdr_error);

		stream->write_function(stream, "# HELP mod_json_tmpcdr_success CDRs moved from tmpdir folder\n");
		stream->write_function(stream, "# TYPE mod_json_tmpcdr_success counter\n");
		stream->write_function(stream, "mod_json_tmpcdr_success %u\n", _tmpcdr_success);

		stream->write_function(stream, "# HELP mod_json_tmpcdr_error CDRs not moved from tmpdir folder\n");
		stream->write_function(stream, "# TYPE mod_json_tmpcdr_error counter\n");
		stream->write_function(stream, "mod_json_tmpcdr_error %u\n", _tmpcdr_error);

		stream->write_function(stream, "# HELP mod_json_backup_cdr_success Successful CDRs backup\n");
		stream->write_function(stream, "# TYPE mod_json_backup_cdr_success counter\n");
		stream->write_function(stream, "mod_json_backup_cdr_success %u\n", _backup_cdr_success);

		stream->write_function(stream, "# HELP mod_json_backup_cdr_error Backup CDR errors\n");
		stream->write_function(stream, "# TYPE mod_json_backup_cdr_error counter\n");
		stream->write_function(stream, "mod_json_backup_cdr_error %u\n", _backup_cdr_error);
	}
	

private:
	switch_memory_pool_t* _pool;
	switch_loadable_module_interface_t **_module_interface;
	switch_mutex_t* _mutex;

	ssize_t _cdr_counter;
	ssize_t _cdr_success;
	ssize_t _cdr_error;
	ssize_t _tmpcdr_success;
	ssize_t _tmpcdr_error;
	ssize_t _backup_cdr_success;
	ssize_t _backup_cdr_error;
};

static prometheus_metrics* instance = 0;


SWITCH_STANDARD_API(json_cdr_prometheus_metrics)
{
	instance->generate_metrics(stream);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_BEGIN_EXTERN_C

void prometheus_init(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t* pool)
{
	switch_api_interface_t *api_interface;
	SWITCH_ADD_API(api_interface, "json_cdr_prometheus_metrics", "json_cdr_prometheus_metrics", json_cdr_prometheus_metrics, "");
	
	delete instance;
	instance = new prometheus_metrics(module_interface, pool);
}

void prometheus_destroy(void)
{
	delete instance;
	instance = 0;
}

void prometheus_increment_cdr_counter(void)
{
	instance->increment_cdr_counter();
}

void prometheus_increment_cdr_success(void)
{
	instance->increment_cdr_success();
}

void prometheus_increment_cdr_error(void)
{
	instance->increment_cdr_error();
}

void prometheus_increment_tmpcdr_move_success(void)
{
	instance->increment_tmpcdr_move_success();
}

void prometheus_increment_tmpcdr_move_error(void)
{
	instance->increment_tmpcdr_move_error();
}

void prometheus_increment_backup_cdr_success(void)
{
	instance->increment_backup_cdr_success();
}

void prometheus_increment_backup_cdr_error(void)
{
	instance->increment_backup_cdr_error();
}

SWITCH_END_EXTERN_C


