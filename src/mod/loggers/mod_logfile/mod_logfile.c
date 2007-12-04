/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
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
 * Anthony LaMantia <anthony@petabit.net>
 * Michael Jerris <mike@jerris.com>
 *
 *
 * mod_logfile.c -- Filesystem Logging
 *
 */

#include <switch.h>

SWITCH_MODULE_LOAD_FUNCTION(mod_logfile_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_logfile_shutdown);
SWITCH_MODULE_DEFINITION(mod_logfile, mod_logfile_load, mod_logfile_shutdown, NULL);

#define DEFAULT_LIMIT    0xA00000 /* About 10 MB */
#define WARM_FUZZY_OFFSET 256
#define MAX_ROT 4096 /* why not */
static const uint8_t STATIC_LEVELS[] = {0, 1, 2, 3, 4, 5, 6, 7, 8};

static switch_status_t load_config(void);
static switch_memory_pool_t *module_pool = NULL;

static struct {
	unsigned int log_fd;
	switch_size_t log_size;   /* keep the log size in check for rotation */
	switch_size_t roll_size;  /* the size that we want to rotate the file at */
	char *logfile;
	switch_file_t    *log_afd;
} globals;

SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_logfile, globals.logfile);

/* i know this is strange but it's the fastest way i could think of managing log levels. */
/* i'd rather not try to search something each time we get a message to log */
struct level_set {
	int level;
	int on;
} static levels[] = {
	{SWITCH_LOG_CONSOLE, 0},   /* 0 */
	{SWITCH_LOG_ALERT,   0},   /* 1 */
	{SWITCH_LOG_CRIT,    0},   /* 2 */
	{SWITCH_LOG_ERROR,   0},   /* 3 */
	{SWITCH_LOG_WARNING, 0},   /* 4 */
	{SWITCH_LOG_NOTICE,  0},   /* 5 */
	{SWITCH_LOG_INFO,    0},   /* 6 */
	{SWITCH_LOG_DEBUG,   0},   /* 7 */
};

void process_levels(char *p)
{
	int x, argc = 0;
	char *argv[10] = { 0 };

	if ((argc = switch_separate_string(p, ',', argv, (sizeof(argv) / sizeof(argv[0]))))) {
		for (x = 0; x < argc; x++) {
			if (!strncasecmp(argv[x], "alert", strlen(argv[x]))) {
				levels[SWITCH_LOG_ALERT].on = 1;
			} else if (!strncasecmp(argv[x], "crit", strlen(argv[x]))) {
				levels[SWITCH_LOG_CRIT].on = 1;
			} else if (!strncasecmp(argv[x], "error", strlen(argv[x]))) {
				levels[SWITCH_LOG_ERROR].on = 1;
			} else if (!strncasecmp(argv[x], "warn", strlen(argv[x]))) {
				levels[SWITCH_LOG_WARNING].on = 1;
			} else if (!strncasecmp(argv[x], "notice", strlen(argv[x]))) {
				levels[SWITCH_LOG_NOTICE].on = 1;
			} else if (!strncasecmp(argv[x], "info", strlen(argv[x]))) {
				levels[SWITCH_LOG_INFO].on = 1;
			} else if (!strncasecmp(argv[x], "debug", strlen(argv[x]))) {
				levels[SWITCH_LOG_DEBUG].on = 1;
			} else if (!strncasecmp(argv[x], "console", strlen(argv[x]))) {
				levels[SWITCH_LOG_CONSOLE].on = 1;
			} else if (!strncasecmp(argv[x], "all", strlen(argv[x]))) {
				int i;
				for (i=0; i < (sizeof(levels) / sizeof(struct level_set)); i++) {
					levels[i].on = 1;
				}
			}
		}
	}
	return;
}

static switch_status_t mod_logfile_rotate(void);

static switch_status_t mod_logfile_openlogfile(switch_bool_t check)
{
	unsigned int flags = 0;
	switch_file_t *afd;
	switch_status_t stat;

	flags |= SWITCH_FOPEN_CREATE;
	flags |= SWITCH_FOPEN_READ;
	flags |= SWITCH_FOPEN_WRITE;
	flags |= SWITCH_FOPEN_APPEND;

	stat = switch_file_open(&afd, globals.logfile, flags, SWITCH_FPROT_UREAD|SWITCH_FPROT_UWRITE, module_pool);
	if (stat != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_FALSE;
	}

	globals.log_afd = afd;

	globals.log_size = switch_file_get_size(globals.log_afd);

	if (check && globals.log_size >= globals.roll_size) {
		mod_logfile_rotate();
	}

	return SWITCH_STATUS_SUCCESS;
}

