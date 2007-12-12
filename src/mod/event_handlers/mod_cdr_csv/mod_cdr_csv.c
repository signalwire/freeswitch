/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005/2006, Anthony Minessale II <anthmct@yahoo.com>
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Anthony Minessale II <anthmct@yahoo.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * mod_cdr_csv.c -- Asterisk Compatible CDR Module
 *
 */
#include <sys/stat.h>
#include <switch.h>

struct cdr_fd {
	int fd;
	char *path;
	int64_t bytes;
	switch_mutex_t *mutex;
};
typedef struct cdr_fd cdr_fd_t;

const char *default_template = 
	"\"${caller_id_name}\",\"${caller_id_number}\",\"${destination_number}\",\"${context}\",\"${start_stamp}\","
	"\"${answer_stamp}\",\"${end_stamp}\",\"${duration}\",\"${billsec}\",\"${hangup_cause}\",\"${uuid}\",\"${bleg_uuid}\", \"${accountcode}\"\n";

static struct {
	switch_memory_pool_t *pool;
	switch_hash_t *fd_hash;
	switch_hash_t *template_hash;
	char *log_dir;
	char *default_template;
	int shutdown;
	int rotate;
	int debug;
} globals;

SWITCH_MODULE_LOAD_FUNCTION(mod_cdr_csv_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_cdr_csv_shutdown);
SWITCH_MODULE_DEFINITION(mod_cdr_csv, mod_cdr_csv_load, NULL, NULL);

static off_t fd_size(int fd)
{
	struct stat s = {0};
	fstat(fd, &s);
	return s.st_size;
}

static void do_reopen(cdr_fd_t *fd) 
{

	if (fd->fd > -1) {
		close(fd->fd);
		fd->fd = -1;
	}

	if ((fd->fd = open(fd->path, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR)) > -1) {
		fd->bytes = fd_size(fd->fd);
	}
}

static void do_rotate(cdr_fd_t *fd)
{
	switch_time_exp_t tm;
	char date[80] = "";
	switch_size_t retsize;
	char *p;
	size_t len;

	close(fd->fd);

	if (globals.rotate) {
		switch_time_exp_lt(&tm, switch_time_now());
		switch_strftime(date, &retsize, sizeof(date), "%Y-%m-%d-%H-%M-%S", &tm);
	
		len = strlen(fd->path) + strlen(date) + 2;
		p = switch_mprintf("%s.%s", fd->path, date);
		assert(p);
		switch_file_rename(fd->path, p, globals.pool);
		free(p);
	}

	do_reopen(fd);
	
	if (fd->fd < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error opening %s\n", fd->path);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "%s CDR logfile %s\n", globals.rotate ? "Rotated" : "Re-opened", fd->path);
	}

}

static void write_cdr(const char *path, const char *log_line)
{
	cdr_fd_t *fd = NULL;

	if ((fd = switch_core_hash_find(globals.fd_hash, path))) {
		switch_mutex_lock(fd->mutex);
	} else {
		int lfd = open(path, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
		
		if (lfd > -1) {
			fd = switch_core_alloc(globals.pool, sizeof(*fd));
			switch_mutex_init(&fd->mutex, SWITCH_MUTEX_NESTED, globals.pool);
			fd->fd = lfd;
			switch_mutex_lock(fd->mutex);
			fd->path = switch_core_strdup(globals.pool, path);
			fd->bytes = fd_size(fd->fd);
			switch_core_hash_insert(globals.fd_hash, path, fd);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error opening %s\n", path);
		}
	}

	if (fd) {
		unsigned int bytes_in, bytes_out;
		bytes_out = (unsigned)strlen(log_line);
		
		if (fd->bytes + bytes_out > UINT_MAX) {
			do_rotate(fd);
		}

		if (fd->fd < 0) {
			do_reopen(fd);
			if (fd->fd < 0) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error opening %s\n", path);
				return;
			}
		}
		
		if ((bytes_in = write(fd->fd, log_line, bytes_out)) != bytes_out) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Write error to file %s\n", path);
		}
		fd->bytes += bytes_in;
		switch_mutex_unlock(fd->mutex);
	}
	
}

