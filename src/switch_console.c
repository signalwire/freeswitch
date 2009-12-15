/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2009, Anthony Minessale II <anthm@freeswitch.org>
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
 * Anthony Minessale II <anthm@freeswitch.org>
 *
 *
 * switch_console.c -- Simple Console
 *
 */

#include <switch.h>
#include <switch_console.h>
#include <switch_version.h>
#define CMD_BUFLEN 1024;

#ifdef SWITCH_HAVE_LIBEDIT
#include <histedit.h>

static EditLine *el;
static History *myhistory;
static HistEvent ev;
static char *hfile = NULL;


/*
 * store a strdup() of the string configured in XML
 * bound to each of the 12 function key
 */
static char *console_fnkeys[12];

/*
 * Load from console.conf XML file the section:
 * <keybindings>
 * <key name="1" value="show calls"/>
 * </keybindings>
 */
static switch_status_t console_xml_config(void)
{
	char *cf = "switch.conf";
	switch_xml_t cfg, xml, settings, param;

	/* clear the keybind array */
	int i;

	for (i = 0; i < 12; i++) {
		console_fnkeys[i] = NULL;
	}

	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", cf);
		return SWITCH_STATUS_TERM;
	}

	if ((settings = switch_xml_child(cfg, "cli-keybindings"))) {
		for (param = switch_xml_child(settings, "key"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");
			int i = atoi(var);
			if ((i < 1) || (i > 12)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Keybind %s is invalid, range is from 1 to 12\n", var);
			} else {
				/* Add the command to the fnkey array */
				console_fnkeys[i - 1] = switch_core_permanent_strdup(val);
			}
		}
	}

	switch_xml_free(xml);

	return SWITCH_STATUS_SUCCESS;
}
#else

#define CC_NORM         0
#define CC_NEWLINE      1
#define CC_EOF          2
#define CC_ARGHACK      3
#define CC_REFRESH      4
#define CC_CURSOR       5
#define CC_ERROR        6
#define CC_FATAL        7
#define CC_REDISPLAY    8
#define CC_REFRESH_BEEP 9

#endif

SWITCH_DECLARE_NONSTD(switch_status_t) switch_console_stream_raw_write(switch_stream_handle_t *handle, uint8_t *data, switch_size_t datalen)
{
	switch_size_t need = handle->data_len + datalen;
	
	if (need >= handle->data_size) {
		void *new_data;
		need += handle->alloc_chunk;

		if (!(new_data = realloc(handle->data, need))) {
			return SWITCH_STATUS_MEMERR;
		}

		handle->data = new_data;
		handle->data_size = need;
	}

	memcpy((uint8_t *) (handle->data) + handle->data_len, data, datalen);
	handle->data_len += datalen;
	handle->end = (uint8_t *) (handle->data) + handle->data_len;
	*(uint8_t *)handle->end = '\0';

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE_NONSTD(switch_status_t) switch_console_stream_write(switch_stream_handle_t *handle, const char *fmt, ...)
{
	va_list ap;
	char *buf = handle->data;
	char *end = handle->end;
	int ret = 0;
	char *data = NULL;

	if (handle->data_len >= handle->data_size) {
		return SWITCH_STATUS_FALSE;
	}

	va_start(ap, fmt);
	//ret = switch_vasprintf(&data, fmt, ap);
	if (!(data = switch_vmprintf(fmt, ap))) {
		ret = -1;
	}
	va_end(ap);

	if (data) {
		switch_size_t remaining = handle->data_size - handle->data_len;
		switch_size_t need = strlen(data) + 1;

		if ((remaining < need) && handle->alloc_len) {
			switch_size_t new_len;
			void *new_data;

			new_len = handle->data_size + need + handle->alloc_chunk;
			if ((new_data = realloc(handle->data, new_len))) {
				handle->data_size = handle->alloc_len = new_len;
				handle->data = new_data;
				buf = handle->data;
				remaining = handle->data_size - handle->data_len;
				handle->end = (uint8_t *) (handle->data) + handle->data_len;
				end = handle->end;
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Memory Error!\n");
				free(data);
				return SWITCH_STATUS_FALSE;
			}
		}

		if (remaining < need) {
			ret = -1;
		} else {
			ret = 0;
			switch_snprintf(end, remaining, "%s", data);
			handle->data_len = strlen(buf);
			handle->end = (uint8_t *) (handle->data) + handle->data_len;
		}
		free(data);
	}

	return ret ? SWITCH_STATUS_FALSE : SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_stream_write_file_contents(switch_stream_handle_t *stream, const char *path)
{
	char *dpath = NULL;
	int fd;
	switch_status_t status = SWITCH_STATUS_FALSE;
	
	if (!switch_is_file_path(path)) {
		dpath = switch_mprintf("%s%s%s", SWITCH_GLOBAL_dirs.conf_dir, SWITCH_PATH_SEPARATOR, path);
		path = dpath;
	}
	
	if ((fd = open(path, O_RDONLY)) > -1) {
		char buf[2048] = { 0 };
		while (switch_fd_read_line(fd, buf, sizeof(buf))) {
			stream->write_function(stream, "%s", buf);
		}
		close(fd);
		status = SWITCH_STATUS_SUCCESS;
	}
	
	switch_safe_free(dpath);
	return status;
}

static int alias_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	char **r = (char **) pArg;
	*r = strdup(argv[0]);
	return -1;
}

char *expand_alias(char *cmd, char *arg)
{
	char *errmsg = NULL;
	char *r = NULL;
	char *sql = NULL;
	char *exp = NULL;
	switch_cache_db_handle_t *db = NULL;
	int full = 0;


	switch_core_db_handle(&db);


	if (db->type == SCDB_TYPE_CORE_DB) {
		sql = switch_mprintf("select command from aliases where alias='%q'", cmd);
	} else {
		sql = switch_mprintf("select command from aliases where alias='%w'", cmd);
	}

	switch_cache_db_execute_sql_callback(db, sql, alias_callback, &r, &errmsg);

	if (errmsg) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "error [%s][%s]\n", sql, errmsg);
		free(errmsg);
	}

	switch_safe_free(sql);

	if (!r) {
		if (db->type == SCDB_TYPE_CORE_DB) {
			sql = switch_mprintf("select command from aliases where alias='%q %q'", cmd, arg);
		} else {
			sql = switch_mprintf("select command from aliases where alias='%w %w'", cmd, arg);
		}

		switch_cache_db_execute_sql_callback(db, sql, alias_callback, &r, &errmsg);

		if (errmsg) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "error [%s][%s]\n", sql, errmsg);
			free(errmsg);
		}
		if (r) {
			full++;
		}
	}

	switch_safe_free(sql);

	if (r) {
		if (arg && !full) {
			exp = switch_mprintf("%s %s", r, arg);
			free(r);
		} else {
			exp = r;
		}
	} else {
		exp = cmd;
	}

	switch_cache_db_release_db_handle(&db);

	return exp;
}

