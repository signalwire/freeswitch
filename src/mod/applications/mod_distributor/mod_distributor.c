/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2014, Anthony Minessale II <anthm@freeswitch.org>
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
 * Neal Horman <neal at wanlink dot com>
 *
 *
 * mod_distributor.c -- Load distributor
 *
 */
#include <switch.h>

/* Prototypes */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_distributor_shutdown);
SWITCH_MODULE_RUNTIME_FUNCTION(mod_distributor_runtime);
SWITCH_MODULE_LOAD_FUNCTION(mod_distributor_load);

/* SWITCH_MODULE_DEFINITION(name, load, shutdown, runtime) 
 * Defines a switch_loadable_module_function_table_t and a static const char[] modname
 */
SWITCH_MODULE_DEFINITION(mod_distributor, mod_distributor_load, mod_distributor_shutdown, NULL);


struct dist_node {
	char *name;
	int weight;
	int cur_weight;
	int wval;
	struct dist_node *next;
};

struct dist_list {
	char *name;
	int target_weight;
	int last;
	int node_count;
	struct dist_node *lastnode;
	struct dist_node *nodes;
	struct dist_list *next;
};


static void destroy_node(struct dist_node *node)
{
	struct dist_node *old;
	while (node) {
		old = node;
		node = node->next;
		if (old->name) {
			free(old->name);
		}
		free(old);
	}
}

static void destroy_list(struct dist_list *list)
{
	struct dist_list *old;

	while (list) {
		old = list;
		list = list->next;
		destroy_node(old->nodes);
		if (old->name) {
			free(old->name);
		}
		free(old);
	}
}

static struct {
	switch_mutex_t *mod_lock;
	switch_memory_pool_t *pool;
	struct dist_list *list;
} globals;


static void calc_weight(struct dist_list *lp)
{
	struct dist_node *np;

	lp->target_weight = 0;

	for (np = lp->nodes; np; np = np->next) {
		lp->target_weight += np->wval;
	}

	for (np = lp->nodes; np; np = np->next) {
		np->weight = np->cur_weight = (lp->target_weight - np->wval);
	}
}


static int load_config(int reloading)
{
	struct dist_list *main_list = NULL, *new_list, *old_list = NULL, *lp = NULL;
	switch_status_t status = SWITCH_STATUS_FALSE;
	char *cf = "distributor.conf";
	switch_xml_t cfg, xml, lists, list, param;


	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", cf);
		return SWITCH_STATUS_TERM;
	}

	if (!(lists = switch_xml_child(cfg, "lists"))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't find any lists!\n");
		return status;
	}

	switch_mutex_lock(globals.mod_lock);

	for (list = switch_xml_child(lists, "list"); list; list = list->next) {
		const char *name = switch_xml_attr(list, "name");
		const char *tweight = switch_xml_attr(list, "total-weight");
		struct dist_node *node, *np = NULL;

		if (zstr(name)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Missing NAME!\n");
			continue;
		}

		if (!zstr(tweight)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "The total-weight attribute is no longer necessary.\n");
		}

		switch_zmalloc(new_list, sizeof(*new_list));

		new_list->name = strdup(name);
		new_list->last = -1;

		if (lp) {
			lp->next = new_list;
		} else {
			main_list = new_list;
		}

		lp = new_list;

		for (param = switch_xml_child(list, "node"); param; param = param->next) {
			char *name_attr = (char *) switch_xml_attr_soft(param, "name");
			char *weight_val = (char *) switch_xml_attr_soft(param, "weight");
			int tmp;

			if ((tmp = atoi(weight_val)) < 1) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Weight %d value incorrect, must be > 0\n", tmp);
				continue;
			}

			switch_zmalloc(node, sizeof(*node));
			node->name = strdup(name_attr);
			node->wval = tmp;
			
			if (np) {
				np->next = node;
			} else {
				lp->nodes = node;
			}

			np = node;
			lp->node_count++;
		}

		calc_weight(lp);

	}

	if (main_list) {
		old_list = globals.list;
		globals.list = main_list;
		status = SWITCH_STATUS_SUCCESS;
	}

	switch_mutex_unlock(globals.mod_lock);

	if (old_list) {
		destroy_list(old_list);
	}


	if (xml) {
		switch_xml_free(xml);
	}

	return status;
}

