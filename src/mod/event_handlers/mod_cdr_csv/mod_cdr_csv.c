/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2012, Anthony Minessale II <anthm@freeswitch.org>
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
 * Anthony Minessale II <anthm@freeswitch.org>
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

typedef enum {
	CDR_LEG_A = (1 << 0),
	CDR_LEG_B = (1 << 1)
} cdr_leg_t;

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
	switch_mutex_t *mutex;
	switch_hash_t *fd_hash;
	switch_hash_t *template_hash;
	char *log_dir;
	char *default_template;
	int masterfileonly;
	int shutdown;
	int rotate;
	int debug;
	cdr_leg_t legs;
} globals;

SWITCH_MODULE_LOAD_FUNCTION(mod_cdr_csv_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_cdr_csv_shutdown);
SWITCH_MODULE_DEFINITION(mod_cdr_csv, mod_cdr_csv_load, mod_cdr_csv_shutdown, NULL);

static off_t fd_size(int fd)
{
	struct stat s = { 0 };
	fstat(fd, &s);
	return s.st_size;
}

static void do_reopen(cdr_fd_t *fd)
{
	int x = 0;

	if (fd->fd > -1) {
		close(fd->fd);
		fd->fd = -1;
	}

	for (x = 0; x < 10; x++) {
		if ((fd->fd = open(fd->path, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR)) > -1) {
			fd->bytes = fd_size(fd->fd);
			break;
		}
		switch_yield(100000);
	}
}

static void do_rotate(cdr_fd_t *fd)
{
	switch_time_exp_t tm;
	char date[80] = "";
	switch_size_t retsize;
	char *p;

	close(fd->fd);
	fd->fd = -1;

	if (globals.rotate) {
		switch_time_exp_lt(&tm, switch_micro_time_now());
		switch_strftime_nocheck(date, &retsize, sizeof(date), "%Y-%m-%d-%H-%M-%S", &tm);


		p = switch_mprintf("%s.%s", fd->path, date);
		assert(p);
		switch_file_rename(fd->path, p, globals.pool);
		free(p);
	}

	do_reopen(fd);

	if (fd->fd < 0) {
		switch_event_t *event;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Error opening %s\n", fd->path);
		if (switch_event_create(&event, SWITCH_EVENT_TRAP) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Critical-Error", "Error opening cdr file %s\n", fd->path);
			switch_event_fire(&event);
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "%s CDR logfile %s\n", globals.rotate ? "Rotated" : "Re-opened", fd->path);
	}

}

static void write_cdr(const char *path, const char *log_line)
{
	cdr_fd_t *fd = NULL;
	unsigned int bytes_in, bytes_out;
	int loops = 0;

	switch_mutex_lock(globals.mutex);
	if (!(fd = switch_core_hash_find(globals.fd_hash, path))) {
		fd = switch_core_alloc(globals.pool, sizeof(*fd));
		switch_assert(fd);
		memset(fd, 0, sizeof(*fd));
		fd->fd = -1;
		switch_mutex_init(&fd->mutex, SWITCH_MUTEX_NESTED, globals.pool);
		fd->path = switch_core_strdup(globals.pool, path);
		switch_core_hash_insert(globals.fd_hash, path, fd);
	}
	switch_mutex_unlock(globals.mutex);

	switch_mutex_lock(fd->mutex);
	bytes_out = (unsigned) strlen(log_line);

	if (fd->fd < 0) {
		do_reopen(fd);
		if (fd->fd < 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error opening %s\n", path);
			goto end;
		}
	}

	if (fd->bytes + bytes_out > UINT_MAX) {
		do_rotate(fd);
	}

	while ((bytes_in = write(fd->fd, log_line, bytes_out)) != bytes_out && ++loops < 10) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Write error to file %s %d/%d\n", path, (int) bytes_in, (int) bytes_out);
		do_rotate(fd);
		switch_yield(250000);
	}

	if (bytes_in > 0) {
		fd->bytes += bytes_in;
	}

  end:

	switch_mutex_unlock(fd->mutex);
}