static int switch_console_process(char *cmd, int rec)
{
	char *arg = NULL, *alias = NULL;
	switch_stream_handle_t stream = { 0 };

	if (!strcmp(cmd, "shutdown") || !strcmp(cmd, "...")) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Bye!\n");
		return 0;
	}
	if (!strcmp(cmd, "version")) {
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_CONSOLE, "FreeSWITCH Version %s\n", SWITCH_VERSION_FULL);
		return 1;
	}
	if ((arg = strchr(cmd, '\r')) != 0 || (arg = strchr(cmd, '\n')) != 0) {
		*arg = '\0';
		arg = NULL;
	}
	if ((arg = strchr(cmd, ' ')) != 0) {
		*arg++ = '\0';
	}

	if (!rec && (alias = expand_alias(cmd, arg)) && alias != cmd) {
		int r = switch_console_process(alias, ++rec);
		free(alias);
		return r;
	}

	SWITCH_STANDARD_STREAM(stream);
	if (stream.data) {
		FILE *handle = switch_core_get_console();

		if (switch_api_execute(cmd, arg, NULL, &stream) == SWITCH_STATUS_SUCCESS) {
			if (handle) {
				fprintf(handle, "API CALL [%s(%s)] output:\n%s\n", cmd, arg ? arg : "", (char *) stream.data);
				fflush(handle);
			}
		} else {
			if (handle) {
				fprintf(handle, "Unknown Command: %s\n", cmd);
				fflush(handle);
			}
		}
		free(stream.data);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Memory Error!\n");
	}

	return 1;
}

SWITCH_DECLARE(void) switch_console_printf(switch_text_channel_t channel, const char *file, const char *func, int line, const char *fmt, ...)
{
	char *data = NULL;
	int ret = 0;
	va_list ap;
	FILE *handle = switch_core_data_channel(channel);
	const char *filep = switch_cut_path(file);
	char date[80] = "";
	switch_size_t retsize;
	switch_time_exp_t tm;
	switch_event_t *event;

	va_start(ap, fmt);
	ret = switch_vasprintf(&data, fmt, ap);
	va_end(ap);

	if (ret == -1) {
		fprintf(stderr, "Memory Error\n");
		goto done;
	}

	if (channel == SWITCH_CHANNEL_ID_LOG_CLEAN) {
		fprintf(handle, "%s", data);
		goto done;
	}

	switch_time_exp_lt(&tm, switch_micro_time_now());
	switch_strftime_nocheck(date, &retsize, sizeof(date), "%Y-%m-%d %T", &tm);

	if (channel == SWITCH_CHANNEL_ID_LOG) {
		fprintf(handle, "[%d] %s %s:%d %s() %s", (int) getpid(), date, filep, line, func, data);
		goto done;
	}

	if (channel == SWITCH_CHANNEL_ID_EVENT &&
		switch_event_running() == SWITCH_STATUS_SUCCESS && switch_event_create(&event, SWITCH_EVENT_LOG) == SWITCH_STATUS_SUCCESS) {

		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Log-Data", data);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Log-File", filep);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Log-Function", func);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Log-Line", "%d", line);
		switch_event_fire(&event);
	}

  done:
	if (data) {
		free(data);
	}
	fflush(handle);
}

static char hostname[256] = "";
static int32_t running = 1;



struct helper {
	int len;
	int hits;
	int words;
	char last[512];
	char partial[512];
	FILE *out;
	switch_stream_handle_t *stream;
	switch_xml_t xml;
	int xml_off;
};