static int reset_list(struct dist_list *list)
{
	struct dist_node *np;

	for (np = list->nodes; np; np = np->next) {
		np->cur_weight = np->weight;
	}
	list->last = -1;
	list->lastnode = NULL;
	return 0;
}

static struct dist_node *find_next(struct dist_list *list, int etotal, char **exceptions)
{
	struct dist_node *np, *match = NULL;
	int x = 0, mx = 0;
	int matches = 0, loops = 0;

	for (;;) {
	top:
		
		if (++loops > 1000) {
			break;
		}

		x = 0;
		if (list->last >= list->node_count) {
			list->last = -1;
		}
		match = NULL;
		matches = 0;
		for (np = list->nodes; np; np = np->next) {
			if (np->cur_weight < list->target_weight) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG10, "%s %d/%d\n", np->name, np->cur_weight, list->target_weight);
				matches++;
				if (!match && x > list->last) {
					match = np;
					mx = x;
				}
			}
			x++;
		}

		if (match) {
			int i;
			
			match->cur_weight++;
			list->lastnode = match;
			list->last = mx;

			for(i = 0; i < etotal; i++) {
				if (!strcmp(match->name, exceptions[i])) {
					if (matches == 1) {
						reset_list(list);
					}
					goto top;
				}
			}
			

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG10, "Choose %s\n", match->name);
			return match;
		}

		if (matches) {
			list->last = -1;
		} else {
			reset_list(list);
		}

	}
	
	return NULL;
}


static char *dist_engine(const char *name)
{
	struct dist_node *np = NULL;
	struct dist_list *lp;
	char *str = NULL;
	char *myname = strdup(name);
	char *except;
	int argc = 0;
	char *argv[100] = { 0 };

	
	if ((except = strchr(myname, ' '))) {
		*except++ = '\0';
		argc = switch_split(except, ' ', argv);
	}

	switch_mutex_lock(globals.mod_lock);
	for (lp = globals.list; lp; lp = lp->next) {
		if (!strcasecmp(myname, lp->name)) {
			np = find_next(lp, argc, argv);
			break;
		}
	}

	if (np) {
		str = strdup(np->name);
	}
	switch_mutex_unlock(globals.mod_lock);

	free(myname);

	return str;

}

SWITCH_STANDARD_APP(distributor_exec)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	char *ret = NULL;

	if (zstr(data)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "distributor requires an argument\n");
		return;
	}

	if ((ret = dist_engine(data))) {
		switch_channel_set_variable(channel, "DISTRIBUTOR", ret);
		free(ret);
	}
}

SWITCH_STANDARD_API(distributor_function)
{
	char *ret = NULL;

	if (!zstr(cmd) && (ret = dist_engine(cmd))) {
		stream->write_function(stream, "%s", ret);
		free(ret);
	} else {
		stream->write_function(stream, "-err");
	}

	return SWITCH_STATUS_SUCCESS;

}

static struct dist_list *find_list(const char *name)
{
	struct dist_list *lp = NULL;

	switch_mutex_lock(globals.mod_lock);
	for (lp = globals.list; lp; lp = lp->next) {
		if (!strcasecmp(name, lp->name)) {
			break;
		}
	}
	switch_mutex_unlock(globals.mod_lock);

	return lp;
}


static struct dist_node *find_node(struct dist_list *list, const char *name)
{
	struct dist_node *np;

	switch_mutex_lock(globals.mod_lock);
	for (np = list->nodes; np; np = np->next) {
		if (!strcasecmp(name, np->name)) {
			break;
		}
	}
	switch_mutex_unlock(globals.mod_lock);