static switch_status_t my_on_reporting(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	const char *log_dir = NULL, *accountcode = NULL, *a_template_str = NULL, *g_template_str = NULL;
	char *log_line, *path = NULL;

	if (globals.shutdown) {
		return SWITCH_STATUS_SUCCESS;
	}

	if (!((globals.legs & CDR_LEG_A) && (globals.legs & CDR_LEG_B))) {
		if ((globals.legs & CDR_LEG_A)) {
			if (switch_channel_get_originator_caller_profile(channel)) {
				return SWITCH_STATUS_SUCCESS;
			}
		} else {
			if (switch_channel_get_originatee_caller_profile(channel)) {
				return SWITCH_STATUS_SUCCESS;
			}
		}
	}

	if (!(log_dir = switch_channel_get_variable(channel, "cdr_csv_base"))) {
		log_dir = globals.log_dir;
	}

	if (switch_dir_make_recursive(log_dir, SWITCH_DEFAULT_DIR_PERMS, switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error creating %s\n", log_dir);
		return SWITCH_STATUS_FALSE;
	}

	if (globals.debug) {
		switch_event_t *event;
		if (switch_event_create_plain(&event, SWITCH_EVENT_CHANNEL_DATA) == SWITCH_STATUS_SUCCESS) {
			char *buf;
			switch_channel_event_set_data(channel, event);
			switch_event_serialize(event, &buf, SWITCH_FALSE);
			switch_assert(buf);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "CHANNEL_DATA:\n%s\n", buf);
			switch_event_destroy(&event);
			free(buf);
		}
	}

	g_template_str = (const char *) switch_core_hash_find(globals.template_hash, globals.default_template);

	if ((accountcode = switch_channel_get_variable(channel, "ACCOUNTCODE"))) {
		a_template_str = (const char *) switch_core_hash_find(globals.template_hash, accountcode);
	}

	if (!g_template_str) {
		g_template_str =
			"\"${accountcode}\",\"${caller_id_number}\",\"${destination_number}\",\"${context}\",\"${caller_id}\",\"${channel_name}\",\"${bridge_channel}\",\"${last_app}\",\"${last_arg}\",\"${start_stamp}\",\"${answer_stamp}\",\"${end_stamp}\",\"${duration}\",\"${billsec}\",\"${hangup_cause}\",\"${amaflags}\",\"${uuid}\",\"${userfield}\";";
	}

	if (!a_template_str) {
		a_template_str = g_template_str;
	}

	log_line = switch_channel_expand_variables(channel, a_template_str);

	if ((accountcode) && (!globals.masterfileonly)) {
		path = switch_mprintf("%s%s%s.csv", log_dir, SWITCH_PATH_SEPARATOR, accountcode);
		assert(path);
		write_cdr(path, log_line);
		free(path);
	}

	if (g_template_str != a_template_str) {
		if (log_line != a_template_str) {
			switch_safe_free(log_line);
		}
		log_line = switch_channel_expand_variables(channel, g_template_str);
	}

	if (!log_line) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error creating cdr\n");
		return SWITCH_STATUS_FALSE;
	}

	path = switch_mprintf("%s%sMaster.csv", log_dir, SWITCH_PATH_SEPARATOR);
	assert(path);
	write_cdr(path, log_line);
	free(path);


	if (log_line != g_template_str) {
		free(log_line);
	}

	return status;
}


static void do_rotate_all()
{
	switch_hash_index_t *hi;
	void *val;
	cdr_fd_t *fd;

	if (globals.shutdown) {
		return;
	}

	switch_mutex_lock(globals.mutex);
	for (hi = switch_hash_first(NULL, globals.fd_hash); hi; hi = switch_hash_next(hi)) {
		switch_hash_this(hi, NULL, NULL, &val);
		fd = (cdr_fd_t *) val;
		switch_mutex_lock(fd->mutex);
		do_rotate(fd);
		switch_mutex_unlock(fd->mutex);
	}
	switch_mutex_unlock(globals.mutex);
}


static void do_teardown()
{
	switch_hash_index_t *hi;
	void *val;
	cdr_fd_t *fd;
	switch_mutex_lock(globals.mutex);
	for (hi = switch_hash_first(NULL, globals.fd_hash); hi; hi = switch_hash_next(hi)) {
		switch_hash_this(hi, NULL, NULL, &val);
		fd = (cdr_fd_t *) val;
		switch_mutex_lock(fd->mutex);
		if (fd->fd > -1) {
			close(fd->fd);
			fd->fd = -1;
		}
		switch_mutex_unlock(fd->mutex);
	}
	switch_mutex_unlock(globals.mutex);
}