static int comp_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	struct helper *h = (struct helper *) pArg;
	char *target = NULL, *str = NULL, *cur = NULL;
	switch_size_t x, y, i;

	
	if (argc > 0) target = argv[0];
	if (argc > 1) str = argv[1];
	if (argc > 2) cur = argv[2];

	if (cur) {
		while (*cur == ' ') cur++;
	}

	if (zstr(cur)) cur = NULL;
	if (zstr(str)) str = NULL;

	if (!target) {
		return -1;
	}

	if (!zstr(target) && *target == ':' && *(target+1) == ':') {
		char *r_argv[1] = { 0 }, *r_cols[1] = { 0 };
		switch_console_callback_match_t *matches;
		if (switch_console_run_complete_func(target, str, cur, &matches) == SWITCH_STATUS_SUCCESS) {
			switch_console_callback_match_node_t *m;
			for (m = matches->head; m; m = m->next) {
				if (!cur || !strncmp(m->val, cur, strlen(cur))) {
					r_argv[0] = m->val;
					comp_callback(h, 1, r_argv, r_cols);
				}
			}
			switch_console_free_matches(&matches);
		}
		return 0;
	}

	if (!zstr(target)) {
		if (h->out) {
			fprintf(h->out, "[%20s]\t", target);
		}
		if (h->stream) {
			h->stream->write_function(h->stream, "[%20s]\t", target);
		}
		if (h->xml) {
			switch_xml_set_txt_d(switch_xml_add_child_d(h->xml, "match", h->xml_off++), target);
		}

		switch_copy_string(h->last, target, sizeof(h->last));
		h->hits++;
	}
	
	x = strlen(h->last);
	y = strlen(h->partial);

	if (h->hits > 1) {
		for(i = 0; i < x && i < y; i++) {
			if (h->last[i] != h->partial[i]) {
				h->partial[i] = '\0';
				break;
			}
		}	
	} else if (h->hits == 1) {
		switch_copy_string(h->partial, target, sizeof(h->last));
	}

	if (!zstr(target)) {
		if ((h->hits % 4) == 0) {
			if (h->out) {
				fprintf(h->out, "\n");
			}
			if (h->stream) {
				h->stream->write_function(h->stream, "\n");
			}
		}
	}

	return 0;
}



struct match_helper {
	switch_console_callback_match_t *my_matches;
};

static int uuid_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	struct match_helper *h = (struct match_helper *) pArg;

	switch_console_push_match(&h->my_matches, argv[0]);
	return 0;

}

SWITCH_DECLARE(switch_status_t) switch_console_list_uuid(const char *line, const char *cursor, switch_console_callback_match_t **matches)
{
	char *sql;
	struct match_helper h = { 0 };
	switch_cache_db_handle_t *db = NULL;
	switch_status_t status = SWITCH_STATUS_FALSE;
	char *errmsg;
	
	switch_core_db_handle(&db);

	if (!zstr(cursor)) {
		sql = switch_mprintf("select distinct uuid from channels where uuid like '%q%%' and hostname='%q' order by uuid", 
							 cursor, switch_core_get_variable("hostname"));
	} else {
		sql = switch_mprintf("select distinct uuid from channels where hostname='%q' order by uuid", 
							 switch_core_get_variable("hostname"));
	}

	switch_cache_db_execute_sql_callback(db, sql, uuid_callback, &h, &errmsg);
	free(sql);

	switch_cache_db_release_db_handle(&db);

	if (h.my_matches) {
		*matches = h.my_matches;
		status = SWITCH_STATUS_SUCCESS;
	}


	return status;
}


