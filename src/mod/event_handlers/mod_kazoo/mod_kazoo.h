#include <switch.h>
#include <switch_event.h>
#include <switch_json.h>
#include <ei.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <float.h>

#define MAX_ACL 100
#define CMD_BUFLEN 1024 * 1000
#define MAX_QUEUE_LEN 25000
#define MAX_MISSED 500
#define MAX_PID_CHARS 255

extern const char kz_default_config[];
extern const int kz_default_config_size;

#include "kazoo_ei.h"
#include "kazoo_message.h"

typedef enum {
	LFLAG_RUNNING = (1 << 0)
} event_flag_t;


/* kazoo_commands.c */
void add_kz_commands(switch_loadable_module_interface_t **module_interface, switch_api_interface_t *api_interface);

/* kazoo_dptools.c */
void add_kz_dptools(switch_loadable_module_interface_t **module_interface, switch_application_interface_t *app_interface);

/* kazoo_api.c */
void add_cli_api(switch_loadable_module_interface_t **module_interface, switch_api_interface_t *api_interface);
void remove_cli_api();

/* kazoo_utils.c */
SWITCH_DECLARE(switch_status_t) kz_switch_core_merge_variables(switch_event_t *event);
SWITCH_DECLARE(switch_status_t) kz_switch_core_base_headers_for_expand(switch_event_t **event);
void kz_check_set_profile_var(switch_channel_t *channel, char* var, char *val);
char* kz_switch_event_get_first_of(switch_event_t *event, const char *list[]);
SWITCH_DECLARE(switch_status_t) kz_switch_event_add_variable_name_printf(switch_event_t *event, switch_stack_t stack, const char *val, const char *fmt, ...);
void kz_xml_process(switch_xml_t cfg);
void kz_event_decode(switch_event_t *event);
char * kz_expand_vars(char *xml_str);
char * kz_expand_vars_pool(char *xml_str, switch_memory_pool_t *pool);
SWITCH_DECLARE(char *) kz_event_expand_headers(switch_event_t *event, const char *in);
SWITCH_DECLARE(char *) kz_expand(const char *in);
SWITCH_DECLARE(char *) kz_expand_pool(switch_memory_pool_t *pool, const char *in);
switch_status_t kz_json_api(const char * command, cJSON *args, cJSON **res);

/* kazoo_endpoints.c */
void add_kz_endpoints(switch_loadable_module_interface_t **module_interface);


/* kazoo_tweaks.c */
void kz_tweaks_start();
void kz_tweaks_stop();
SWITCH_DECLARE(const char *) kz_tweak_name(kz_tweak_t tweak);
SWITCH_DECLARE(switch_status_t) kz_name_tweak(const char *name, kz_tweak_t *type);

/* kazoo_node.c */
void add_kz_node(switch_loadable_module_interface_t **module_interface);

SWITCH_MODULE_LOAD_FUNCTION(mod_kazoo_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_kazoo_shutdown);


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