/* rotate the log file */
static switch_status_t mod_logfile_rotate(void)
{
	unsigned int i = 0;
	char *p = NULL;
	switch_status_t stat = 0;
	int64_t offset = 0;
    switch_memory_pool_t *pool;
    switch_time_exp_t tm;
    char date[80] = "";
    switch_size_t retsize;

    switch_time_exp_lt(&tm, switch_time_now());
    switch_strftime(date, &retsize, sizeof(date), "%Y-%m-%d-%H-%M-%S", &tm);

	globals.log_size = 0;

	stat = switch_file_seek(globals.log_afd, SWITCH_SEEK_SET, &offset);

	if (stat != SWITCH_STATUS_SUCCESS)
		return SWITCH_STATUS_FALSE;

	p = malloc(strlen(globals.logfile)+WARM_FUZZY_OFFSET);
    assert(p);

	memset(p, '\0', strlen(globals.logfile)+WARM_FUZZY_OFFSET);

    switch_core_new_memory_pool(&pool);
    
	for (i=1; i < MAX_ROT; i++) {
		sprintf((char *)p, "%s.%s.%i", globals.logfile, date, i);
        if (switch_file_exists(p, pool) == SWITCH_STATUS_SUCCESS) {
            continue;
        }
        
        switch_file_close(globals.log_afd);
        switch_file_rename(globals.logfile, p, pool);
        mod_logfile_openlogfile(SWITCH_FALSE);
        break;
	}

	free(p);

    switch_core_destroy_memory_pool(&pool);

	return SWITCH_STATUS_SUCCESS;
}

/* write to the actual logfile */
static switch_status_t mod_logfile_raw_write(char *log_data) 
{
	switch_size_t len;
	len = strlen(log_data);

	if (len <= 0)
		return SWITCH_STATUS_FALSE;

	if (switch_file_write(globals.log_afd, log_data, &len) != SWITCH_STATUS_SUCCESS) {
        switch_file_close(globals.log_afd);
        mod_logfile_openlogfile(SWITCH_TRUE);
    }
    
	globals.log_size += len;

	if (globals.log_size >= globals.roll_size) {
		mod_logfile_rotate();
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t mod_logfile_logger(const switch_log_node_t *node, switch_log_level_t level)
{

	if (levels[node->level].on) {
        mod_logfile_raw_write(node->data);
    }

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t load_config(void)
{
	char *cf = "logfile.conf";
	switch_xml_t cfg, xml, settings, param;

	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "open of %s failed\n", cf);
	} else {
		if ((settings = switch_xml_child(cfg, "settings"))) {
			for (param = switch_xml_child(settings, "param"); param; param = param->next) {
				char *var = (char *) switch_xml_attr_soft(param, "name");
				char *val = (char *) switch_xml_attr_soft(param, "value");
				if (!strcmp(var, "logfile")) {
					set_global_logfile(val);
				} else if (!strcmp(var, "level")) {
					process_levels(val);
				} else if (!strcmp(var, "rollover")) {
					globals.roll_size = atoi(val);
				}  
			}
		}
		switch_xml_free(xml);
	}
	if (globals.roll_size == 0) {
		globals.roll_size = DEFAULT_LIMIT;
	}
	if (switch_strlen_zero(globals.logfile)) {
		char logfile[512];
		snprintf(logfile, sizeof(logfile), "%s%s%s", SWITCH_GLOBAL_dirs.log_dir, SWITCH_PATH_SEPARATOR, "freeswitch.log");
		set_global_logfile(logfile);
	}

	return 0;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_logfile_load)
{
	switch_status_t status;

	module_pool = pool;

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	if ((status=load_config()) != SWITCH_STATUS_SUCCESS) {
		return status;
	}

	mod_logfile_openlogfile(SWITCH_TRUE);

	switch_log_bind_logger(mod_logfile_logger, SWITCH_LOG_DEBUG);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_logfile_shutdown)
{
	/* TODO:  Need to finish processing pending log messages before we close the file handle */

	//switch_file_close(globals.log_afd);	
	return SWITCH_STATUS_SUCCESS;
}

/* For Emacs:
* Local Variables:
* mode:c
* indent-tabs-mode:nil
* tab-width:4
* c-basic-offset:4
* End:
* For VIM:
* vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
*/