SWITCH_DECLARE(unsigned char) switch_console_complete(const char *line, const char *cursor, FILE *console_out, 
													  switch_stream_handle_t *stream, switch_xml_t xml)
{
	switch_cache_db_handle_t *db = NULL;
	char *sql = NULL;
	char *dup = strdup(line);
	char *buf = dup;
	char *p, *lp = NULL;
	char *errmsg = NULL;
	struct helper h = { 0 };
	unsigned char ret = CC_REDISPLAY;
	int pos = 0;

#ifndef SWITCH_HAVE_LIBEDIT
	if (!stream) {
		return CC_ERROR;
	}
#endif

	switch_core_db_handle(&db);
	
	if (!zstr(cursor) && !zstr(line)) {
		pos = (cursor - line);
	}
	
	h.out = console_out;
	h.stream = stream;
	h.xml = xml;

	if (pos > 0) {
		*(buf + pos) = '\0';
	}

	if ((p = strchr(buf, '\r')) || (p = strchr(buf, '\n'))) {
		*p = '\0';
	}

	while (*buf == ' ') {
		buf++;
	}

	for (p = buf; p && *p; p++) {
		if (*p == ' ') {
			lp = p;
			h.words++;
			while(*p == ' ') p++;
		}
	}

	if (lp) {
		buf = lp + 1;
	}

	h.len = strlen(buf);
	
	if (h.out) {
		fprintf(h.out, "\n\n");
	}

	if (h.stream) {
		h.stream->write_function(h.stream, "\n\n");
	}

	if (h.words == 0) {
		sql = switch_mprintf("select distinct name from interfaces where type='api' and name like '%q%%' and hostname='%q' order by name", 
							 buf, switch_core_get_variable("hostname"));
	}
	
	if (sql) {
		switch_cache_db_execute_sql_callback(db, sql, comp_callback, &h, &errmsg);

		if (errmsg) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "error [%s][%s]\n", sql, errmsg);
			free(errmsg);
			ret = CC_ERROR;
			goto end;
		}
		free(sql);
		sql = NULL;
	}

	if (h.hits != -1) {
		char *dupdup = strdup(dup);
		int x, argc = 0;
		char *argv[10] = { 0 };
		switch_stream_handle_t stream = { 0 };
		SWITCH_STANDARD_STREAM(stream);
		switch_assert(dupdup);

		argc = switch_separate_string(dupdup, ' ', argv, (sizeof(argv) / sizeof(argv[0])));

		if (h.words == 0) {
			stream.write_function(&stream, "select distinct a1 from complete where " "a1 not in (select name from interfaces where hostname='%s') %s ", 
								  switch_core_get_variable("hostname"), argc ? "and" : "");
		} else {
			if (db->type == SCDB_TYPE_CORE_DB) {
				stream.write_function(&stream, "select distinct a%d,'%q','%q' from complete where ", h.words + 1, switch_str_nil(dup), switch_str_nil(lp));
			} else {
				stream.write_function(&stream, "select distinct a%d,'%q','%w' from complete where ", h.words + 1, switch_str_nil(dup), switch_str_nil(lp));
			}
		}

		for (x = 0; x < argc && x < 11; x++) {
			if (h.words + 1 > argc) {
				if (db->type == SCDB_TYPE_CORE_DB) {
					stream.write_function(&stream, "(a%d like '::%%' or a%d = '' or a%d = '%q')%q", 
										  x + 1, x + 1, x + 1, switch_str_nil(argv[x]), x == argc - 1 ? "" : " and ");
				} else {
					stream.write_function(&stream, "(a%d like '::%%' or a%d = '' or a%d = '%w')%w", 
										  x + 1, x + 1, x + 1, switch_str_nil(argv[x]), x == argc - 1 ? "" : " and ");
				}
			} else {
				if (db->type == SCDB_TYPE_CORE_DB) {
					stream.write_function(&stream, "(a%d like '::%%' or a%d = '' or a%d like '%q%%')%q",
										  x + 1, x + 1, x + 1, switch_str_nil(argv[x]), x == argc - 1 ? "" : " and ");
				} else {
					stream.write_function(&stream, "(a%d like '::%%' or a%d = '' or a%d like '%w%%')%w",
										  x + 1, x + 1, x + 1, switch_str_nil(argv[x]), x == argc - 1 ? "" : " and ");
				}
			}
		}

		stream.write_function(&stream, " and hostname='%s'", switch_core_get_variable("hostname"));
		switch_cache_db_execute_sql_callback(db, stream.data, comp_callback, &h, &errmsg);
		
		if (errmsg) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "error [%s][%s]\n", (char *) stream.data, errmsg);
			free(errmsg);
			ret = CC_ERROR;
		}

		switch_safe_free(dupdup);
		switch_safe_free(stream.data);

		if (ret == CC_ERROR) {
			goto end;
		}
	}

	if (h.out) {
		fprintf(h.out, "\n\n");
	}
	
	if (h.stream) {
		h.stream->write_function(h.stream, "\n\n");
		if (h.hits == 1 && !zstr(h.last)) {
			h.stream->write_function(h.stream, "write=%d:%s ", h.len, h.last);
		} else if (h.hits > 1 && !zstr(h.partial)) {
			h.stream->write_function(h.stream, "write=%d:%s", h.len, h.partial);
		}
	} 
	
	if (h.xml) {
		switch_xml_t x_write = switch_xml_add_child_d(h.xml, "write", h.xml_off++);
		char buf[32];
		
		snprintf(buf, sizeof(buf), "%d", h.len);
		switch_xml_set_attr_d_buf(x_write, "length", buf);
		
		if (h.hits == 1 && !zstr(h.last)) {
			switch_xml_set_txt_d(x_write, h.last);
		} else if (h.hits > 1 && !zstr(h.partial)) {
			switch_xml_set_txt_d(x_write, h.partial);
		}
	}

#ifdef SWITCH_HAVE_LIBEDIT
	if (h.out) {
		if (h.hits == 1 && !zstr(h.last)) {
			el_deletestr(el, h.len);
			el_insertstr(el, h.last);
			el_insertstr(el, " ");
		} else if (h.hits > 1 && !zstr(h.partial)) {
			el_deletestr(el, h.len);
			el_insertstr(el, h.partial);
		}
	}
#endif

  end:

	if (h.out) {
		fflush(h.out);
	}

	switch_safe_free(sql);
	switch_safe_free(dup);

	switch_cache_db_release_db_handle(&db);

	return (ret);
}




#ifdef SWITCH_HAVE_LIBEDIT
static char prompt_str[512] = "";

/*
 * If a fnkey is configured then process the command
 */
static unsigned char console_fnkey_pressed(int i)
{
	char *c, *cmd;

	assert((i > 0) && (i <= 12));

	c = console_fnkeys[i - 1];

	/* This new line is necessary to avoid output to begin after the ">" of the CLI's prompt */
	switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_CONSOLE, "\n");

	if (c == NULL) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "FUNCTION KEY F%d IS NOT BOUND, please edit switch.conf XML file\n", i);
		return CC_REDISPLAY;
	}

	cmd = strdup(c);
	switch_console_process(cmd, 0);
	free(cmd);

	return CC_REDISPLAY;
}

