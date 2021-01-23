#include "mod_kazoo.h"

SWITCH_DECLARE(switch_status_t) kz_switch_loadable_module_enumerate_available(const char *dir_path, switch_modulename_callback_func_t callback, void *user_data)
{
	switch_dir_t *dir = NULL;
	switch_status_t status;
	char buffer[256];
	const char *fname;
	const char *fname_ext;
	char *fname_base;
	switch_memory_pool_t *pool = NULL;
	const char *ext = ".so";

	if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_GENERR;
	};

	if ((status = switch_dir_open(&dir, dir_path, pool)) != SWITCH_STATUS_SUCCESS) {
		goto end1;
	}

	while((fname = switch_dir_next_file(dir, buffer, sizeof(buffer)))) {
		if ((fname_ext = strrchr(fname, '.'))) {
			if (!strcmp(fname_ext, ext)) {
				if (!(fname_base = switch_mprintf("%.*s", (int)(fname_ext-fname), fname))) {
					status = SWITCH_STATUS_GENERR;
					goto end;
				}
				callback(user_data, fname_base);
				switch_safe_free(fname_base)
			}
		}
	}

  end:
	switch_dir_close(dir);
  end1:
	switch_core_perform_destroy_memory_pool_now(&pool, __FILE__, __SWITCH_FUNC__, __LINE__);
	return status;
}

static int kz_nodes_module_names_array_callback(void *pArg, const char *module_name)
{
	cJSON *json = (cJSON *) pArg;
	if(!strstr(module_name, "CORE")) {
		cJSON_AddItemToArray(json, cJSON_CreateString(module_name));
	}
	return 0;
}

void kz_nodes_collect_media_role(cJSON *container)
{
	cJSON *retval = NULL;
	if(kz_json_api("sofia.status.info", NULL, &retval) == SWITCH_STATUS_SUCCESS) {
		if(retval != NULL && (!(retval->type & cJSON_NULL))) {
			cJSON_AddItemToObject(container, "Media", cJSON_Duplicate(retval, 1));
		}
	}
	if(retval) {
		cJSON_Delete(retval);
	}
}

void kz_nodes_collect_modules(cJSON *container)
{
	cJSON *modules = cJSON_CreateObject();
	cJSON *loaded = cJSON_CreateArray();
	cJSON *available = cJSON_CreateArray();
	kz_switch_loadable_module_enumerate_available(SWITCH_GLOBAL_dirs.mod_dir, kz_nodes_module_names_array_callback, available);
	switch_loadable_module_enumerate_loaded(kz_nodes_module_names_array_callback, loaded);
	cJSON_AddItemToObject(modules, "available", available);
	cJSON_AddItemToObject(modules, "loaded", loaded);
	cJSON_AddItemToObject(container, "Modules", modules);
}

void kz_nodes_collect_runtime(cJSON *container)
{
	cJSON *retval = NULL;
	if(kz_json_api("status", NULL, &retval) == SWITCH_STATUS_SUCCESS) {
		if(retval != NULL && (!(retval->type & cJSON_NULL))) {
			cJSON *val = cJSON_Duplicate(retval, 1);
			cJSON_AddItemToObject(val, "Core-UUID", cJSON_CreateString(switch_core_get_uuid()));
			cJSON_AddItemToObject(container, "Runtime-Info", val);
		}
	}
	if(retval) {
		cJSON_Delete(retval);
	}
}

void kz_nodes_collect_apps(cJSON *container)
{
	cJSON *apps = cJSON_CreateObject();
	cJSON *app = cJSON_CreateObject();
	cJSON_AddItemToObject(app, "Uptime", cJSON_CreateNumber(switch_core_uptime()));
	cJSON_AddItemToObject(apps, "freeswitch", app);
	cJSON_AddItemToObject(container, "WhApps", apps);
}

void kz_nodes_collect_roles(cJSON *container)
{
	cJSON *roles = cJSON_CreateObject();
	cJSON_AddItemToObject(container, "Roles", roles);
	kz_nodes_collect_media_role(roles);
}

cJSON * kz_node_create()
{
	cJSON *node = cJSON_CreateObject();

	kz_nodes_collect_apps(node);
	kz_nodes_collect_runtime(node);
	kz_nodes_collect_modules(node);
	kz_nodes_collect_roles(node);

	return node;
}

SWITCH_STANDARD_JSON_API(kz_node_info_json_function)
{
	cJSON * ret = kz_node_create();
	*json_reply = ret;
	return SWITCH_STATUS_SUCCESS;
}

void add_kz_node(switch_loadable_module_interface_t **module_interface)
{
	switch_json_api_interface_t *json_api_interface = NULL;
	SWITCH_ADD_JSON_API(json_api_interface, "node.info", "JSON node API", kz_node_info_json_function, "");
}