static switch_status_t my_on_hangup(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	const char *log_dir = NULL, *accountcode = NULL, *cid_buf = NULL, *a_template_str = NULL, *g_template_str = NULL;
	char *log_line, *path = NULL;	
	switch_caller_profile_t *caller_profile, *ocp;
	
	switch_app_log_t *app_log, *ap;
	char *last_app = NULL, *last_arg = NULL;
	char start[80] = "", answer[80] = "", end[80] = "", tmp[80] = "";
	int32_t duration = 0, billsec = 0, mduration = 0, billmsec = 0;
	switch_time_t uduration = 0, billusec = 0;
	time_t tt_created = 0, tt_answered = 0, tt_hungup = 0, mtt_created = 0, mtt_answered = 0, mtt_hungup = 0;

	if (switch_channel_get_originator_caller_profile(channel)) {
		return SWITCH_STATUS_SUCCESS;
	}

	if (!(log_dir = switch_channel_get_variable(channel, "cdr_csv_base"))) {
		log_dir = globals.log_dir;
	}
	
	if (switch_dir_make_recursive(log_dir, SWITCH_DEFAULT_DIR_PERMS, switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error creating %s\n", log_dir);
		return SWITCH_STATUS_FALSE;
	}

	caller_profile = switch_channel_get_caller_profile(channel);
	ocp = switch_channel_get_originatee_caller_profile(channel);

	if (!switch_strlen_zero(caller_profile->caller_id_name)) {
		cid_buf = switch_core_session_sprintf(session, "\"%s\" <%s>", caller_profile->caller_id_name, 
											  switch_str_nil(caller_profile->caller_id_number));
	} else {
		cid_buf = caller_profile->caller_id_number;
	}

	if ((app_log = switch_core_session_get_app_log(session))) {
		for (ap = app_log; ap && ap->next; ap = ap->next);
		last_app = ap->app;
		last_arg = ap->arg;
	}
	
	if (caller_profile->times) {
		switch_time_exp_t tm;
		switch_size_t retsize;
		const char *fmt = "%Y-%m-%d %T";

		switch_time_exp_lt(&tm, caller_profile->times->created);
		switch_strftime(start, &retsize, sizeof(start), fmt, &tm);
		switch_channel_set_variable(channel, "start_stamp", start);

		switch_time_exp_lt(&tm, caller_profile->times->answered);
		switch_strftime(answer, &retsize, sizeof(answer), fmt, &tm);
		switch_channel_set_variable(channel, "answer_stamp", answer);

		switch_time_exp_lt(&tm, caller_profile->times->hungup);
		switch_strftime(end, &retsize, sizeof(end), fmt, &tm);
		switch_channel_set_variable(channel, "end_stamp", end);

		tt_created = (time_t) (caller_profile->times->created / 1000000);
		mtt_created = (time_t) (caller_profile->times->created / 1000);
		switch_snprintf(tmp, sizeof(tmp), "%" TIME_T_FMT, tt_created);
		switch_channel_set_variable(channel, "start_epoch", tmp);
		switch_snprintf(tmp, sizeof(tmp), "%" SWITCH_TIME_T_FMT, caller_profile->times->created);
		switch_channel_set_variable(channel, "start_uepoch", tmp);
		
		tt_answered = (time_t) (caller_profile->times->answered / 1000000);
		mtt_answered = (time_t) (caller_profile->times->answered / 1000);
		switch_snprintf(tmp, sizeof(tmp), "%" TIME_T_FMT, tt_answered);
		switch_channel_set_variable(channel, "answer_epoch", tmp);
		switch_snprintf(tmp, sizeof(tmp), "%" SWITCH_TIME_T_FMT, caller_profile->times->answered);
		switch_channel_set_variable(channel, "answer_uepoch", tmp);		


		tt_hungup = (time_t) (caller_profile->times->hungup / 1000000);
		mtt_hungup = (time_t) (caller_profile->times->hungup / 1000);
		switch_snprintf(tmp, sizeof(tmp), "%" TIME_T_FMT, tt_hungup);
		switch_channel_set_variable(channel, "end_epoch", tmp);
		switch_snprintf(tmp, sizeof(tmp), "%" SWITCH_TIME_T_FMT, caller_profile->times->hungup);
		switch_channel_set_variable(channel, "end_uepoch", tmp);

		uduration = caller_profile->times->hungup - caller_profile->times->created;
		duration = (int32_t)(tt_hungup - tt_created);
		mduration = (int32_t)(mtt_hungup - mtt_created);
		
		if (caller_profile->times->answered) {
			billsec = (int32_t)(tt_hungup - tt_answered);
			billmsec = (int32_t)(mtt_hungup - mtt_answered);
			billusec = caller_profile->times->hungup - caller_profile->times->answered;
		}
	}
	
	
	switch_channel_set_variable(channel, "last_app", last_app);
	switch_channel_set_variable(channel, "last_arg", last_arg);
	switch_channel_set_variable(channel, "caller_id", cid_buf);

	switch_snprintf(tmp, sizeof(tmp), "%d", duration);
	switch_channel_set_variable(channel, "duration", tmp);

	switch_snprintf(tmp, sizeof(tmp), "%d", billsec);
	switch_channel_set_variable(channel, "billsec", tmp);
	
	switch_snprintf(tmp, sizeof(tmp), "%d", mduration);
	switch_channel_set_variable(channel, "mduration", tmp);

	switch_snprintf(tmp, sizeof(tmp), "%d", billmsec);
	switch_channel_set_variable(channel, "billmsec", tmp);

	switch_snprintf(tmp, sizeof(tmp), "%" SWITCH_TIME_T_FMT, uduration);
	switch_channel_set_variable(channel, "uduration", tmp);

	switch_snprintf(tmp, sizeof(tmp), "%" SWITCH_TIME_T_FMT, billusec);
	switch_channel_set_variable(channel, "billusec", tmp);

	if (globals.debug) {
		switch_event_t *event;
		if (switch_event_create(&event, SWITCH_EVENT_MESSAGE) == SWITCH_STATUS_SUCCESS) {
			char *buf;
			switch_channel_event_set_data(channel, event);
			switch_event_serialize(event, &buf, SWITCH_FALSE);
			switch_assert(buf);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "CHANNEL_DATA:\n%s\n", buf);
			switch_event_destroy(&event);
			free(buf);
		}
	}

	g_template_str = (const char *) switch_core_hash_find(globals.template_hash, globals.default_template);
	
	if ((accountcode = switch_channel_get_variable(channel, "ACCOUNTCODE"))) {
		a_template_str = (const char *) switch_core_hash_find(globals.template_hash, accountcode);
	}
	
	if (!a_template_str) {
		a_template_str = g_template_str;
	}
	
	log_line = switch_channel_expand_variables(channel, a_template_str);

	if (accountcode) {
		path = switch_mprintf("%s%s%s.csv", log_dir, SWITCH_PATH_SEPARATOR, accountcode);
		assert(path);
		write_cdr(path, log_line);
		free(path);
	}
		
	if (g_template_str != a_template_str) {
		if (log_line && log_line != a_template_str) {
			switch_safe_free(log_line);
		}
		log_line = switch_channel_expand_variables(channel, g_template_str);
	}
	
		

	path = switch_mprintf("%s%sMaster.csv", log_dir, SWITCH_PATH_SEPARATOR);
	assert(path);
	write_cdr(path, log_line);
	free(path);
	

	if (log_line && log_line != g_template_str) {
		free(log_line);
	}

	return status;
}