static unsigned char console_f1key(EditLine * el, int ch)
{
	return console_fnkey_pressed(1);
}
static unsigned char console_f2key(EditLine * el, int ch)
{
	return console_fnkey_pressed(2);
}
static unsigned char console_f3key(EditLine * el, int ch)
{
	return console_fnkey_pressed(3);
}
static unsigned char console_f4key(EditLine * el, int ch)
{
	return console_fnkey_pressed(4);
}
static unsigned char console_f5key(EditLine * el, int ch)
{
	return console_fnkey_pressed(5);
}
static unsigned char console_f6key(EditLine * el, int ch)
{
	return console_fnkey_pressed(6);
}
static unsigned char console_f7key(EditLine * el, int ch)
{
	return console_fnkey_pressed(7);
}
static unsigned char console_f8key(EditLine * el, int ch)
{
	return console_fnkey_pressed(8);
}
static unsigned char console_f9key(EditLine * el, int ch)
{
	return console_fnkey_pressed(9);
}
static unsigned char console_f10key(EditLine * el, int ch)
{
	return console_fnkey_pressed(10);
}
static unsigned char console_f11key(EditLine * el, int ch)
{
	return console_fnkey_pressed(11);
}
static unsigned char console_f12key(EditLine * el, int ch)
{
	return console_fnkey_pressed(12);
}


char *prompt(EditLine * e)
{
	if (*prompt_str == '\0') {
		switch_snprintf(prompt_str, sizeof(prompt_str), "freeswitch@%s> ", hostname);
	}

	return prompt_str;
}

static void *SWITCH_THREAD_FUNC console_thread(switch_thread_t *thread, void *obj)
{
	int count;
	const char *line;
	switch_memory_pool_t *pool = (switch_memory_pool_t *) obj;

	while (running) {
		int32_t arg = 0;

		if (getppid() == 1) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "We've become an orphan, no more console for us.\n");
			break;
		}
		
		switch_core_session_ctl(SCSC_CHECK_RUNNING, &arg);
		if (!arg) {
			break;
		}

		line = el_gets(el, &count);

		if (count > 1) {
			if (!zstr(line)) {
				char *cmd = strdup(line);
				char *p;
				const LineInfo *lf = el_line(el);
				char *foo = (char *) lf->buffer;
				if ((p = strrchr(cmd, '\r')) || (p = strrchr(cmd, '\n'))) {
					*p = '\0';
				}
				assert(cmd != NULL);
				history(myhistory, &ev, H_ENTER, line);
				running = switch_console_process(cmd, 0);
				el_deletestr(el, strlen(foo) + 1);
				memset(foo, 0, strlen(foo));
				free(cmd);
			}
		}
		switch_cond_next();
	}

	switch_core_destroy_memory_pool(&pool);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Editline thread exiting\n");
	return NULL;
}

static unsigned char complete(EditLine * el, int ch)
{
	const LineInfo *lf = el_line(el);

	return switch_console_complete(lf->buffer, lf->cursor, switch_core_get_console(), NULL, NULL);
}


