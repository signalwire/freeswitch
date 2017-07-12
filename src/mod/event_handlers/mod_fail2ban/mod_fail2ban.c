#include <switch.h>

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_fail2ban_shutdown);
SWITCH_MODULE_LOAD_FUNCTION(mod_fail2ban_load);

SWITCH_MODULE_DEFINITION(mod_fail2ban, mod_fail2ban_load, mod_fail2ban_shutdown, NULL);

static struct {
	switch_memory_pool_t *modpool;
	switch_file_t *logfile;
	char *logfile_name;
} globals = {0};


static switch_status_t mod_fail2ban_do_config(void)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	char *cf = "fail2ban.conf";
	switch_xml_t cfg, xml, bindings_tag, config = NULL, param = NULL;
	char *var = NULL, *val = NULL;
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "setting configs\n");		

	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", cf);
		return SWITCH_STATUS_TERM;
	}
	
	if (!(bindings_tag = switch_xml_child(cfg, "bindings"))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Missing <bindings> tag!\n");
		goto done;
	}
	
	for (config = switch_xml_child(bindings_tag, "config"); config; config = config->next) {
		
		for (param = switch_xml_child(config, "param"); param; param = param->next) {
			var = (char *) switch_xml_attr_soft(param, "name");
			val = (char *) switch_xml_attr_soft(param, "value");
			
			if (strncmp(var,"logfile", 7) == 0) {
			  if (zstr(val)) {
			    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Null or empty Logfile attribute %s: %s\n", var, val);		
			  } else {
			    globals.logfile_name = switch_core_strdup(globals.modpool, val);
			  }
			} else {
			  switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unknown attribute %s: %s\n", var, val);		
			}
		}	
	}
	if ( zstr(globals.logfile_name) ) {
	  globals.logfile_name = switch_core_sprintf(globals.modpool, "%s%s%s", SWITCH_GLOBAL_dirs.log_dir, SWITCH_PATH_SEPARATOR, "fail2ban.log");
	}
	
	if ((status = switch_file_open(&globals.logfile, globals.logfile_name, SWITCH_FOPEN_WRITE|SWITCH_FOPEN_APPEND|SWITCH_FOPEN_CREATE, SWITCH_FPROT_OS_DEFAULT, globals.modpool)) != SWITCH_STATUS_SUCCESS) {
	  switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "failed to open %s\n", globals.logfile_name);		
	  status = SWITCH_STATUS_FALSE;
	} 
	
 done:
	switch_xml_free(xml);
	
	return SWITCH_STATUS_SUCCESS;
}

static int fail2ban_logger(const char *message, char *user, char *ip)
{
	switch_time_exp_t tm;
	if (!globals.logfile) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not print to fail2ban log!\n");
		return -1;
	}                                                                                                                                            
	
	switch_time_exp_lt(&tm, switch_micro_time_now());
	return switch_file_printf(globals.logfile, "%s user[%s] ip[%s] at[%04u-%02u-%02uT%02u:%02u:%02u.%06u%+03d%02d]\n", message, user, ip,
								tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
								tm.tm_hour, tm.tm_min, tm.tm_sec, tm.tm_usec, tm.tm_gmtoff / 3600, tm.tm_gmtoff % 3600);
}

static void fail2ban_event_handler(switch_event_t *event)
{
	if (event->event_id == SWITCH_EVENT_CUSTOM && event->subclass_name) {
		if (strncmp(event->subclass_name, "sofia::register_attempt",23) == 0) {
			fail2ban_logger("A registration was attempted", switch_event_get_header(event, "to-user"), switch_event_get_header(event, "network-ip"));
		} else if (strncmp(event->subclass_name, "sofia::register_failure",23) == 0) {
			fail2ban_logger("A registration failed", switch_event_get_header(event, "to-user"), switch_event_get_header(event, "network-ip"));
		} else if (strncmp(event->subclass_name, "sofia::wrong_call_state",23) == 0) {
			fail2ban_logger("Abandoned call from ", switch_event_get_header(event, "from_user"), switch_event_get_header(event, "network_ip"));
		}
	}
}

SWITCH_MODULE_LOAD_FUNCTION(mod_fail2ban_load)
{
	switch_status_t status;
	void *user_data = NULL;
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	globals.modpool = pool;

	if (mod_fail2ban_do_config() != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_FALSE;
	}

	if ((status = switch_event_bind(modname, SWITCH_EVENT_CUSTOM, SWITCH_EVENT_SUBCLASS_ANY, fail2ban_event_handler, user_data)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "event bind failed\n");
		return SWITCH_STATUS_FALSE;
	} 

	switch_file_printf(globals.logfile, "Fail2ban was started\n");
	return SWITCH_STATUS_SUCCESS;
}


SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_fail2ban_shutdown)
{
	switch_status_t status;

	if (globals.logfile != NULL) {
	  switch_file_printf(globals.logfile, "Fail2ban stoping\n");
	}

	if ((status = switch_event_unbind_callback(fail2ban_event_handler)) != SWITCH_STATUS_SUCCESS) {
	  switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "event unbind failed\n");
	}

	if ((status = switch_file_close(globals.logfile)) != SWITCH_STATUS_SUCCESS) {
	  switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "failed to close %s\n", globals.logfile_name);		
	}
	
	return status;
}
