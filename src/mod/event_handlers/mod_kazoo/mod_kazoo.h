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
char *kazoo_expand_header(switch_memory_pool_t *pool, switch_event_t *event, char *val);
char* switch_event_get_first_of(switch_event_t *event, const char *list[]);
SWITCH_DECLARE(switch_status_t) switch_event_add_variable_name_printf(switch_event_t *event, switch_stack_t stack, const char *val, const char *fmt, ...);
void kz_xml_process(switch_xml_t cfg);

/* kazoo_tweaks.c */
void kz_tweaks_start();
void kz_tweaks_stop();

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