SWITCH_DECLARE(void) switch_console_loop(void)
{
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;
	switch_memory_pool_t *pool;

	gethostname(hostname, sizeof(hostname));

	if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Pool Failure\n");
		return;
	}

	el = el_init(__FILE__, switch_core_get_console(), switch_core_get_console(), switch_core_get_console());
	el_set(el, EL_PROMPT, &prompt);
	el_set(el, EL_EDITOR, "emacs");
	/* AGX: Bind Keyboard function keys. This has been tested with:
	 * - linux console keyabord
	 * - putty.exe connected via ssh to linux
	 */
	/* Load/Init the config first */
	console_xml_config();
	/* Bind the functions to the key */
	el_set(el, EL_ADDFN, "f1-key", "F1 KEY PRESS", console_f1key);
	el_set(el, EL_ADDFN, "f2-key", "F2 KEY PRESS", console_f2key);
	el_set(el, EL_ADDFN, "f3-key", "F3 KEY PRESS", console_f3key);
	el_set(el, EL_ADDFN, "f4-key", "F4 KEY PRESS", console_f4key);
	el_set(el, EL_ADDFN, "f5-key", "F5 KEY PRESS", console_f5key);
	el_set(el, EL_ADDFN, "f6-key", "F6 KEY PRESS", console_f6key);
	el_set(el, EL_ADDFN, "f7-key", "F7 KEY PRESS", console_f7key);
	el_set(el, EL_ADDFN, "f8-key", "F8 KEY PRESS", console_f8key);
	el_set(el, EL_ADDFN, "f9-key", "F9 KEY PRESS", console_f9key);
	el_set(el, EL_ADDFN, "f10-key", "F10 KEY PRESS", console_f10key);
	el_set(el, EL_ADDFN, "f11-key", "F11 KEY PRESS", console_f11key);
	el_set(el, EL_ADDFN, "f12-key", "F12 KEY PRESS", console_f12key);

	el_set(el, EL_BIND, "\033OP", "f1-key", NULL);
	el_set(el, EL_BIND, "\033OQ", "f2-key", NULL);
	el_set(el, EL_BIND, "\033OR", "f3-key", NULL);
	el_set(el, EL_BIND, "\033OS", "f4-key", NULL);


	el_set(el, EL_BIND, "\033[11~", "f1-key", NULL);
	el_set(el, EL_BIND, "\033[12~", "f2-key", NULL);
	el_set(el, EL_BIND, "\033[13~", "f3-key", NULL);
	el_set(el, EL_BIND, "\033[14~", "f4-key", NULL);
	el_set(el, EL_BIND, "\033[15~", "f5-key", NULL);
	el_set(el, EL_BIND, "\033[17~", "f6-key", NULL);
	el_set(el, EL_BIND, "\033[18~", "f7-key", NULL);
	el_set(el, EL_BIND, "\033[19~", "f8-key", NULL);
	el_set(el, EL_BIND, "\033[20~", "f9-key", NULL);
	el_set(el, EL_BIND, "\033[21~", "f10-key", NULL);
	el_set(el, EL_BIND, "\033[23~", "f11-key", NULL);
	el_set(el, EL_BIND, "\033[24~", "f12-key", NULL);


	el_set(el, EL_ADDFN, "ed-complete", "Complete argument", complete);
	el_set(el, EL_BIND, "^I", "ed-complete", NULL);

	myhistory = history_init();
	if (myhistory == 0) {
		fprintf(stderr, "history could not be initialized\n");
		return;
	}

	hfile = switch_mprintf("%s%sfreeswitch.history", SWITCH_GLOBAL_dirs.log_dir, SWITCH_PATH_SEPARATOR);
	assert(hfile != NULL);

	history(myhistory, &ev, H_SETSIZE, 800);
	el_set(el, EL_HIST, history, myhistory);
	history(myhistory, &ev, H_LOAD, hfile);

	el_source(el, NULL);

	switch_threadattr_create(&thd_attr, pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&thread, thd_attr, console_thread, pool, pool);

	while (running) {
		int32_t arg = 0;
		switch_core_session_ctl(SCSC_CHECK_RUNNING, &arg);
		if (!arg) {
			break;
		}
		switch_yield(1000000);
	}

	history(myhistory, &ev, H_SAVE, hfile);
	free(hfile);

	/* Clean up our memory */
	history_end(myhistory);
	el_end(el);
}

#else

SWITCH_DECLARE(void) switch_console_loop(void)
{

	char cmd[2048] = "";
	int32_t activity = 1;	
#ifndef _MSC_VER
	int x = 0;
#endif	

	gethostname(hostname, sizeof(hostname));

	while (running) {
		int32_t arg;
#ifdef _MSC_VER
		DWORD read, i;
		HANDLE stdinHandle = GetStdHandle(STD_INPUT_HANDLE);
		INPUT_RECORD in[128];
#else
		fd_set rfds, efds;
		struct timeval tv = { 0, 20000 };
#endif

		switch_core_session_ctl(SCSC_CHECK_RUNNING, &arg);
		if (!arg) {
			break;
		}

		if (activity) {
			switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_CONSOLE, "\nfreeswitch@%s> ", hostname);
		}
#ifdef _MSC_VER
		activity = 0;
		PeekConsoleInput(stdinHandle, in, 128, &read);
		for (i = 0; i < read; i++) {
			if (in[i].EventType == KEY_EVENT && !in[i].Event.KeyEvent.bKeyDown) {
				activity = 1;
				break;
			}
		}

		if (activity) {
			DWORD bytes = 0;
			char *end;
			ReadConsole(stdinHandle, cmd, sizeof(cmd), &bytes, NULL);
			FlushConsoleInputBuffer(stdinHandle);
			end = end_of_p(cmd);
			while(*end == '\r' || *end == '\n') {
				*end-- = '\0';	
			}
		}

		if (cmd[0]) {
			running = switch_console_process(cmd, 0);
			memset(cmd, 0, sizeof(cmd));
		}
		Sleep(20);
#else
		FD_ZERO(&rfds);
		FD_ZERO(&efds);
		FD_SET(fileno(stdin), &rfds);
		FD_SET(fileno(stdin), &efds);
		if ((activity = select(fileno(stdin) + 1, &rfds, NULL, &efds, &tv)) < 0) {
			break;
		}

		if (FD_ISSET(fileno(stdin), &efds)) {
			continue;
		}
		
		if (!FD_ISSET(fileno(stdin), &rfds)) {
			activity = 0;
		}

		if (activity == 0) {
			fflush(stdout);
			continue;
		}

		memset(&cmd, 0, sizeof(cmd));
		for (x = 0; x < (sizeof(cmd) - 1); x++) {
			int c = getchar();
			if (c < 0) {
				int y = read(fileno(stdin), cmd, sizeof(cmd) - 1);
				cmd[y - 1] = '\0';
				break;
			}

			cmd[x] = (char) c;

			if (cmd[x] == '\n') {
				cmd[x] = '\0';
				break;
			}
		}

		if (cmd[0]) {
			running = switch_console_process(cmd, 0);
		}
#endif
	}
}



#endif


static struct {
	switch_hash_t *func_hash;
	switch_mutex_t *func_mutex;
} globals;