	return np;
}

#define MAX 50

SWITCH_STANDARD_API(distributor_ctl_function)
{
	int argc = 0;
	char *argv[MAX] = { 0 };
	const char *err = "-error";
	char *dup = NULL;

	switch_mutex_lock(globals.mod_lock);

	if (zstr(cmd)) {
		goto err;
	}

	dup = strdup(cmd);
	argc = switch_split(dup, ' ', argv);

	if (argc > 0) {
		if (!strcasecmp(argv[0], "reload")) {
			if (load_config(SWITCH_TRUE) == SWITCH_STATUS_SUCCESS) {
				stream->write_function(stream, "+ok reloaded.\n");
				err = NULL;
			}
		} else if (!strcasecmp(argv[0], "dump")) {
			if (argc > 1) {
				const char *listname = argv[1];
				struct dist_list *list = find_list(listname);

				if (!list) {
					err = "cannot find list";
				} else {
					struct dist_node *np;
					stream->write_function(stream, "list: name=%s\n", list->name);

					for (np = list->nodes; np; np = np->next) {
						stream->write_function(stream, "node: name=%s weight=%d\n", np->name, np->wval);
					}

					err = NULL;
				}

			} else {
				err = "missing list name";
			}
		} else if (!strcasecmp(argv[0], "modify")) {
			if (argc > 1) {
				const char *listname = argv[1];
				struct dist_list *list = find_list(listname);

				if (!list) {
					err = "cannot find list";
				} else {
					struct dist_node *np;
					int i = 2;
					char *e;

					for(i = 2; i < argc; i++) {
						if ((e = strchr(argv[i], '='))) {
							*e++ = '\0';
							if ((np = find_node(list, argv[i]))) {
								int tmp = -1;
								
								if (e) {
									tmp = atoi(e);
								}

								if (tmp > 0) {
									np->wval = tmp;
								} else {
									stream->write_function(stream, "error: name=%s, specified weight invalid\n", np->name);
								}
							} else {
								stream->write_function(stream, "error: node %s not found\n", argv[i]);
							}
						}
					}

					calc_weight(list);
					reset_list(list);

					stream->write_function(stream, "list: name=%s\n", list->name);

					for (np = list->nodes; np; np = np->next) {
						stream->write_function(stream, "node: name=%s weight=%d\n", np->name, np->wval);
					}

					err = NULL;
				}

			} else {
				err = "missing list name";
			}
		}
	}
	
 err:

	if (err) {
		stream->write_function(stream, "%s\n", err);
	}

	switch_safe_free(dup);

	switch_mutex_unlock(globals.mod_lock);

	return SWITCH_STATUS_SUCCESS;
}

/* Macro expands to: switch_status_t mod_distributor_load(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool) */
SWITCH_MODULE_LOAD_FUNCTION(mod_distributor_load)
{
	switch_api_interface_t *api_interface;
	switch_application_interface_t *app_interface;

	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	memset(&globals, 0, sizeof(globals));
	switch_mutex_init(&globals.mod_lock, SWITCH_MUTEX_NESTED, pool);
	globals.pool = pool;

	load_config(SWITCH_FALSE);


	SWITCH_ADD_API(api_interface, "distributor", "Distributor API", distributor_function, "<list name>[ <exception1> <exceptionN>]");
	SWITCH_ADD_API(api_interface, "distributor_ctl", "Distributor API", distributor_ctl_function, "[reload]");
	SWITCH_ADD_APP(app_interface, "distributor", "Distributor APP", "Distributor APP", distributor_exec, "<list name>[ <exception1> <exceptionN>]",
				   SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC);


	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

/*
  Called when the system shuts down
  Macro expands to: switch_status_t mod_distributor_shutdown() */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_distributor_shutdown)
{
	switch_mutex_lock(globals.mod_lock);
	destroy_list(globals.list);
	switch_mutex_unlock(globals.mod_lock);
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet
 */
