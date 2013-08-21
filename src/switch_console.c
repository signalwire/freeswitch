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
 * Anthony Minessale II <anthm@freeswitch.org>
 *
 *
 * switch_console.c -- Simple Console
 *
 */

#include <switch.h>
#include <switch_console.h>
#include <switch_version.h>
#include <switch_private.h>
#define CMD_BUFLEN 1024

#ifdef SWITCH_HAVE_LIBEDIT
#include <histedit.h>

static EditLine *el;
static History *myhistory;
static HistEvent ev;
static char *hfile = NULL;

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

#ifdef _MSC_VER
#define HISTLEN 10
#define KEY_UP 1
#define KEY_DOWN 2
#define KEY_TAB 3
#define CLEAR_OP 4
#define DELETE_REFRESH_OP 5
#define KEY_LEFT 6
#define KEY_RIGHT 7
#define KEY_INSERT 8
#define PROMPT_OP 9
#define KEY_DELETE 10

static int console_bufferInput(char *buf, int len, char *cmd, int key);
#endif
#endif

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
			i = atoi(var);
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
	*(uint8_t *) handle->end = '\0';

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
	FILE *fd = NULL;
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (!switch_is_file_path(path)) {
		dpath = switch_mprintf("%s%s%s", SWITCH_GLOBAL_dirs.conf_dir, SWITCH_PATH_SEPARATOR, path);
		path = dpath;
	}

	if ((fd = fopen(path, "r"))) {
		char *line_buf = NULL;
		switch_size_t llen = 0;

		while (switch_fp_read_dline(fd, &line_buf, &llen)) {
			stream->write_function(stream, "%s", line_buf);
		}
		fclose(fd);
		switch_safe_free(line_buf);
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

SWITCH_DECLARE(char *) switch_console_expand_alias(char *cmd, char *arg)
{
	char *errmsg = NULL;
	char *r = NULL;
	char *sql = NULL;
	char *exp = NULL;
	switch_cache_db_handle_t *db = NULL;
	switch_core_flag_t cflags = switch_core_flags();
	int full = 0;

	
	if (!(cflags & SCF_USE_SQL)) {
		return NULL;
	}

	if (switch_core_db_handle(&db) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Database Error\n");
		return NULL;
	}


	if (switch_cache_db_get_type(db) == SCDB_TYPE_CORE_DB) {
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
		if (switch_cache_db_get_type(db) == SCDB_TYPE_CORE_DB) {
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


static int switch_console_process(char *xcmd)
{
	switch_stream_handle_t stream = { 0 };
	switch_status_t status;
	FILE *handle = switch_core_get_console();
	int r = 1;

	SWITCH_STANDARD_STREAM(stream);
	switch_assert(stream.data);

	status = switch_console_execute(xcmd, 0, &stream);

	if (status == SWITCH_STATUS_SUCCESS) {
		if (handle) {
			fprintf(handle, "\n%s\n", (char *) stream.data);
			fflush(handle);
		}
	} else {
		if (!strcasecmp(xcmd, "...") || !strcasecmp(xcmd, "shutdown")) {
			r = 0;
		}
		if (handle) {
			fprintf(handle, "Unknown Command: %s\n", xcmd);
			fflush(handle);
		}
	}

	switch_safe_free(stream.data);

	return r;

}


SWITCH_DECLARE(switch_status_t) switch_console_execute(char *xcmd, int rec, switch_stream_handle_t *istream)
{
	char *arg = NULL, *alias = NULL;

	char *delim = ";;";
	int argc;
	char *argv[128];
	int x;
	char *dup = strdup(xcmd);
	char *cmd;

	switch_status_t status = SWITCH_STATUS_FALSE;


	if (rec > 100) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Too much recursion!\n");
		goto end;
	}

	if (!strncasecmp(xcmd, "alias", 5)) {
		argc = 1;
		argv[0] = xcmd;
	} else {
		argc = switch_separate_string_string(dup, delim, argv, 128);
	}

	for (x = 0; x < argc; x++) {
		cmd = argv[x];
		if ((arg = strchr(cmd, '\r')) != 0 || (arg = strchr(cmd, '\n')) != 0) {
			*arg = '\0';
			arg = NULL;
		}
		if ((arg = strchr(cmd, ' ')) != 0) {
			*arg++ = '\0';
		}

		if ((alias = switch_console_expand_alias(cmd, arg)) && alias != cmd) {
			istream->write_function(istream, "\nExpand Alias [%s]->[%s]\n\n", cmd, alias);
			status = switch_console_execute(alias, ++rec, istream);
			free(alias);
			continue;
		}


		status = switch_api_execute(cmd, arg, NULL, istream);
	}

  end:

	switch_safe_free(dup);

	return status;
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


	if (argc > 0)
		target = argv[0];
	if (argc > 1)
		str = argv[1];
	if (argc > 2)
		cur = argv[2];

	if (cur) {
		while (*cur == ' ')
			cur++;
	}

	if (zstr(cur))
		cur = NULL;
	if (zstr(str))
		str = NULL;

	if (!target) {
		return -1;
	}

	if (!zstr(target) && *target == ':' && *(target + 1) == ':' && *(target + 2) == '[') {
		char *p = target + 3, *list = NULL;

		if (p) {
			char *argv[100] = { 0 };
			char *r_argv[1] = { 0 }, *r_cols[1] = {0};
			list = strdup(p);
			
			argc = switch_separate_string(list, ':', argv, (sizeof(argv) / sizeof(argv[0])));

			for (i = 0; (int)i < argc; i++) {
				if (!cur || !strncmp(argv[i], cur, strlen(cur))) {
					r_argv[0] = argv[i];
					comp_callback(h, 1, r_argv, r_cols);
				}
			}
			switch_safe_free(list);
		}
		return 0;
	}

	if (!zstr(target) && *target == ':' && *(target + 1) == ':') {
		char *r_argv[1] = { 0 }, *r_cols[1] = {0};
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
		for (i = 0; i < x && i < y; i++) {
			if (h->last[i] != h->partial[i]) {
				h->partial[i] = '\0';
				break;
			}
		}
	} else if (h->hits == 1) {
		switch_copy_string(h->partial, target, sizeof(h->last));
	}

	if (!zstr(target)) {
#ifdef _MSC_VER
		if ((h->hits % 3) == 0) {
#else
		if ((h->hits % 4) == 0) {
#endif
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

static int modulename_callback(void *pArg, const char *module_name)
{
	struct match_helper *h = (struct match_helper *) pArg;

	switch_console_push_match(&h->my_matches, module_name);
	return 0;
}

SWITCH_DECLARE_NONSTD(switch_status_t) switch_console_list_available_modules(const char *line, const char *cursor, switch_console_callback_match_t **matches)
{
	struct match_helper h = { 0 };

	if (switch_loadable_module_enumerate_available(SWITCH_GLOBAL_dirs.mod_dir, modulename_callback, &h) != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_GENERR;
	}

	if (h.my_matches) {
		*matches = h.my_matches;
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;
}

SWITCH_DECLARE_NONSTD(switch_status_t) switch_console_list_loaded_modules(const char *line, const char *cursor, switch_console_callback_match_t **matches)
{
	struct match_helper h = { 0 };

	if (switch_loadable_module_enumerate_loaded(modulename_callback, &h) != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_GENERR;
	}

	if (h.my_matches) {
		*matches = h.my_matches;
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;
}

#ifdef HAVE_GETIFADDRS
#include <ifaddrs.h>
#include <net/if.h>
SWITCH_DECLARE_NONSTD(switch_status_t) switch_console_list_interfaces(const char *line, const char *cursor, switch_console_callback_match_t **matches)
{
	struct match_helper h = { 0 };
	struct ifaddrs *addrs, *addr;

	getifaddrs(&addrs);
	for(addr = addrs; addr; addr = addr->ifa_next) {
		if (addr->ifa_flags & IFF_UP) {
			switch_console_push_match_unique(&h.my_matches, addr->ifa_name);
		}
	}
	freeifaddrs(addrs);

	if (h.my_matches) {
		*matches = h.my_matches;
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;
}
#else
SWITCH_DECLARE_NONSTD(switch_status_t) switch_console_list_interfaces(const char *line, const char *cursor, switch_console_callback_match_t **matches)
{
	return SWITCH_STATUS_FALSE;
}
#endif

static int uuid_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	struct match_helper *h = (struct match_helper *) pArg;

	switch_console_push_match(&h->my_matches, argv[0]);
	return 0;

}

SWITCH_DECLARE_NONSTD(switch_status_t) switch_console_list_uuid(const char *line, const char *cursor, switch_console_callback_match_t **matches)
{
	char *sql;
	struct match_helper h = { 0 };
	switch_cache_db_handle_t *db = NULL;
	switch_status_t status = SWITCH_STATUS_FALSE;
	char *errmsg;


	if (switch_core_db_handle(&db) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Database Error\n");
		return SWITCH_STATUS_GENERR;
	}

	if (!zstr(cursor)) {
		sql = switch_mprintf("select distinct uuid from channels where uuid like '%q%%' and hostname='%q' order by uuid",
							 cursor, switch_core_get_switchname());
	} else {
		sql = switch_mprintf("select distinct uuid from channels where hostname='%q' order by uuid", switch_core_get_switchname());
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


SWITCH_DECLARE(unsigned char) switch_console_complete(const char *line, const char *cursor, FILE * console_out,
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
	int sc = 0;

#ifndef SWITCH_HAVE_LIBEDIT
#ifndef _MSC_VER
	if (!stream) {
		return CC_ERROR;
	}
#endif
#endif

	if (switch_core_db_handle(&db) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Database Error\n");
		return CC_ERROR;
	}

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
		sc++;
		buf++;
	}

	if (!*buf) {
#ifdef SWITCH_HAVE_LIBEDIT
		if (h.out && sc) {
			el_deletestr(el, sc);
		}
#endif
	}

	sc = 0;
	p = end_of_p(buf);
	while (p >= buf && *p == ' ') {
		sc++;
		p--;
	}

	if (sc > 1) {
#ifdef SWITCH_HAVE_LIBEDIT
		if (h.out) {
			el_deletestr(el, sc - 1);
		}
#endif
		*(p + 2) = '\0';
	}

	for (p = buf; p && *p; p++) {
		if (*p == ' ') {
			lp = p;
			h.words++;
			while (*p == ' ')
				p++;
			if (!*p)
				break;
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
							 buf, switch_core_get_switchname());
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
								  switch_core_get_switchname(), argc ? "and" : "");
		} else {
			if (switch_cache_db_get_type(db) == SCDB_TYPE_CORE_DB) {
				stream.write_function(&stream, "select distinct a%d,'%q','%q' from complete where ", h.words + 1, switch_str_nil(dup), switch_str_nil(lp));
			} else {
				stream.write_function(&stream, "select distinct a%d,'%q','%w' from complete where ", h.words + 1, switch_str_nil(dup), switch_str_nil(lp));
			}
		}

		for (x = 0; x < argc && x < 11; x++) {
			if (h.words + 1 > argc) {
				if (switch_cache_db_get_type(db) == SCDB_TYPE_CORE_DB) {
					stream.write_function(&stream, "(a%d like '::%%' or a%d = '' or a%d = '%q')%q",
										  x + 1, x + 1, x + 1, switch_str_nil(argv[x]), x == argc - 1 ? "" : " and ");
				} else {
					stream.write_function(&stream, "(a%d like '::%%' or a%d = '' or a%d = '%w')%w",
										  x + 1, x + 1, x + 1, switch_str_nil(argv[x]), x == argc - 1 ? "" : " and ");
				}
			} else {
				if (switch_cache_db_get_type(db) == SCDB_TYPE_CORE_DB) {
					stream.write_function(&stream, "(a%d like '::%%' or a%d = '' or a%d like '%q%%')%q",
										  x + 1, x + 1, x + 1, switch_str_nil(argv[x]), x == argc - 1 ? "" : " and ");
				} else {
					stream.write_function(&stream, "(a%d like '::%%' or a%d = '' or a%d like '%w%%')%w",
										  x + 1, x + 1, x + 1, switch_str_nil(argv[x]), x == argc - 1 ? "" : " and ");
				}
			}
		}

		stream.write_function(&stream, " and hostname='%s' order by a%d", switch_core_get_switchname(), h.words + 1);
		
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
#else
#ifdef _MSC_VER
	if (h.out) {
		if (h.hits == 1 && !zstr(h.last)) {
			console_bufferInput(0, h.len, (char *) line, DELETE_REFRESH_OP);
			console_bufferInput(h.last, (int) strlen(h.last), (char *) line, 0);
			console_bufferInput(" ", 1, (char *) line, 0);
		} else if (h.hits > 1 && !zstr(h.partial)) {
			console_bufferInput(0, h.len, (char *) line, DELETE_REFRESH_OP);
			console_bufferInput(h.partial, (int) strlen(h.partial), (char *) line, 0);
		} else {
			console_bufferInput(0, 0, (char *) line, DELETE_REFRESH_OP);
		}
	}
#endif
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


#if defined(SWITCH_HAVE_LIBEDIT) || defined(_MSC_VER)
/*
 * If a fnkey is configured then process the command
 */
static unsigned char console_fnkey_pressed(int i)
{
	char *c, *cmd;

	switch_assert((i > 0) && (i <= 12));

	c = console_fnkeys[i - 1];

	/* This new line is necessary to avoid output to begin after the ">" of the CLI's prompt */
	switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_CONSOLE, "\n");

	if (c == NULL) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "FUNCTION KEY F%d IS NOT BOUND, please edit switch.conf XML file\n", i);
		return CC_REDISPLAY;
	}

	cmd = strdup(c);
	switch_console_process(cmd);
	free(cmd);

	return CC_REDISPLAY;
}
#endif

SWITCH_DECLARE(void) switch_console_save_history(void)
{
#ifdef SWITCH_HAVE_LIBEDIT
	history(myhistory, &ev, H_SAVE, hfile);
#else
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "NOT IMPLEMENTED!\n");
#endif
}

#ifdef SWITCH_HAVE_LIBEDIT
static char prompt_str[512] = "";

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
		switch_snprintf(prompt_str, sizeof(prompt_str), "freeswitch@%s> ", switch_core_get_switchname());
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
				running = switch_console_process(cmd);
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

	/* "Delete" key. */
	el_set(el, EL_BIND, "\033[3~", "ed-delete-next-char", NULL);

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

#ifdef _MSC_VER
char history[HISTLEN][CMD_BUFLEN + 1];
int iHistory = 0;
int iHistorySel = 0;

static int console_history(char *cmd, int direction)
{
	int i;
	static int first;

	if (direction == 0) {
		first = 1;
		if (iHistory < HISTLEN) {
			if (iHistory && strcmp(history[iHistory - 1], cmd)) {
				iHistorySel = iHistory;
				strcpy(history[iHistory++], cmd);
			} else if (iHistory == 0) {
				iHistorySel = iHistory;
				strcpy(history[iHistory++], cmd);
			}
		} else {
			iHistory = HISTLEN - 1;
			for (i = 0; i < HISTLEN - 1; i++) {
				strcpy(history[i], history[i + 1]);
			}
			iHistorySel = iHistory;
			strcpy(history[iHistory++], cmd);
		}
	} else {
		if (!first) {
			iHistorySel += direction;
		}
		first = 0;
		if (iHistorySel < 0) {
			iHistorySel = 0;
		}
		if (iHistory && iHistorySel >= iHistory) {
			iHistorySel = iHistory - 1;
		}
		strcpy(cmd, history[iHistorySel]);
	}
	return (SWITCH_STATUS_SUCCESS);
}

static int console_bufferInput(char *addchars, int len, char *cmd, int key)
{
	static int iCmdBuffer = 0;
	static int iCmdCursor = 0;
	static int ignoreNext = 0;
	static int insertMode = 1;
	static COORD orgPosition;
	static char prompt[80];
	int iBuf;
	int i;

	HANDLE hOut;
	CONSOLE_SCREEN_BUFFER_INFO info;
	COORD position;
	hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	GetConsoleScreenBufferInfo(hOut, &info);
	position = info.dwCursorPosition;
	if (iCmdCursor == 0) {
		orgPosition = position;
	}

	if (key == PROMPT_OP) {
		if (strlen(cmd) < sizeof(prompt)) {
			strcpy(prompt, cmd);
		}
		return 0;
	}

	if (key == KEY_TAB) {
		switch_console_complete(cmd, cmd + iCmdBuffer, switch_core_get_console(), NULL, NULL);
		return 0;
	}
	if (key == KEY_UP || key == KEY_DOWN || key == CLEAR_OP) {
		SetConsoleCursorPosition(hOut, orgPosition);
		for (i = 0; i < (int) strlen(cmd); i++) {
			printf(" ");
		}
		SetConsoleCursorPosition(hOut, orgPosition);
		iCmdBuffer = 0;
		iCmdCursor = 0;
		memset(cmd, 0, CMD_BUFLEN);
	}
	if (key == DELETE_REFRESH_OP) {
		int l = len < (int) strlen(cmd) ? len : (int) strlen(cmd);
		for (i = 0; i < l; i++) {
			cmd[--iCmdBuffer] = 0;
		}
		iCmdCursor = (int) strlen(cmd);
		printf("%s", prompt);
		GetConsoleScreenBufferInfo(hOut, &info);
		orgPosition = info.dwCursorPosition;
		printf("%s", cmd);
		return 0;
	}

	if (key == KEY_LEFT) {
		if (iCmdCursor) {
			if (position.X == 0) {
				position.Y -= 1;
				position.X = info.dwSize.X - 1;
			} else {
				position.X -= 1;
			}

			SetConsoleCursorPosition(hOut, position);
			iCmdCursor--;
		}
	}
	if (key == KEY_RIGHT) {
		if (iCmdCursor < (int) strlen(cmd)) {
			if (position.X == info.dwSize.X - 1) {
				position.Y += 1;
				position.X = 0;
			} else {
				position.X += 1;
			}

			SetConsoleCursorPosition(hOut, position);
			iCmdCursor++;
		}
	}
	if (key == KEY_INSERT) {
		insertMode = !insertMode;
	}
	if (key == KEY_DELETE) {
		if (iCmdCursor < iCmdBuffer) {
			int pos;
			for (pos = iCmdCursor; pos < iCmdBuffer; pos++) {
				cmd[pos] = cmd[pos + 1];
			}
			cmd[pos] = 0;
			iCmdBuffer--;

			for (pos = iCmdCursor; pos < iCmdBuffer; pos++) {
				printf("%c", cmd[pos]);
			}
			printf(" ");
			SetConsoleCursorPosition(hOut, position);
		}
	}
	for (iBuf = 0; iBuf < len; iBuf++) {
		switch (addchars[iBuf]) {
		case '\r':
		case '\n':
			if (ignoreNext) {
				ignoreNext = 0;
			} else {
				int ret = iCmdBuffer;
				if (iCmdBuffer == 0) {
					strcpy(cmd, "Empty");
					ret = (int) strlen(cmd);
				} else {
					console_history(cmd, 0);
					cmd[iCmdBuffer] = 0;
				}
				iCmdBuffer = 0;
				iCmdCursor = 0;
				printf("\n");
				return (ret);
			}
			break;
		case '\b':
			if (iCmdCursor) {
				if (position.X == 0) {
					position.Y -= 1;
					position.X = info.dwSize.X - 1;
					SetConsoleCursorPosition(hOut, position);
				} else {
					position.X -= 1;
					SetConsoleCursorPosition(hOut, position);
				}
				printf(" ");
				if (iCmdCursor < iCmdBuffer) {
					int pos;
					iCmdCursor--;
					for (pos = iCmdCursor; pos < iCmdBuffer; pos++) {
						cmd[pos] = cmd[pos + 1];
					}
					cmd[pos] = 0;
					iCmdBuffer--;

					SetConsoleCursorPosition(hOut, position);
					for (pos = iCmdCursor; pos < iCmdBuffer; pos++) {
						printf("%c", cmd[pos]);
					}
					printf(" ");
					SetConsoleCursorPosition(hOut, position);
				} else {
					SetConsoleCursorPosition(hOut, position);
					iCmdBuffer--;
					iCmdCursor--;
					cmd[iCmdBuffer] = 0;
				}
			}
			break;
		default:
			if (!ignoreNext) {
				if (iCmdCursor < iCmdBuffer) {
					int pos;

					if (position.X == info.dwSize.X - 1) {
						position.Y += 1;
						position.X = 0;
					} else {
						position.X += 1;
					}

					if (insertMode) {
						for (pos = iCmdBuffer - 1; pos >= iCmdCursor; pos--) {
							cmd[pos + 1] = cmd[pos];
						}
					}
					iCmdBuffer++;
					cmd[iCmdCursor++] = addchars[iBuf];
					printf("%c", addchars[iBuf]);
					for (pos = iCmdCursor; pos < iCmdBuffer; pos++) {
						GetConsoleScreenBufferInfo(hOut, &info);
						if (info.dwCursorPosition.X == info.dwSize.X - 1 && info.dwCursorPosition.Y == info.dwSize.Y - 1) {
							orgPosition.Y -= 1;
							position.Y -= 1;
						}
						printf("%c", cmd[pos]);
					}
					SetConsoleCursorPosition(hOut, position);
				} else {
					if (position.X == info.dwSize.X - 1 && position.Y == info.dwSize.Y - 1) {
						orgPosition.Y -= 1;
					}
					cmd[iCmdBuffer++] = addchars[iBuf];
					iCmdCursor++;
					printf("%c", addchars[iBuf]);
				}
			}
		}
		if (iCmdBuffer == CMD_BUFLEN) {
			printf("Read Console... BUFFER OVERRUN\n");
			iCmdBuffer = 0;
			ignoreNext = 1;
		}
	}
	return (SWITCH_STATUS_SUCCESS);
}


static BOOL console_readConsole(HANDLE conIn, char *buf, int len, int *pRed, int *key)
{
	DWORD recordIndex, bufferIndex, toRead = 0, red;
	PINPUT_RECORD pInput;

	GetNumberOfConsoleInputEvents(conIn, &toRead);
	if (len < (int) toRead) {
		toRead = len;
	}
	if (toRead == 0) {
		return (FALSE);
	}

	if ((pInput = (PINPUT_RECORD) malloc(toRead * sizeof(INPUT_RECORD))) == NULL) {
		return (FALSE);
	}
	*key = 0;
	ReadConsoleInput(conIn, pInput, toRead, &red);

	for (recordIndex = bufferIndex = 0; recordIndex < red; recordIndex++) {
		KEY_EVENT_RECORD keyEvent = pInput[recordIndex].Event.KeyEvent;
		if (pInput[recordIndex].EventType == KEY_EVENT && keyEvent.bKeyDown) {
			if (keyEvent.wVirtualKeyCode == 38 && keyEvent.wVirtualScanCode == 72) {
				buf[0] = 0;
				console_history(buf, -1);
				*key = KEY_UP;
				bufferIndex += (DWORD) strlen(buf);
			}
			if (keyEvent.wVirtualKeyCode == 40 && keyEvent.wVirtualScanCode == 80) {
				buf[0] = 0;
				console_history(buf, 1);
				*key = KEY_DOWN;
				bufferIndex += (DWORD) strlen(buf);
			}
			if (keyEvent.wVirtualKeyCode == 112 && keyEvent.wVirtualScanCode == 59) {
				console_fnkey_pressed(1);
			}
			if (keyEvent.wVirtualKeyCode == 113 && keyEvent.wVirtualScanCode == 60) {
				console_fnkey_pressed(2);
			}
			if (keyEvent.wVirtualKeyCode == 114 && keyEvent.wVirtualScanCode == 61) {
				console_fnkey_pressed(3);
			}
			if (keyEvent.wVirtualKeyCode == 115 && keyEvent.wVirtualScanCode == 62) {
				console_fnkey_pressed(4);
			}
			if (keyEvent.wVirtualKeyCode == 116 && keyEvent.wVirtualScanCode == 63) {
				console_fnkey_pressed(5);
			}
			if (keyEvent.wVirtualKeyCode == 117 && keyEvent.wVirtualScanCode == 64) {
				console_fnkey_pressed(6);
			}
			if (keyEvent.wVirtualKeyCode == 118 && keyEvent.wVirtualScanCode == 65) {
				console_fnkey_pressed(7);
			}
			if (keyEvent.wVirtualKeyCode == 119 && keyEvent.wVirtualScanCode == 66) {
				console_fnkey_pressed(8);
			}
			if (keyEvent.wVirtualKeyCode == 120 && keyEvent.wVirtualScanCode == 67) {
				console_fnkey_pressed(9);
			}
			if (keyEvent.wVirtualKeyCode == 121 && keyEvent.wVirtualScanCode == 68) {
				console_fnkey_pressed(10);
			}
			if (keyEvent.wVirtualKeyCode == 122 && keyEvent.wVirtualScanCode == 87) {
				console_fnkey_pressed(11);
			}
			if (keyEvent.wVirtualKeyCode == 123 && keyEvent.wVirtualScanCode == 88) {
				console_fnkey_pressed(12);
			}
			if (keyEvent.uChar.AsciiChar == 9) {
				*key = KEY_TAB;
				break;
			}
			if (keyEvent.uChar.AsciiChar == 27) {
				*key = CLEAR_OP;
				break;
			}
			if (keyEvent.wVirtualKeyCode == 37 && keyEvent.wVirtualScanCode == 75) {
				*key = KEY_LEFT;
			}
			if (keyEvent.wVirtualKeyCode == 39 && keyEvent.wVirtualScanCode == 77) {
				*key = KEY_RIGHT;
			}
			if (keyEvent.wVirtualKeyCode == 45 && keyEvent.wVirtualScanCode == 82) {
				*key = KEY_INSERT;
			}
			if (keyEvent.wVirtualKeyCode == 46 && keyEvent.wVirtualScanCode == 83) {
				*key = KEY_DELETE;
			}
			while (keyEvent.wRepeatCount && keyEvent.uChar.AsciiChar) {
				buf[bufferIndex] = keyEvent.uChar.AsciiChar;
				if (buf[bufferIndex] == '\r') {
					buf[bufferIndex] = '\n';
				}
				bufferIndex++;
				keyEvent.wRepeatCount--;
			}
		}
	}

	free(pInput);
	*pRed = bufferIndex;
	return (TRUE);
}
#endif


SWITCH_DECLARE(void) switch_console_loop(void)
{
	char cmd[CMD_BUFLEN + 1] = "";
	int32_t activity = 1;
#ifndef _MSC_VER
	int x = 0;
#else
	char keys[CMD_BUFLEN];
#endif

	/* Load/Init the config first */
	console_xml_config();

#ifdef _MSC_VER
	sprintf(cmd, "\nfreeswitch@%s> ", switch_core_get_switchname());
	console_bufferInput(0, 0, cmd, PROMPT_OP);
	memset(cmd, 0, sizeof(cmd));
#endif

	while (running) {
		int32_t arg;
#ifdef _MSC_VER
		int read, key;
		HANDLE stdinHandle = GetStdHandle(STD_INPUT_HANDLE);
#else
		fd_set rfds, efds;
		struct timeval tv = { 0, 20000 };
#endif

		switch_core_session_ctl(SCSC_CHECK_RUNNING, &arg);
		if (!arg) {
			break;
		}

		if (activity) {
			switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_CONSOLE, "\nfreeswitch@%s> ", switch_core_get_switchname());
		}
#ifdef _MSC_VER
		activity = 0;
		if (console_readConsole(stdinHandle, keys, (int) sizeof(keys), &read, &key)) {
			if (console_bufferInput(keys, read, cmd, key)) {
				if (!strcmp(cmd, "Empty")) {
					cmd[0] = 0;
				}
				activity = 1;
				if (cmd[0]) {
					running = switch_console_process(cmd);
				}
				memset(cmd, 0, sizeof(cmd));
			}
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
			running = switch_console_process(cmd);
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
	switch_console_add_complete_func("::console::list_available_modules", (switch_console_complete_callback_t) switch_console_list_available_modules);
	switch_console_add_complete_func("::console::list_loaded_modules", (switch_console_complete_callback_t) switch_console_list_loaded_modules);
	switch_console_add_complete_func("::console::list_interfaces", (switch_console_complete_callback_t) switch_console_list_interfaces);
	switch_console_add_complete_func("::console::list_uuid", (switch_console_complete_callback_t) switch_console_list_uuid);
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
	status = switch_core_hash_insert(globals.func_hash, name, (void *) (intptr_t) cb);
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
	while (m) {
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

	for (i = 1; i < matches->count; i++) {
		sort[0] = NULL;
		sort[1] = matches->head;
		sort[2] = sort[1] ? sort[1]->next : NULL;
		sort[3] = sort[2] ? sort[2]->next : NULL;

		for (j = 1; j <= (matches->count - i); j++) {
			switch_assert(sort[1] && sort[2]);
			if (strcmp(sort[1]->val, sort[2]->val) > 0) {
				sort[1]->next = sort[3];
				sort[2]->next = sort[1];

				if (sort[0])
					sort[0]->next = sort[2];
				if (sort[1] == matches->head)
					matches->head = sort[2];




				sort[0] = sort[2];
				sort[2] = sort[1]->next;
				if (sort[3] && sort[3]->next)
					sort[3] = sort[3]->next;

			} else {
				sort[0] = sort[1];
				sort[1] = sort[2];
				sort[2] = sort[3];
				if (sort[3] && sort[3]->next)
					sort[3] = sort[3]->next;
			}

		}
	}

	p = matches->head;

	for (i = 1; i < matches->count; i++)
		p = p->next;

	if (p) {
		p->next = NULL;
		matches->end = p;
	}
}

SWITCH_DECLARE(void) switch_console_push_match_unique(switch_console_callback_match_t **matches, const char *new_val)
{
	/* Ignore the entry if it is already in the list */
	if (*matches) {
		switch_console_callback_match_node_t *node;

		for(node = (*matches)->head; node; node = node->next) {
			if (!strcasecmp(node->val, new_val)) return;
		}
	}

	switch_console_push_match(matches, new_val);
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
	if ((cb = (switch_console_complete_callback_t) (intptr_t) switch_core_hash_find(globals.func_hash, func))) {
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
	switch_core_flag_t cflags = switch_core_flags();

	if (!(cflags & SCF_USE_SQL)) {
		return SWITCH_STATUS_FALSE;
	}

	if (string && (mydata = strdup(string))) {
		if ((argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0]))))) {
			switch_stream_handle_t mystream = { 0 };
			SWITCH_STANDARD_STREAM(mystream);
			
			if (!strcasecmp(argv[0], "stickyadd")) {
				mystream.write_function(&mystream, "insert into complete values (1,");
				for (x = 0; x < 10; x++) {
					if (argv[x + 1] && !strcasecmp(argv[x + 1], "_any_")) {
						mystream.write_function(&mystream, "%s", "'', ");
					} else {
						if (switch_core_dbtype() == SCDB_TYPE_CORE_DB) {
							mystream.write_function(&mystream, "'%q', ", switch_str_nil(argv[x + 1]));
						} else {
							mystream.write_function(&mystream, "'%w', ", switch_str_nil(argv[x + 1]));
						}
					}
				}
				mystream.write_function(&mystream, " '%s')", switch_core_get_switchname());
				switch_core_sql_exec(mystream.data);
				status = SWITCH_STATUS_SUCCESS;
			} else if (!strcasecmp(argv[0], "add")) {
				mystream.write_function(&mystream, "insert into complete values (0,");
				for (x = 0; x < 10; x++) {
					if (argv[x + 1] && !strcasecmp(argv[x + 1], "_any_")) {
						mystream.write_function(&mystream, "%s", "'', ");
					} else {
						if (switch_core_dbtype() == SCDB_TYPE_CORE_DB) {
							mystream.write_function(&mystream, "'%q', ", switch_str_nil(argv[x + 1]));
						} else {
							mystream.write_function(&mystream, "'%w', ", switch_str_nil(argv[x + 1]));
						}
					}
				}
				mystream.write_function(&mystream, " '%s')", switch_core_get_switchname());

				switch_core_sql_exec(mystream.data);
				status = SWITCH_STATUS_SUCCESS;
			} else if (!strcasecmp(argv[0], "del")) {
				char *what = argv[1];
				if (!strcasecmp(what, "*")) {
					switch_core_sql_exec("delete from complete");
				} else {
					mystream.write_function(&mystream, "delete from complete where ");
					for (x = 0; x < argc - 1; x++) {
						if (switch_core_dbtype() == SCDB_TYPE_CORE_DB) {
							mystream.write_function(&mystream, "a%d = '%q'%q", x + 1, switch_str_nil(argv[x + 1]), x == argc - 2 ? "" : " and ");
						} else {
							mystream.write_function(&mystream, "a%d = '%w'%w", x + 1, switch_str_nil(argv[x + 1]), x == argc - 2 ? "" : " and ");
						}
					}
					mystream.write_function(&mystream, " and hostname='%s'", switch_core_get_switchname());
					switch_core_sql_exec(mystream.data);
				}
				status = SWITCH_STATUS_SUCCESS;
			}

			switch_safe_free(mystream.data);
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

			if (switch_core_db_handle(&db) != SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Database Error\n");
				free(mydata);
				return SWITCH_STATUS_FALSE;
			}
			
			if (!strcasecmp(argv[0], "stickyadd") && argc == 3) {
				sql = switch_mprintf("delete from aliases where alias='%q' and hostname='%q'", argv[1], switch_core_get_switchname());
				switch_cache_db_persistant_execute(db, sql, 5);
				switch_safe_free(sql);
				if (switch_cache_db_get_type(db) == SCDB_TYPE_CORE_DB) {
					sql = switch_mprintf("insert into aliases (sticky, alias, command, hostname) values (1, '%q','%q','%q')",
										 argv[1], argv[2], switch_core_get_switchname());
				} else {
					sql = switch_mprintf("insert into aliases (sticky, alias, command, hostname) values (1, '%w','%w','%w')",
										 argv[1], argv[2], switch_core_get_switchname());
				}
				switch_cache_db_persistant_execute(db, sql, 5);
				status = SWITCH_STATUS_SUCCESS;
			} else if (!strcasecmp(argv[0], "add") && argc == 3) {
				sql = switch_mprintf("delete from aliases where alias='%q' and hostname='%q'", argv[1], switch_core_get_switchname());
				switch_cache_db_persistant_execute(db, sql, 5);
				switch_safe_free(sql);
				if (switch_cache_db_get_type(db) == SCDB_TYPE_CORE_DB) {
					sql = switch_mprintf("insert into aliases (sticky, alias, command, hostname) values (0, '%q','%q','%q')",
										 argv[1], argv[2], switch_core_get_switchname());
				} else {
					sql = switch_mprintf("insert into aliases (sticky, alias, command, hostname) values (0, '%w','%w','%w')",
										 argv[1], argv[2], switch_core_get_switchname());
				}
				switch_cache_db_persistant_execute(db, sql, 5);
				status = SWITCH_STATUS_SUCCESS;
			} else if (!strcasecmp(argv[0], "del") && argc == 2) {
				char *what = argv[1];
				if (!strcasecmp(what, "*")) {
					sql = switch_mprintf("delete from aliases where hostname='%q'", switch_core_get_switchname());
					switch_cache_db_persistant_execute(db, sql, 1);
				} else {
					sql = switch_mprintf("delete from aliases where alias='%q' and hostname='%q'", argv[1], switch_core_get_switchname());
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