SWITCH_DECLARE(switch_status_t) switch_console_init(switch_memory_pool_t *pool)
{
	switch_mutex_init(&globals.func_mutex, SWITCH_MUTEX_NESTED, pool);
	switch_core_hash_init(&globals.func_hash, pool);
	switch_console_add_complete_func("::console::list_uuid", (switch_console_complete_callback_t)switch_console_list_uuid);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_console_shutdown(void)
{
	return switch_core_hash_destroy(&globals.func_hash);
}

SWITCH_DECLARE(switch_status_t) switch_console_add_complete_func(const char *name, switch_console_complete_callback_t cb)
{
	switch_status_t status;
	
	switch_mutex_lock(globals.func_mutex);
	status = switch_core_hash_insert(globals.func_hash, name, (void *)(intptr_t)cb);
	switch_mutex_unlock(globals.func_mutex);

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_console_del_complete_func(const char *name)
{
	switch_status_t status;
	
	switch_mutex_lock(globals.func_mutex);
	status = switch_core_hash_insert(globals.func_hash, name, NULL);
	switch_mutex_unlock(globals.func_mutex);

    return status;
}

SWITCH_DECLARE(void) switch_console_free_matches(switch_console_callback_match_t **matches)
{
	switch_console_callback_match_t *my_match = *matches;
	switch_console_callback_match_node_t *m, *cur;

	/* Don't play with matches */
	*matches = NULL;

	m = my_match->head;
	while(m) {
		cur = m;
		m = m->next;
		free(cur->val);
		free(cur);
	}

	if (my_match->dynamic) {
		free(my_match);
	}
}

SWITCH_DECLARE(void) switch_console_sort_matches(switch_console_callback_match_t *matches)
{
	switch_console_callback_match_node_t *p = NULL, *sort[4] = { 0 };
	int i, j;

	switch_assert(matches);

	if (matches->count < 2) {
		return;
	}
	
	for(i = 1; i < matches->count; i++) {
		sort[0] = NULL;
		sort[1] = matches->head;
		sort[2] = sort[1] ? sort[1]->next : NULL;
		sort[3] = sort[2] ? sort[2]->next : NULL;		

		for(j = 1; j <= (matches->count - i); j++) {
			if (strcmp(sort[1]->val, sort[2]->val) > 0) {
				sort[1]->next = sort[3];
				sort[2]->next = sort[1];

				if (sort[0]) sort[0]->next = sort[2];
				if (sort[1] == matches->head) matches->head = sort[2];
				
				


				sort[0] = sort[2];
				sort[2] = sort[1]->next;
				if (sort[3] && sort[3]->next) sort[3] = sort[3]->next;
				
			} else {
				sort[0] = sort[1];
				sort[1] = sort[2];
				sort[2] = sort[3];
				if (sort[3] && sort[3]->next) sort[3] = sort[3]->next;
			}
			
		}
	}
	
	p = matches->head;

	for(i = 1; i < matches->count; i++) p = p->next;

	if (p) {
		p->next = NULL;
		matches->end = p;
	}
}

SWITCH_DECLARE(void) switch_console_push_match(switch_console_callback_match_t **matches, const char *new_val)
{
	switch_console_callback_match_node_t *match;

	if (!*matches) {
		switch_zmalloc(*matches, sizeof(**matches));
		(*matches)->dynamic = 1;
	}

	switch_zmalloc(match, sizeof(*match));
	match->val = strdup(new_val);

	if ((*matches)->head) {
		(*matches)->end->next = match;
	} else {
		(*matches)->head = match;
	}

	(*matches)->count++;

	(*matches)->end = match;
}

SWITCH_DECLARE(switch_status_t) switch_console_run_complete_func(const char *func, const char *line, const char *last_word,
																 switch_console_callback_match_t **matches)
{
	switch_console_complete_callback_t cb;
	switch_status_t status = SWITCH_STATUS_FALSE;

	switch_mutex_lock(globals.func_mutex);
	if ((cb = (switch_console_complete_callback_t)(intptr_t)switch_core_hash_find(globals.func_hash, func))) {
		if ((status = cb(line, last_word, matches)) == SWITCH_STATUS_SUCCESS) {
			switch_console_sort_matches(*matches);
		}
	}
	switch_mutex_unlock(globals.func_mutex);

	return status;
}


SWITCH_DECLARE(switch_status_t) switch_console_set_complete(const char *string)
{
	char *mydata = NULL, *argv[11] = { 0 };
	int argc, x;
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (string && (mydata = strdup(string))) {
		if ((argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0]))))) {
			switch_cache_db_handle_t *db = NULL;
			switch_stream_handle_t mystream = { 0 };
			SWITCH_STANDARD_STREAM(mystream);

			switch_core_db_handle(&db);

			if (!strcasecmp(argv[0], "stickyadd")) {
				mystream.write_function(&mystream, "insert into complete values (1,");
				for (x = 0; x < 10; x++) {
					if (argv[x + 1] && !strcasecmp(argv[x + 1], "_any_")) {
						mystream.write_function(&mystream, "%s", "'', ");
					} else {
						if (db->type == SCDB_TYPE_CORE_DB) {
							mystream.write_function(&mystream, "'%q', ", switch_str_nil(argv[x + 1]));
						} else {
							mystream.write_function(&mystream, "'%w', ", switch_str_nil(argv[x + 1]));
						}
					}
				}
				mystream.write_function(&mystream, " '%s')", switch_core_get_variable("hostname"));
				switch_cache_db_persistant_execute(db, mystream.data, 5);
				status = SWITCH_STATUS_SUCCESS;
			} else if (!strcasecmp(argv[0], "add")) {
				mystream.write_function(&mystream, "insert into complete values (0,");
				for (x = 0; x < 10; x++) {
					if (argv[x + 1] && !strcasecmp(argv[x + 1], "_any_")) {
						mystream.write_function(&mystream, "%s", "'', ");
					} else {
						if (db->type == SCDB_TYPE_CORE_DB) {
							mystream.write_function(&mystream, "'%q', ", switch_str_nil(argv[x + 1]));
						} else {
							mystream.write_function(&mystream, "'%w', ", switch_str_nil(argv[x + 1]));
						}
					}
				}
				mystream.write_function(&mystream, " '%s')", switch_core_get_variable("hostname"));
				
				switch_cache_db_persistant_execute(db, mystream.data, 5);
				status = SWITCH_STATUS_SUCCESS;
			} else if (!strcasecmp(argv[0], "del")) {
				char *what = argv[1];
				if (!strcasecmp(what, "*")) {
					switch_cache_db_persistant_execute(db, "delete from complete", 1);
				} else {
					mystream.write_function(&mystream, "delete from complete where ");
					for (x = 0; x < argc - 1; x++) {
						if (db->type == SCDB_TYPE_CORE_DB) {
							mystream.write_function(&mystream, "a%d = '%q'%q", x + 1, switch_str_nil(argv[x + 1]), x == argc - 2 ? "" : " and ");
						} else {
							mystream.write_function(&mystream, "a%d = '%w'%w", x + 1, switch_str_nil(argv[x + 1]), x == argc - 2 ? "" : " and ");
						}
					}
					mystream.write_function(&mystream, " and hostname='%s'", switch_core_get_variable("hostname"));
					switch_cache_db_persistant_execute(db, mystream.data, 1);
				}
				status = SWITCH_STATUS_SUCCESS;
			}

			switch_safe_free(mystream.data);
			switch_cache_db_release_db_handle(&db);
		}
	}

	switch_safe_free(mydata);

	return status;

}


