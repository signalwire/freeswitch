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
 * Michal Bielicki <michal.bielicki@halokwadrat.de>
 * Daniel Swarbrick <daniel.swarbrick@seventhsignal.de>
 * Evgeney Bugorov <evgeney.bugrov@halokwadrat.ru>
 * Sponsored by Halo Kwadrat Sp. z o.o. & Seventh Signal Ltd. & CO. KG
 *
 * mod_cdr_pg_csv.c -- Asterisk Compatible CDR Module with PostgreSQL interface
 * derived from:
 * mod_cdr_csv.c -- Asterisk Compatible CDR Module
 *
 */

#include <sys/stat.h>
#include <switch.h>
#include <libpq-fe.h>

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
	"\"${local_ip_v4}\",\"${caller_id_name}\",\"${caller_id_number}\",\"${destination_number}\",\"${context}\",\"${start_stamp}\","
	"\"${answer_stamp}\",\"${end_stamp}\",\"${duration}\",\"${billsec}\",\"${hangup_cause}\",\"${uuid}\",\"${bleg_uuid}\", \"${accountcode}\","
	"\"${read_codec}\", \"${write_codec}\"\n";

static struct {
	switch_memory_pool_t *pool;
	switch_hash_t *fd_hash;
	switch_hash_t *template_hash;
	char *log_dir;
	char *default_template;
	int shutdown;
	int rotate;
	int debug;
	cdr_leg_t legs;
	char *a_table;
	char *g_table;
	char *db_info;
	PGconn *db_connection;
	int db_online;
	switch_mutex_t *db_mutex;
} globals = { 0 };

SWITCH_MODULE_LOAD_FUNCTION(mod_cdr_pg_csv_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_cdr_pg_csv_shutdown);
SWITCH_MODULE_DEFINITION(mod_cdr_pg_csv, mod_cdr_pg_csv_load, mod_cdr_pg_csv_shutdown, NULL);

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
	size_t len;

	close(fd->fd);
	fd->fd = -1;

	if (globals.rotate) {
		switch_time_exp_lt(&tm, switch_micro_time_now());
		switch_strftime(date, &retsize, sizeof(date), "%Y-%m-%d-%H-%M-%S", &tm);

		len = strlen(fd->path) + strlen(date) + 2;
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

	if (!(fd = switch_core_hash_find(globals.fd_hash, path))) {
		fd = switch_core_alloc(globals.pool, sizeof(*fd));
		switch_assert(fd);
		memset(fd, 0, sizeof(*fd));
		fd->fd = -1;
		switch_mutex_init(&fd->mutex, SWITCH_MUTEX_NESTED, globals.pool);
		fd->path = switch_core_strdup(globals.pool, path);
		switch_core_hash_insert(globals.fd_hash, path, fd);
	}

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

	if ((bytes_in = write(fd->fd, log_line, bytes_out)) != bytes_out) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Write error to file %s %d/%d\n", path, (int) bytes_in, (int) bytes_out);
	}

	fd->bytes += bytes_in;

  end:

	switch_mutex_unlock(fd->mutex);
}

static int save_cdr(const char* const table, const char* const template, const char* const cdr)
{
	char* columns;
	char* values;
	char* p;
	unsigned clen;
        unsigned vlen;
	char* query;
	const char* const query_template = "INSERT INTO %s (%s) VALUES (%s);";
	PGresult* res;

	if (!table || !*table || !template || !*template || !cdr || !*cdr) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Bad parameter\n");
		return 0;
	}

	columns = strdup(template);
	for (p = columns; *p; ++p) {
		switch (*p) {
		case '$': case '"': case '{': case '}': case ';':
			*p = ' ';
			break;
		}
	}
	clen = p - columns;

	values = strdup(cdr);
	for (p = values; *p; ++p) {
		switch(*p) {
		case '"':
			*p = '\'';
			break;
		case ';':
			*p = ' ';
			break;
		}
	}
	vlen = p - values;