static void event_handler(switch_event_t *event)
{
	const char *sig = switch_event_get_header(event, "Trapped-Signal");

	if (sig && !strcmp(sig, "HUP")) {
		do_rotate_all();
	}
}


SWITCH_STANDARD_API(cdr_csv_function)
{
	if (!strcmp(cmd, "rotate")) {
		do_rotate_all();
		stream->write_function(stream, "+OK");
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;
}


static switch_state_handler_table_t state_handlers = {
	/*.on_init */ NULL,
	/*.on_routing */ NULL,
	/*.on_execute */ NULL,
	/*.on_hangup */ NULL,
	/*.on_exchange_media */ NULL,
	/*.on_soft_execute */ NULL,
	/*.on_consume_media */ NULL,
	/*.on_hibernate */ NULL,
	/*.on_reset */ NULL,
	/*.on_park */ NULL,
	/*.on_reporting */ my_on_reporting
};



static switch_status_t load_config(switch_memory_pool_t *pool)
{
	char *cf = "cdr_csv.conf";
	switch_xml_t cfg, xml, settings, param;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	memset(&globals, 0, sizeof(globals));
	switch_core_hash_init(&globals.fd_hash, pool);
	switch_core_hash_init(&globals.template_hash, pool);

	globals.pool = pool;

	switch_core_hash_insert(globals.template_hash, "default", default_template);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Adding default template.\n");
	globals.legs = CDR_LEG_A;

	if ((xml = switch_xml_open_cfg(cf, &cfg, NULL))) {

		if ((settings = switch_xml_child(cfg, "settings"))) {
			for (param = switch_xml_child(settings, "param"); param; param = param->next) {
				char *var = (char *) switch_xml_attr_soft(param, "name");
				char *val = (char *) switch_xml_attr_soft(param, "value");
				if (!strcasecmp(var, "debug")) {
					globals.debug = switch_true(val);
				} else if (!strcasecmp(var, "legs")) {
					globals.legs = 0;

					if (strchr(val, 'a')) {
						globals.legs |= CDR_LEG_A;
					}

					if (strchr(val, 'b')) {
						globals.legs |= CDR_LEG_B;
					}
				} else if (!strcasecmp(var, "log-base")) {
					globals.log_dir = switch_core_sprintf(pool, "%s%scdr-csv", val, SWITCH_PATH_SEPARATOR);
				} else if (!strcasecmp(var, "rotate-on-hup")) {
					globals.rotate = switch_true(val);
				} else if (!strcasecmp(var, "default-template")) {
					globals.default_template = switch_core_strdup(pool, val);
				} else if (!strcasecmp(var, "master-file-only")) {
					globals.masterfileonly = switch_true(val);
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


	if (zstr(globals.default_template)) {
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
	switch_api_interface_t *api_interface;

	load_config(pool);

	switch_mutex_init(&globals.mutex, SWITCH_MUTEX_NESTED, globals.pool);

	if ((status = switch_dir_make_recursive(globals.log_dir, SWITCH_DEFAULT_DIR_PERMS, pool)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error creating %s\n", globals.log_dir);
		return status;
	}

	if ((status = switch_event_bind(modname, SWITCH_EVENT_TRAP, SWITCH_EVENT_SUBCLASS_ANY, event_handler, NULL)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind!\n");
		return status;
	}

	switch_core_add_state_handler(&state_handlers);
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	SWITCH_ADD_API(api_interface, "cdr_csv", "cdr_csv controls", cdr_csv_function, "parameters");
	switch_console_set_complete("add cdr_csv rotate");

	return status;
}


SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_cdr_csv_shutdown)
{
	switch_console_set_complete("del cdr_csv");

	globals.shutdown = 1;
	switch_event_unbind_callback(event_handler);
	switch_core_remove_state_handler(&state_handlers);

	do_teardown();
	switch_core_hash_destroy(&globals.fd_hash);
	switch_core_hash_destroy(&globals.template_hash);

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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