SWITCH_DECLARE(switch_status_t) switch_console_set_alias(const char *string)
{
	char *mydata = NULL, *argv[3] = { 0 };
	int argc;
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (string && (mydata = strdup(string))) {
		if ((argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])))) >= 2) {
			switch_cache_db_handle_t *db = NULL;
			char *sql = NULL;

			switch_core_db_handle(&db);

			if (!strcasecmp(argv[0], "stickyadd") && argc == 3) {
				sql = switch_mprintf("delete from aliases where alias='%q' and hostname='%q'", argv[1], switch_core_get_variable("hostname"));
				switch_cache_db_persistant_execute(db, sql, 5);
				switch_safe_free(sql);
				if (db->type == SCDB_TYPE_CORE_DB) {
					sql = switch_mprintf("insert into aliases (sticky, alias, command, hostname) values (1, '%q','%q','%q')", 
										 argv[1], argv[2], switch_core_get_variable("hostname"));
				} else {
					sql = switch_mprintf("insert into aliases (sticky, alias, command, hostname) values (1, '%w','%w','%w')", 
										 argv[1], argv[2], switch_core_get_variable("hostname"));
				}
				switch_cache_db_persistant_execute(db, sql, 5);
				status = SWITCH_STATUS_SUCCESS;
			} else if (!strcasecmp(argv[0], "add") && argc == 3) {
				sql = switch_mprintf("delete from aliases where alias='%q' and hostname='%q'", argv[1], switch_core_get_variable("hostname"));
				switch_cache_db_persistant_execute(db, sql, 5);
				switch_safe_free(sql);
				if (db->type == SCDB_TYPE_CORE_DB) {
					sql = switch_mprintf("insert into aliases (sticky, alias, command, hostname) values (0, '%q','%q','%q')", 
										 argv[1], argv[2], switch_core_get_variable("hostname"));
				} else {
					sql = switch_mprintf("insert into aliases (sticky, alias, command, hostname) values (0, '%w','%w','%w')", 
										 argv[1], argv[2], switch_core_get_variable("hostname"));
				}
				switch_cache_db_persistant_execute(db, sql, 5);
				status = SWITCH_STATUS_SUCCESS;
			} else if (!strcasecmp(argv[0], "del") && argc == 2) {
				char *what = argv[1];
				if (!strcasecmp(what, "*")) {
					sql = switch_mprintf("delete from aliases where hostname='%q'", switch_core_get_variable("hostname"));
					switch_cache_db_persistant_execute(db, sql, 1);
				} else {
					sql = switch_mprintf("delete from aliases where alias='%q' and hostname='%q'", argv[1], switch_core_get_variable("hostname"));
					switch_cache_db_persistant_execute(db, sql, 5);
				}
				status = SWITCH_STATUS_SUCCESS;
			}
			switch_safe_free(sql);
			switch_cache_db_release_db_handle(&db);
		}
	}

	switch_safe_free(mydata);

	return status;

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