/*
         Patch for changing spaces (; ;) in the template paterns to NULL   (ex.)  ; ; --PACH--> null
         - added new functionality - space removing
*/
        char *spaceColumns;
	int spaceCounter=0;
	for (p=columns; *p; ++p)
	{
		if (*p==' ')
		{
			spaceCounter++;
		}
	}
	spaceColumns = (char*)malloc(clen-spaceCounter+1);
	char *pt=spaceColumns;
	for (p=columns; *p; ++p)
	{
		if (*p!=' ')
		{
			*pt=*p;
			pt++;
		}
	}
	*pt=0;
	pt=columns;
	columns=spaceColumns;
	free(pt);
	char *nullValues;
	int nullCounter=0;
	int charCounter=0;
	for (p=values; *p; ++p)
	{
		if (*p==',')
		{
			if (charCounter==0)
			{
				nullCounter++;
			}
			charCounter=0;
		}
		else if (*p!=' ' && *p!='\'')
		{
			charCounter++;
		}
	}
	if (charCounter==0)
	{
		nullCounter++;
	}
	charCounter=0;
	nullCounter*=4;
	vlen+=nullCounter;
	nullValues=(char*)malloc(strlen(values)+nullCounter+1);
	charCounter=0;
	char *temp=nullValues;
	char *tp=nullValues;
	for (p=values; *p; ++tp,++p)
	{
	    if (*p==',')
		{
			if (charCounter==0)
			{
				temp++;
				*temp='n';temp++;
				if (temp==tp) tp++;
				*temp='u';temp++;
				if (temp==tp) tp++;
				*temp='l';temp++;
				if (temp==tp) tp++;
				*temp='l';temp++;
				while (temp!=tp)
				{
					*temp=' ';temp++;
				}
			}
			charCounter=0;
			temp=tp;
		}
		else if (*p!=' ' && *p!='\'')
		{
			charCounter++;
		}
		*tp=*p;
	}
	if (charCounter==0)
	{
		temp++;
		*temp='n';temp++;
		if (temp==tp) tp++;
		*temp='u';temp++;
		if (temp==tp) tp++;
		*temp='l';temp++;
		if (temp==tp) tp++;
		*temp='l';temp++;
		while (temp!=tp)
		{
			*temp=' ';temp++;
		}
	}
	charCounter=0;
	temp=tp;
	*tp=0;
	tp=values;
	values=nullValues;
	free(tp);
//-----------------------------END_OF_PATCH----------------------------------------------------------------
	query = malloc(strlen(query_template) - 6 + strlen(table) + clen + vlen + 1);
	sprintf(query, query_template, table, columns, values);
	free(columns);
	free(values);
	if (globals.debug) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Query: \"%s\"\n", query);
	}

	switch_mutex_lock(globals.db_mutex);

	if (!globals.db_online || PQstatus(globals.db_connection) != CONNECTION_OK) {
		globals.db_connection = PQconnectdb(globals.db_info);
	}
	if (PQstatus(globals.db_connection) == CONNECTION_OK) {
		globals.db_online = 1;
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Connection to database failed: %s", PQerrorMessage(globals.db_connection));
		PQfinish(globals.db_connection);
		globals.db_online = 0;
		switch_mutex_unlock(globals.db_mutex);
		free(query);
		return 0;
	}

	res = PQexec(globals.db_connection, "BEGIN");
	if (PQresultStatus(res) != PGRES_COMMAND_OK) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "BEGIN command failed: %s", PQerrorMessage(globals.db_connection));
		PQclear(res);
		PQfinish(globals.db_connection);
		globals.db_online = 0;
		switch_mutex_unlock(globals.db_mutex);
		free(query);
		return 0;
	}
	PQclear(res);

	res = PQexec(globals.db_connection, query);
	if (PQresultStatus(res) != PGRES_COMMAND_OK) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "INSERT command failed: %s", PQerrorMessage(globals.db_connection));
		PQclear(res);
		PQfinish(globals.db_connection);
		globals.db_online = 0;
		switch_mutex_unlock(globals.db_mutex);
		free(query);
		return 0;
	}
	PQclear(res);

	free(query);

	res = PQexec(globals.db_connection, "END");
	if (PQresultStatus(res) != PGRES_COMMAND_OK) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "END command failed: %s", PQerrorMessage(globals.db_connection));
		PQclear(res);
		PQfinish(globals.db_connection);
		globals.db_online = 0;
		switch_mutex_unlock(globals.db_mutex);
		return 0;
	}
	PQclear(res);

	switch_mutex_unlock(globals.db_mutex);

	return 1;
}

static switch_status_t my_on_hangup(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	const char *log_dir = NULL, *accountcode = NULL, *a_template_str = NULL, *g_template_str = NULL;
	char *log_line, *path = NULL;
	int saved = 0;

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

	if (!(log_dir = switch_channel_get_variable(channel, "cdr_pg_csv_base"))) {
		log_dir = globals.log_dir;
	}

	if (switch_dir_make_recursive(log_dir, SWITCH_DEFAULT_DIR_PERMS, switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error creating %s\n", log_dir);
		return SWITCH_STATUS_FALSE;
	}

	if (globals.debug) {
		switch_event_t *event;
		if (switch_event_create(&event, SWITCH_EVENT_COMMAND) == SWITCH_STATUS_SUCCESS) {
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
		g_template_str = "\"${accountcode}\",\"${caller_id_number}\",\"${destination_number}\",\"${context}\",\"${caller_id}\",\"${channel_name}\",\"${bridge_channel}\",\"${last_app}\",\"${last_arg}\",\"${start_stamp}\",\"${answer_stamp}\",\"${end_stamp}\",\"${duration}\",\"${billsec}\",\"${hangup_cause}\",\"${amaflags}\",\"${uuid}\",\"${userfield}\";";
	}

	if (!a_template_str) {
		a_template_str = g_template_str;
	}

	log_line = switch_channel_expand_variables(channel, a_template_str);

	saved = 1; // save_cdr(globals.a_table, a_template_str, log_line);

	if (!saved && accountcode) {
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

	saved = save_cdr(globals.g_table, g_template_str, log_line);

	if (!saved) {
		path = switch_mprintf("%s%sMaster.csv", log_dir, SWITCH_PATH_SEPARATOR);
		assert(path);
		write_cdr(path, log_line);
		free(path);
	}

	if (log_line != g_template_str) {
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

	if (globals.shutdown) {
		return;
	}

	if (sig && !strcmp(sig, "HUP")) {
		for (hi = switch_hash_first(NULL, globals.fd_hash); hi; hi = switch_hash_next(hi)) {
			switch_hash_this(hi, NULL, NULL, &val);
			fd = (cdr_fd_t *) val;
			switch_mutex_lock(fd->mutex);
			do_rotate(fd);
			switch_mutex_unlock(fd->mutex);
		}
		if (globals.db_online) {
			PQfinish(globals.db_connection);
			globals.db_online = 0;
		}

	}
}


static switch_state_handler_table_t state_handlers = {
	/*.on_init */ NULL,
	/*.on_routing */ NULL,
	/*.on_execute */ NULL,
	/*.on_hangup */ my_on_hangup,
	/*.on_exchange_media */ NULL,
	/*.on_soft_execute */ NULL
};



static switch_status_t load_config(switch_memory_pool_t *pool)
{
	char *cf = "cdr_pg_csv.conf";
	switch_xml_t cfg, xml, settings, param;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (globals.db_online) {
		PQfinish(globals.db_connection);
		switch_mutex_destroy(globals.db_mutex);
		globals.db_online = 0;
	}

	memset(&globals, 0, sizeof(globals));
	switch_core_hash_init(&globals.fd_hash, pool);
	switch_core_hash_init(&globals.template_hash, pool);
	switch_mutex_init(&globals.db_mutex, SWITCH_MUTEX_NESTED, pool);

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
					globals.log_dir = switch_core_sprintf(pool, "%s%scdr-pg-csv", val, SWITCH_PATH_SEPARATOR);
				} else if (!strcasecmp(var, "rotate-on-hup")) {
					globals.rotate = switch_true(val);
				} else if (!strcasecmp(var, "default-template")) {
					globals.default_template = switch_core_strdup(pool, val);
				} else if (!strcasecmp(var, "a-table")) {
					globals.a_table = switch_core_strdup(pool, val);
				} else if (!strcasecmp(var, "g-table")) {
					globals.g_table = switch_core_strdup(pool, val);
				} else if (!strcasecmp(var, "db-info")) {
					globals.db_info = switch_core_strdup(pool, val);
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
		globals.log_dir = switch_core_sprintf(pool, "%s%scdr-pg-csv", SWITCH_GLOBAL_dirs.log_dir, SWITCH_PATH_SEPARATOR);
	}

	if (zstr(globals.a_table)) {
		globals.a_table = switch_core_strdup(pool, "a");
	}

	if (zstr(globals.g_table)) {
		globals.g_table = switch_core_strdup(pool, "g");
	}

	if (zstr(globals.db_info)) {
		globals.db_info = switch_core_strdup(pool, "dbname = cdr");
	}

	return status;
}


SWITCH_MODULE_LOAD_FUNCTION(mod_cdr_pg_csv_load)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (switch_event_bind(modname, SWITCH_EVENT_TRAP, SWITCH_EVENT_SUBCLASS_ANY, event_handler, NULL) != SWITCH_STATUS_SUCCESS) {
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


SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_cdr_pg_csv_shutdown)
{

	globals.shutdown = 1;

	if (globals.db_online) {
		PQfinish(globals.db_connection);
		globals.db_online = 0;
	}

	switch_event_unbind_callback(event_handler);
	switch_core_remove_state_handler(&state_handlers);


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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