static void event_handler(switch_event_t *event)
{
	const char *sig = switch_event_get_header(event, "Trapped-Signal");
    switch_hash_index_t *hi;
	void *val;
	cdr_fd_t *fd;

	if (sig && !strcmp(sig, "HUP")) {
		for (hi = switch_hash_first(NULL, globals.fd_hash); hi; hi = switch_hash_next(hi)) {
			switch_hash_this(hi, NULL, NULL, &val);
			fd = (cdr_fd_t *) val;
			do_rotate(fd);
		}
	}
}


static switch_state_handler_table_t state_handlers = {
	/*.on_init */ NULL,
	/*.on_ring */ NULL,
	/*.on_execute */ NULL,
	/*.on_hangup */ my_on_hangup,
	/*.on_loopback */ NULL,
	/*.on_transmit */ NULL
};



static switch_status_t load_config(switch_memory_pool_t *pool)
{
	char *cf = "cdr_csv.conf";
	switch_xml_t cfg, xml, settings, param;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	
	memset(&globals,0,sizeof(globals));
	switch_core_hash_init(&globals.fd_hash, pool);
	switch_core_hash_init(&globals.template_hash, pool);
	
	globals.pool = pool;
	
	switch_core_hash_insert(globals.template_hash, "default", default_template);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Adding default template.\n");

	if ((xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		
		if ((settings = switch_xml_child(cfg, "settings"))) {
			for (param = switch_xml_child(settings, "param"); param; param = param->next) {
				char *var = (char *) switch_xml_attr_soft(param, "name");
				char *val = (char *) switch_xml_attr_soft(param, "value");
				if (!strcasecmp(var, "debug")) {
					globals.debug = switch_true(val);
				} else if (!strcasecmp(var, "log-base")) {
					globals.log_dir = switch_core_sprintf(pool, "%s%scdr-csv", val, SWITCH_PATH_SEPARATOR);
				} else if (!strcasecmp(var, "rotate-on-hup")) {
					globals.rotate = switch_true(val);
				} else if (!strcasecmp(var, "default-template")) {
					globals.default_template = switch_core_strdup(pool, val);
				}
			}
		}

		if ((settings = switch_xml_child(cfg, "templates"))) {
			for (param = switch_xml_child(settings, "template"); param; param = param->next) {
				char *var = (char *) switch_xml_attr(param, "name");
				if (var) {
					char *tpl;
					size_t len = strlen(param->txt) + 2;
					if (end_of(param->txt) != '\n') {
						tpl = switch_core_alloc(pool, len);
						switch_snprintf(tpl, len, "%s\n", param->txt);
					} else {
						tpl = switch_core_strdup(pool, param->txt);
					}

					switch_core_hash_insert(globals.template_hash, var, tpl);
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Adding template %s.\n", var);
				}
			}
		}
		switch_xml_free(xml);
	}
	

	if (switch_strlen_zero(globals.default_template)) {
		globals.default_template = switch_core_strdup(pool, "default");
	}

	if (!globals.log_dir) {
		globals.log_dir = switch_core_sprintf(pool, "%s%scdr-csv", SWITCH_GLOBAL_dirs.log_dir, SWITCH_PATH_SEPARATOR);
	}
	
	return status;
}


SWITCH_MODULE_LOAD_FUNCTION(mod_cdr_csv_load)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (switch_event_bind((char *) modname, SWITCH_EVENT_TRAP, SWITCH_EVENT_SUBCLASS_ANY, event_handler, NULL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind!\n");
		return SWITCH_STATUS_GENERR;
	}

	switch_core_add_state_handler(&state_handlers);
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	load_config(pool);
	
	if ((status = switch_dir_make_recursive(globals.log_dir, SWITCH_DEFAULT_DIR_PERMS, pool)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error creating %s\n", globals.log_dir);
	}
	
	return status;
}


SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_cdr_csv_shutdown)
{
	
	globals.shutdown=1;
    return SWITCH_STATUS_SUCCESS;
}



/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */
