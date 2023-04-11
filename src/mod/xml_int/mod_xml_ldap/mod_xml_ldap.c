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
 * Bret McDanel <trixter AT 0xdecafbad.com>
 * Justin Cassidy <xachenant@hotmail.com>
 * John Skopis <john+fs@skopis.com>
 *
 * mod_xml_ldap.c -- LDAP XML Gateway
 *
 */
#include <switch.h>
#include <lber.h>
#include <ldap.h>

#define PCACHE_TTL 300
#define NCACHE_TTL 900

typedef struct xml_ldap_attribute xml_ldap_attribute_t;


typedef enum {
	XML_LDAP_CONFIG = 0,
	XML_LDAP_DIRECTORY,
	XML_LDAP_DIALPLAN,
	XML_LDAP_PHRASE
} xml_ldap_query_type_t;


typedef struct xml_binding {
	char *bindings;
        xml_ldap_query_type_t bt;
        char *url;
	char *basedn;
	char *binddn;
	char *bindpass;
	char *filter;
        xml_ldap_attribute_t *attr_list;
} xml_binding_t;

typedef enum exten_types {
        LDAP_EXTEN_ID = 0,
        LDAP_EXTEN_CIDR,
        LDAP_EXTEN_NUMBER_ALIAS,
        LDAP_EXTEN_DIAL_STRING,
        LDAP_EXTEN_PASSWORD,
        LDAP_EXTEN_REV_AUTH_USER,
        LDAP_EXTEN_REV_AUTH_PASS,
        LDAP_EXTEN_A1_HASH,
        LDAP_EXTEN_VM_PASSWORD,
        LDAP_EXTEN_VM_ENABLED,
        LDAP_EXTEN_VM_MAILFROM,
        LDAP_EXTEN_VM_MAILTO,
        LDAP_EXTEN_VM_NOTIFY_MAILTO,
        LDAP_EXTEN_VM_ATTACH_FILE,
        LDAP_EXTEN_VM_MESSAGE_EXT,
        LDAP_EXTEN_VM_EMAIL_ALL_MSGS,
        LDAP_EXTEN_VM_KEEP_LOCAL_AFTER_MAIL,
        LDAP_EXTEN_VM_NOTIFY_EMAIL_ALL_MSGS,
        LDAP_EXTEN_VM_SKIP_INSTRUCTIONS,
        LDAP_EXTEN_VM_CC,
        LDAP_EXTEN_VM_DISK_QUOTA,
        LDAP_EXTEN_ACCOUNTCODE,
        LDAP_EXTEN_USER_CONTEXT,
        LDAP_EXTEN_VM_MAILBOX,
        LDAP_EXTEN_CALLGROUP,
        LDAP_EXTEN_TOLL_ALLOW,
        LDAP_EXTEN_EFF_CLIDNUM,
        LDAP_EXTEN_EFF_CLIDNAME,
        LDAP_EXTEN_OUT_CLIDNUM,
        LDAP_EXTEN_OUT_CLIDNAME,
        LDAP_EXTEN_DOMAIN_NAME
/* not used now
        LDAP_EXTEN_AREACODE,
        LDAP_EXTEN_CID_EXTNAME,
        LDAP_EXTEN_CID_EXTNUM,
        LDAP_EXTEN_INTNAME,
        LDAP_EXTEN_INTNUM,
        LDAP_EXTEN_RECORD_CALLS,
        LDAP_EXTEN_ACTIVE,
        LDAP_EXTEN_CFWD_REWRITECID,
        LDAP_EXTEN_CFWD_ACTIVE,
        LDAP_EXTEN_CFWD_DEST,
        LDAP_EXTEN_CFWD_BUSYACTIVE,
        LDAP_EXTEN_CFWD_BUSYDEST,
        LDAP_EXTEN_NOANSWERACTIVE,
        LDAP_EXTEN_NOANSWERDEST,
        LDAP_EXTEN_NOANSWERSECONDS,
        LDAP_EXTEN_PROGRESSAUDIO,
        LDAP_EXTEN_ALLOW_OUTBOUND,
        LDAP_EXTEN_ALLOW_XFER,
        LDAP_EXTEN_HOTLINE_ACTIVE,
        LDAP_EXTEN_HOTLINE_DEST,
        LDAP_EXTEN_CLASSOFSERVICE
*/
} exten_type_t;

struct xml_ldap_attribute {
        exten_type_t type;
        uint64_t len;
        char *val;
        xml_ldap_attribute_t *next;
};

static struct {
    switch_memory_pool_t *pool;
} globals;

SWITCH_MODULE_LOAD_FUNCTION(mod_xml_ldap_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_xml_ldap_shutdown);
SWITCH_MODULE_DEFINITION(mod_xml_ldap, mod_xml_ldap_load, mod_xml_ldap_shutdown, NULL);


static switch_xml_t xml_ldap_search(const char *section, const char *tag_name, const char *key_name, const char *key_value, switch_event_t *params,
                                                                       void *user_data);

static switch_status_t trydir(switch_xml_t *, int *, LDAP *, char *, char *, xml_binding_t *, switch_event_t *params);
static switch_status_t do_config(void);
static switch_status_t trysearch(switch_xml_t *pxml, int *xoff, LDAP * ld, char *basedn, char *filter);
void rec(switch_xml_t *, int *, LDAP * ld, char *);

#define XML_LDAP_SYNTAX ""

SWITCH_STANDARD_API(xml_ldap_function)
{
        return SWITCH_STATUS_FALSE;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_xml_ldap_load)
{
        switch_api_interface_t *xml_ldap_api_interface;

        /* connect my internal structure to the blank pointer passed to me */
        *module_interface = switch_loadable_module_create_module_interface(pool, modname);

        memset(&globals, 0, sizeof(globals));
        globals.pool = pool;

        SWITCH_ADD_API(xml_ldap_api_interface, "xml_ldap", "XML LDAP", xml_ldap_function, XML_LDAP_SYNTAX);
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "XML LDAP module loading...\n");

        if (do_config() != SWITCH_STATUS_SUCCESS) {
               return SWITCH_STATUS_FALSE;
	}

        /* indicate that the module should continue to be loaded */
        return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_xml_ldap_shutdown)
{
        return SWITCH_STATUS_SUCCESS;
}

static switch_status_t do_config(void)
{
        char *cf = "xml_ldap.conf";
        switch_xml_t cfg, xml, bindings_tag, binding_tag, param, tran;
        xml_binding_t *binding = NULL;
        xml_ldap_attribute_t *attr_list = NULL;
        int x = 0;

        if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "open of %s failed\n", cf);
                return SWITCH_STATUS_TERM;
        }

        if (!(bindings_tag = switch_xml_child(cfg, "bindings"))) {
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Missing <bindings> tag!\n");
                goto done;
        }
        for (binding_tag = switch_xml_child(bindings_tag, "binding"); binding_tag; binding_tag = binding_tag->next) {
                char *bname = (char *) switch_xml_attr_soft(binding_tag, "name");

				if (!(binding = switch_core_alloc(globals.pool, sizeof(*binding)))) {
					goto done;
				}

				memset(binding, 0, sizeof(*binding));
                binding->attr_list = attr_list;

                for (param = switch_xml_child(binding_tag, "param"); param; param = param->next) {

                       char *var = (char *) switch_xml_attr_soft(param, "name");
                       char *val = (char *) switch_xml_attr_soft(param, "value");

                       if (!strcasecmp(var, "filter")) {
                               binding->bindings = (char *) switch_xml_attr_soft(param, "bindings");

                               if (!strncmp(binding->bindings, "configuration", strlen(binding->bindings))) {
                                       switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "setting type XML_LDAP_CONFIG\n");
                                       binding->bt = XML_LDAP_CONFIG;
                               } else if (!strncmp(binding->bindings, "directory", strlen(binding->bindings))) {
                                       binding->bt = XML_LDAP_DIRECTORY;
                               } else if (!strncmp(binding->bindings, "dialplan", strlen(binding->bindings))) {
                                       binding->bt = XML_LDAP_DIALPLAN;
                               } else if (!strncmp(binding->bindings, "phrases", strlen(binding->bindings))) {
                                       binding->bt = XML_LDAP_PHRASE;
                               }

                               if (val) {
                                       binding->filter = switch_core_strdup(globals.pool, val);
                                       printf("binding filter %s to %s\n", binding->filter, binding->bindings);
                               }
                       } else if (!strncasecmp(var, "basedn", strlen(val))) {
                               binding->basedn = switch_core_strdup(globals.pool, val);
                       } else if (!strncasecmp(var, "binddn", strlen(val))) {
                               binding->binddn = switch_core_strdup(globals.pool, val);
                       } else if (!strncasecmp(var, "bindpass", strlen(val))) {
                               binding->bindpass = switch_core_strdup(globals.pool, val);
                       } else if (!strncasecmp(var, "url", strlen(val))) {
                               binding->url = switch_core_strdup(globals.pool, val);
                       }
                }

                if (binding && binding->bt == XML_LDAP_DIRECTORY) {
                       attr_list = switch_core_alloc(globals.pool, sizeof(*attr_list));
                       attr_list = memset(attr_list, 0, sizeof(*attr_list));
                       binding->attr_list = attr_list;

                       param = switch_xml_child(binding_tag, "trans");
                       for (tran = switch_xml_child(param, "tran"); tran; tran = tran->next) {
                               char *n = (char *) switch_xml_attr_soft(tran, "name");
                               char *m = (char *) switch_xml_attr_soft(tran, "mapfrom");
                               switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, " adding map %s => %s\n", m, n);
                               /* Params */
                               if (!strncasecmp("id", n, strlen(n))) {
                                       attr_list->type = LDAP_EXTEN_ID;
                                       attr_list->len = strlen(m);
                                       attr_list->val = switch_core_strdup(globals.pool, m);
                                       attr_list->next = switch_core_alloc(globals.pool, sizeof(*attr_list));
                                       attr_list->next = memset(attr_list->next, 0, sizeof(*attr_list));
                                       attr_list = attr_list->next;
                               } else if (!strncasecmp("cidr", n, strlen(n))) {
                                       attr_list->type = LDAP_EXTEN_CIDR;
                                       attr_list->len = strlen(m);
                                       attr_list->val = switch_core_strdup(globals.pool, m);
                                       attr_list->next = switch_core_alloc(globals.pool, sizeof(*attr_list));
                                       attr_list->next = memset(attr_list->next, 0, sizeof(*attr_list));
                                       attr_list = attr_list->next;
                               } else if (!strncasecmp("number-alias", n, strlen(n))) {
                                       attr_list->type = LDAP_EXTEN_NUMBER_ALIAS;
                                       attr_list->len = strlen(m);
                                       attr_list->val = switch_core_strdup(globals.pool, m);
                                       attr_list->next = switch_core_alloc(globals.pool, sizeof(*attr_list));
                                       attr_list->next = memset(attr_list->next, 0, sizeof(*attr_list));
                                       attr_list = attr_list->next;
                               } else if (!strncasecmp("dial-string", n, strlen(n))) {
                                       attr_list->type = LDAP_EXTEN_DIAL_STRING;
                                       attr_list->len = strlen(m);
                                       attr_list->val = switch_core_strdup(globals.pool, m);
                                       attr_list->next = switch_core_alloc(globals.pool, sizeof(*attr_list));
                                       attr_list->next = memset(attr_list->next, 0, sizeof(*attr_list));
                                       attr_list = attr_list->next;
                               } else if (!strncasecmp("password", n, strlen(n))) {
                                       attr_list->type = LDAP_EXTEN_PASSWORD;
                                       attr_list->len = strlen(m);
                                       attr_list->val = switch_core_strdup(globals.pool, m);
                                       attr_list->next = switch_core_alloc(globals.pool, sizeof(*attr_list));
                                       attr_list->next = memset(attr_list->next, 0, sizeof(*attr_list));
                                       attr_list = attr_list->next;
                               } else if (!strncasecmp("reverse-auth-user", n, strlen(n))) {
                                       attr_list->type = LDAP_EXTEN_REV_AUTH_USER;
                                       attr_list->len = strlen(m);
                                       attr_list->val = switch_core_strdup(globals.pool, m);
                                       attr_list->next = switch_core_alloc(globals.pool, sizeof(*attr_list));
                                       attr_list->next = memset(attr_list->next, 0, sizeof(*attr_list));
                                       attr_list = attr_list->next;
                               } else if (!strncasecmp("reverse-auth-pass", n, strlen(n))) {
                                       attr_list->type = LDAP_EXTEN_REV_AUTH_PASS;
                                       attr_list->len = strlen(m);
                                       attr_list->val = switch_core_strdup(globals.pool, m);
                                       attr_list->next = switch_core_alloc(globals.pool, sizeof(*attr_list));
                                       attr_list->next = memset(attr_list->next, 0, sizeof(*attr_list));
                                       attr_list = attr_list->next;
                               } else if (!strncasecmp("a1-hash", n, strlen(n))) {
                                       attr_list->type = LDAP_EXTEN_A1_HASH;
                                       attr_list->len = strlen(m);
                                       attr_list->val = switch_core_strdup(globals.pool, m);
                                       attr_list->next = switch_core_alloc(globals.pool, sizeof(*attr_list));
                                       attr_list->next = memset(attr_list->next, 0, sizeof(*attr_list));
                                       attr_list = attr_list->next;
                               } else if (!strncasecmp("vm-password", n, strlen(n))) {
                                       attr_list->type = LDAP_EXTEN_VM_PASSWORD;
                                       attr_list->len = strlen(m);
                                       attr_list->val = switch_core_strdup(globals.pool, m);
                                       attr_list->next = switch_core_alloc(globals.pool, sizeof(*attr_list));
                                       attr_list->next = memset(attr_list->next, 0, sizeof(*attr_list));
                                       attr_list = attr_list->next;
                               } else if (!strncasecmp("vm-enabled", n, strlen(n))) {
                                       attr_list->type = LDAP_EXTEN_VM_ENABLED;
                                       attr_list->len = strlen(m);
                                       attr_list->val = switch_core_strdup(globals.pool, m);
                                       attr_list->next = switch_core_alloc(globals.pool, sizeof(*attr_list));
                                       attr_list->next = memset(attr_list->next, 0, sizeof(*attr_list));
                                       attr_list = attr_list->next;
                               } else if (!strncasecmp("vm-mailfrom", n, strlen(n))) {
                                       attr_list->type = LDAP_EXTEN_VM_MAILFROM;
                                       attr_list->len = strlen(m);
                                       attr_list->val = switch_core_strdup(globals.pool, m);
                                       attr_list->next = switch_core_alloc(globals.pool, sizeof(*attr_list));
                                       attr_list->next = memset(attr_list->next, 0, sizeof(*attr_list));
                                       attr_list = attr_list->next;
                               } else if (!strncasecmp("vm-mailto", n, strlen(n))) {
                                       attr_list->type = LDAP_EXTEN_VM_MAILTO;
                                       attr_list->len = strlen(m);
                                       attr_list->val = switch_core_strdup(globals.pool, m);
                                       attr_list->next = switch_core_alloc(globals.pool, sizeof(*attr_list));
                                       attr_list->next = memset(attr_list->next, 0, sizeof(*attr_list));
                                       attr_list = attr_list->next;
                               } else if (!strncasecmp("vm-notify-mailto", n, strlen(n))) {
                                       attr_list->type = LDAP_EXTEN_VM_NOTIFY_MAILTO;
                                       attr_list->len = strlen(m);
                                       attr_list->val = switch_core_strdup(globals.pool, m);
                                       attr_list->next = switch_core_alloc(globals.pool, sizeof(*attr_list));
                                       attr_list->next = memset(attr_list->next, 0, sizeof(*attr_list));
                                       attr_list = attr_list->next;
                               } else if (!strncasecmp("vm-attach-file", n, strlen(n))) {
                                       attr_list->type = LDAP_EXTEN_VM_ATTACH_FILE;
                                       attr_list->len = strlen(m);
                                       attr_list->val = switch_core_strdup(globals.pool, m);
                                       attr_list->next = switch_core_alloc(globals.pool, sizeof(*attr_list));
                                       attr_list->next = memset(attr_list->next, 0, sizeof(*attr_list));
                                       attr_list = attr_list->next;
                               } else if (!strncasecmp("vm-message-ext", n, strlen(n))) {
                                       attr_list->type = LDAP_EXTEN_VM_MESSAGE_EXT;
                                       attr_list->len = strlen(m);
                                       attr_list->val = switch_core_strdup(globals.pool, m);
                                       attr_list->next = switch_core_alloc(globals.pool, sizeof(*attr_list));
                                       attr_list->next = memset(attr_list->next, 0, sizeof(*attr_list));
                                       attr_list = attr_list->next;
                               } else if (!strncasecmp("vm-email-all-messages", n, strlen(n))) {
                                       attr_list->type = LDAP_EXTEN_VM_EMAIL_ALL_MSGS;
                                       attr_list->len = strlen(m);
                                       attr_list->val = switch_core_strdup(globals.pool, m);
                                       attr_list->next = switch_core_alloc(globals.pool, sizeof(*attr_list));
                                       attr_list->next = memset(attr_list->next, 0, sizeof(*attr_list));
                                       attr_list = attr_list->next;
                               } else if (!strncasecmp("vm-keep-local-after-mail", n, strlen(n))) {
                                       attr_list->type = LDAP_EXTEN_VM_KEEP_LOCAL_AFTER_MAIL;
                                       attr_list->len = strlen(m);
                                       attr_list->val = switch_core_strdup(globals.pool, m);
                                       attr_list->next = switch_core_alloc(globals.pool, sizeof(*attr_list));
                                       attr_list->next = memset(attr_list->next, 0, sizeof(*attr_list));
                                       attr_list = attr_list->next;
                               } else if (!strncasecmp("vm-notify-email-all-messages", n, strlen(n))) {
                                       attr_list->type = LDAP_EXTEN_VM_NOTIFY_EMAIL_ALL_MSGS;
                                       attr_list->len = strlen(m);
                                       attr_list->val = switch_core_strdup(globals.pool, m);
                                       attr_list->next = switch_core_alloc(globals.pool, sizeof(*attr_list));
                                       attr_list->next = memset(attr_list->next, 0, sizeof(*attr_list));
                                       attr_list = attr_list->next;
                               } else if (!strncasecmp("vm-skip-instructions", n, strlen(n))) {
                                       attr_list->type = LDAP_EXTEN_VM_SKIP_INSTRUCTIONS;
                                       attr_list->len = strlen(m);
                                       attr_list->val = switch_core_strdup(globals.pool, m);
                                       attr_list->next = switch_core_alloc(globals.pool, sizeof(*attr_list));
                                       attr_list->next = memset(attr_list->next, 0, sizeof(*attr_list));
                                       attr_list = attr_list->next;
                               } else if (!strncasecmp("vm-cc", n, strlen(n))) {
                                       attr_list->type = LDAP_EXTEN_VM_CC;
                                       attr_list->len = strlen(m);
                                       attr_list->val = switch_core_strdup(globals.pool, m);
                                       attr_list->next = switch_core_alloc(globals.pool, sizeof(*attr_list));
                                       attr_list->next = memset(attr_list->next, 0, sizeof(*attr_list));
                                       attr_list = attr_list->next;
                               } else if (!strncasecmp("vm-disk-quota", n, strlen(n))) {
                                       attr_list->type = LDAP_EXTEN_VM_DISK_QUOTA;
                                       attr_list->len = strlen(m);
                                       attr_list->val = switch_core_strdup(globals.pool, m);
                                       attr_list->next = switch_core_alloc(globals.pool, sizeof(*attr_list));
                                       attr_list->next = memset(attr_list->next, 0, sizeof(*attr_list));
                                       attr_list = attr_list->next;
                               /* Variables */
                               } else if (!strncasecmp("accountcode", n, strlen(n))) {
                                       attr_list->type = LDAP_EXTEN_ACCOUNTCODE;
                                       attr_list->len = strlen(m);
                                       attr_list->val = switch_core_strdup(globals.pool, m);
                                       attr_list->next = switch_core_alloc(globals.pool, sizeof(*attr_list));
                                       attr_list->next = memset(attr_list->next, 0, sizeof(*attr_list));
                                       attr_list = attr_list->next;
                               } else if (!strncasecmp("user_context", n, strlen(n))) {
                                       attr_list->type = LDAP_EXTEN_USER_CONTEXT;
                                       attr_list->len = strlen(m);
                                       attr_list->val = switch_core_strdup(globals.pool, m);
                                       attr_list->next = switch_core_alloc(globals.pool, sizeof(*attr_list));
                                       attr_list->next = memset(attr_list->next, 0, sizeof(*attr_list));
                                       attr_list = attr_list->next;
                               } else if (!strncasecmp("vm_mailbox", n, strlen(n))) {
                                       attr_list->type = LDAP_EXTEN_VM_MAILBOX;
                                       attr_list->len = strlen(m);
                                       attr_list->val = switch_core_strdup(globals.pool, m);
                                       attr_list->next = switch_core_alloc(globals.pool, sizeof(*attr_list));
                                       attr_list->next = memset(attr_list->next, 0, sizeof(*attr_list));
                                       attr_list = attr_list->next;
                               } else if (!strncasecmp("callgroup", n, strlen(n))) {
                                       attr_list->type = LDAP_EXTEN_CALLGROUP;
                                       attr_list->len = strlen(m);
                                       attr_list->val = switch_core_strdup(globals.pool, m);
                                       attr_list->next = switch_core_alloc(globals.pool, sizeof(*attr_list));
                                       attr_list->next = memset(attr_list->next, 0, sizeof(*attr_list));
                                       attr_list = attr_list->next;
                               } else if (!strncasecmp("toll_allow", n, strlen(n))) {
                                       attr_list->type = LDAP_EXTEN_TOLL_ALLOW;
                                       attr_list->len = strlen(m);
                                       attr_list->val = switch_core_strdup(globals.pool, m);
                                       attr_list->next = switch_core_alloc(globals.pool, sizeof(*attr_list));
                                       attr_list->next = memset(attr_list->next, 0, sizeof(*attr_list));
                                       attr_list = attr_list->next;
                               } else if (!strncasecmp("effective_caller_id_number", n, strlen(n))) {
                                       attr_list->type = LDAP_EXTEN_EFF_CLIDNUM;
                                       attr_list->len = strlen(m);
                                       attr_list->val = switch_core_strdup(globals.pool, m);
                                       attr_list->next = switch_core_alloc(globals.pool, sizeof(*attr_list));
                                       attr_list->next = memset(attr_list->next, 0, sizeof(*attr_list));
                                       attr_list = attr_list->next;
                               } else if (!strncasecmp("effective_caller_id_name", n, strlen(n))) {
                                       attr_list->type = LDAP_EXTEN_EFF_CLIDNAME;
                                       attr_list->len = strlen(m);
                                       attr_list->val = switch_core_strdup(globals.pool, m);
                                       attr_list->next = switch_core_alloc(globals.pool, sizeof(*attr_list));
                                       attr_list->next = memset(attr_list->next, 0, sizeof(*attr_list));
                                       attr_list = attr_list->next;
                               } else if (!strncasecmp("outbound_caller_id_number", n, strlen(n))) {
                                       attr_list->type = LDAP_EXTEN_OUT_CLIDNUM;
                                       attr_list->len = strlen(m);
                                       attr_list->val = switch_core_strdup(globals.pool, m);
                                       attr_list->next = switch_core_alloc(globals.pool, sizeof(*attr_list));
                                       attr_list->next = memset(attr_list->next, 0, sizeof(*attr_list));
                                       attr_list = attr_list->next;
                               } else if (!strncasecmp("outbound_caller_id_name", n, strlen(n))) {
                                       attr_list->type = LDAP_EXTEN_OUT_CLIDNAME;
                                       attr_list->len = strlen(m);
                                       attr_list->val = switch_core_strdup(globals.pool, m);
                                       attr_list->next = switch_core_alloc(globals.pool, sizeof(*attr_list));
                                       attr_list->next = memset(attr_list->next, 0, sizeof(*attr_list));
                                       attr_list = attr_list->next;
                               }
                       }
                       attr_list->next = NULL;
                }
                if (!binding->basedn || !binding->filter || !binding->url) {
                       switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "You must define \"basedn\", and \"filter\" in mod_xml_ldap.conf.xml\n");
                       continue;
                }

                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Binding [%s] XML Fetch Function [%s] (%s) [%s]\n",
                                             zstr(bname) ? "N/A" : bname, binding->basedn, binding->filter, binding->bindings ? binding->bindings : "all");

                switch_xml_bind_search_function(xml_ldap_search, switch_xml_parse_section_string(bname), binding);

                x++;
                binding = NULL;
        }

  done:
        switch_xml_free(xml);

	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t trydir(switch_xml_t *pxml, int *xoff, LDAP * ld, char *dir_domain, char *dir_exten, xml_binding_t *binding, switch_event_t *param_event)
{
        switch_status_t ret = SWITCH_STATUS_FALSE;
        int off = *xoff;
        char *key = NULL;
        char *basedn = NULL, *filter = NULL;
        struct berval **val = NULL;
        BerElement *ber = NULL;
        switch_xml_t xml = *pxml, params = NULL, param = NULL, vars = NULL, cur = NULL;
        LDAPMessage *msg, *entry;
        xml_ldap_attribute_t *attr = NULL;
        static char *fsattr[] =
                { "id", "cidr", "number-alias", "dial-string", "password", "reverse-auth-user",
                   "reverse-auth-pass", "a1-hash", "vm-password", "vm-enabled", "vm-mailfrom",
                   "vm-mailto", "vm-notify-mailto", "vm-attach-file", "vm-message-ext",
                   "vm-email-all-messages", "vm-keep-local-after-mail", "vm-notify-email-all-messages",
                   "vm-skip-instructions", "vm-cc", "vm-disk-quota", "accountcode", "user_context",
                   "vm_mailbox", "callgroup", "effective_caller_id_number", "effective_caller_id_name",
                   "outbound_caller_id_number", "outbound_caller_id_name", "toll_allow", NULL };

        basedn = switch_mprintf(binding->basedn, dir_domain);
        filter = switch_mprintf(binding->filter, dir_exten);

		if (param_event) {
			char *expanded = switch_event_expand_headers(param_event, filter);

			if (expanded != filter) {
				free(filter);
				filter = expanded;
			}
		}

        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "searching in basedn %s with filter %s\n", basedn, filter);

        if ((ldap_search_s(ld, basedn, LDAP_SCOPE_SUB, filter, NULL, 0, &msg) != LDAP_SUCCESS))
                goto cleanup;

        if (ldap_count_entries(ld, msg) > 0) {
                ret = SWITCH_STATUS_SUCCESS;
                xml = switch_xml_add_child_d(xml, "section", off++);
                switch_xml_set_attr_d(xml, "name", "directory");

                xml = switch_xml_add_child_d(xml, "domain", off++);
                switch_xml_set_attr_d(xml, "name", dir_domain);

                params = switch_xml_add_child_d(xml, "params", off++);
                param = switch_xml_add_child_d(params, "param", off++);
                switch_xml_set_attr_d(param, "name", "dial-string");
                switch_xml_set_attr_d(param, "value", "{^^:sip_invite_domain=${dialed_domain}:presence_id=${dialed_user}@${dialed_domain}}${sofia_contact(*/${dialed_user}@${dialed_domain})}");

                xml = switch_xml_add_child_d(xml, "user", off++);

                params = switch_xml_add_child_d(xml, "params", off++);
                vars = switch_xml_add_child_d(xml, "variables", off++);

                for (entry = ldap_first_entry(ld, msg); entry != NULL; entry = ldap_next_entry(ld, entry)) {
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "entry searched by filter %s\n", filter);

                        for (key = ldap_first_attribute(ld, entry, &ber); key != NULL; key = ldap_next_attribute(ld, entry, ber)) {

                                for (attr = binding->attr_list; attr; attr = attr->next) {
                                        if (strlen(key) == attr->len) {
                                                if (!strncasecmp(attr->val, key, strlen(key))) {
                                                        val = ldap_get_values_len(ld, entry, key);
                                                        if (val != NULL) {
                                                                if (ldap_count_values_len(val) > 0 && val[0] != NULL && val[0]->bv_val != NULL) {
									switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, " attribute %s => %s\n", fsattr[attr->type], val[0]->bv_val);
                                                                        if (attr->type < 2) {
                                                                                switch_xml_set_attr_d(xml, fsattr[attr->type], val[0]->bv_val);
                                                                        } else if (attr->type < 10) {
                                                                                cur = switch_xml_add_child_d(params, "param", 0);
                                                                                switch_xml_set_attr_d(cur, "name", fsattr[attr->type]);
                                                                                switch_xml_set_attr_d(cur, "value", val[0]->bv_val);
                                                                        } else {
                                                                                cur = switch_xml_add_child_d(vars, "variable", 0);
                                                                                switch_xml_set_attr_d(cur, "name", fsattr[attr->type]);
                                                                                switch_xml_set_attr_d(cur, "value", val[0]->bv_val);
                                                                        }
                                                                }
                                                                ldap_value_free_len(val);
                                                        }
                                                        continue;
                                                }
                                        }
                                }
                                ldap_memfree(key);
			}
                        ber_free(ber, 0);
                }

                ldap_msgfree(entry);
                ldap_msgfree(msg);
        } else {
                ret = SWITCH_STATUS_FALSE;
        }

   cleanup:
        switch_safe_free(filter);
        switch_safe_free(basedn);

        return ret;
}

static switch_status_t trysearch(switch_xml_t *pxml, int *xoff, LDAP * ld, char *basedn, char *filter)
{
        switch_status_t ret = SWITCH_STATUS_FALSE;
        int off = *xoff;
        char *key = NULL;
        char *dn = NULL;
        char **val = NULL;
        BerElement *ber = NULL;
        switch_xml_t xml = *pxml;
        LDAPMessage *msg, *entry;

        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "trying search in base %s with filter %s\n", basedn, filter);

        if ((ldap_search_s(ld, basedn, LDAP_SCOPE_ONE, filter, NULL, 0, &msg) != LDAP_SUCCESS))
                goto cleanup;

        if (ldap_count_entries(ld, msg) > 0) {
                ret = SWITCH_STATUS_SUCCESS;
                for (entry = ldap_first_entry(ld, msg); entry != NULL; entry = ldap_next_entry(ld, entry)) {

                        val = ldap_get_values(ld, entry, "fstag");
                        xml = switch_xml_add_child_d(xml, val[0], off);
                        ldap_value_free(val);

                        for (key = ldap_first_attribute(ld, entry, &ber); key != NULL; key = ldap_next_attribute(ld, entry, ber)) {

                                if (!strncasecmp(key, "fstag", strlen(key)) || !strncasecmp(key, "objectclass", strlen(key))) {
                                        ldap_memfree(key);
                                        continue;
				}

                                val = ldap_get_values(ld, entry, key);
                                switch_xml_set_attr_d(xml, key, val[0]);

                                ldap_memfree(key);
                                ldap_value_free(val);
			}
                        ber_free(ber, 0);

                        dn = ldap_get_dn(ld, entry);
                        rec(&xml, &off, ld, dn);

                        *xoff = 1;
		}

                ldap_msgfree(entry);
                ldap_msgfree(msg);
        }

   cleanup:
        switch_safe_free(basedn);
        switch_safe_free(filter);
        switch_safe_free(key);

        return ret;
}

void rec(switch_xml_t *pxml, int *xoff, LDAP * ld, char *dn)
{
        int off = *xoff;
        char *key;
        char **val;

        switch_xml_t xml = *pxml, new;

        LDAPMessage *msg, *entry;
        BerElement *ber;

        ldap_search_s(ld, dn, LDAP_SCOPE_ONE, NULL, NULL, 0, &msg);
        switch_safe_free(dn);

        if (ldap_count_entries(ld, msg) > 0) {

                for (entry = ldap_first_entry(ld, msg); entry != NULL; entry = ldap_next_entry(ld, entry)) {

                        val = ldap_get_values(ld, entry, "fstag");
                        new = switch_xml_add_child_d(xml, val[0], off);
                        ldap_value_free(val);

                        for (key = ldap_first_attribute(ld, entry, &ber); key != NULL; key = ldap_next_attribute(ld, entry, ber)) {

                                if (!strncasecmp("fstag", key, 5) || !strncasecmp("objectclass", key, 10)) {
                                        ldap_memfree(key);
                                        continue;
                                }

                                val = ldap_get_values(ld, entry, key);
                                switch_xml_set_attr_d(new, key, val[0]);
                                ldap_memfree(key);
                                ldap_value_free(val);
                        }
                        ber_free(ber, 0);
                        rec(&new, xoff, ld, ldap_get_dn(ld, entry));
                }
                ldap_msgfree(entry);

	}

        ldap_msgfree(msg);
}

static switch_xml_t xml_ldap_search(const char *section, const char *tag_name, const char *key_name, const char *key_value, switch_event_t *params,
                                                                        void *user_data)
{

        xml_binding_t *binding = (xml_binding_t *) user_data;
        switch_event_header_t *hi;
        switch_status_t ret = SWITCH_STATUS_FALSE;

        int desired_version = LDAP_VERSION3;
        int auth_method = LDAP_AUTH_SIMPLE;

        char *basedn = NULL, *filter = NULL;
        char *dir_domain = NULL, *dir_exten = NULL;

        LDAP *ld;
        switch_xml_t xml = NULL;

        int xoff = 0;

        char *buf;
        buf = malloc(4096);

        xml = switch_xml_new("document");
        switch_xml_set_attr_d(xml, "type", "freeswitch/xml");

        if (params) {
                if ((hi = params->headers)) {
                        for (; hi; hi = hi->next) {
                                switch (binding->bt) {
                                case XML_LDAP_CONFIG:
                                        break;

                                case XML_LDAP_DIRECTORY:
                                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "from cb got %s=%s\n", hi->name, hi->value);
                                        if (!strncmp(hi->name, "user", strlen(hi->name))) {
                                                switch_safe_free(dir_exten);
                                                dir_exten = strdup(hi->value);
                                        } else if (!strncmp(hi->name, "domain", strlen(hi->name))) {
                                                switch_safe_free(dir_domain);
                                                dir_domain = strdup(hi->value);
                                        }
                                        break;

                                case XML_LDAP_DIALPLAN:
                                case XML_LDAP_PHRASE:
                                        break;
				}
			}
		}
	}
        if ((ldap_initialize(&ld, binding->url)) != LDAP_SUCCESS)
                goto cleanup;
        if ((ldap_set_option(ld, LDAP_OPT_PROTOCOL_VERSION, &desired_version)) != LDAP_SUCCESS)
                goto cleanup;
        if ((ldap_bind_s(ld, binding->binddn, binding->bindpass, auth_method)) != LDAP_SUCCESS)
                goto cleanup;

        switch (binding->bt) {
        case XML_LDAP_CONFIG:
                xml = switch_xml_add_child_d(xml, "section", xoff++);
                switch_xml_set_attr_d(xml, "name", "configuration");
                filter = switch_mprintf(binding->filter, key_name, key_value);
                basedn = switch_mprintf(binding->basedn, tag_name);
                ret = trysearch(&xml, &xoff, ld, basedn, filter);
                break;

        case XML_LDAP_DIRECTORY:
			ret = trydir(&xml, &xoff, ld, dir_domain, dir_exten, binding, params);
                break;

        case XML_LDAP_DIALPLAN:
                break;

        case XML_LDAP_PHRASE:
                break;
        }


   cleanup:
        ldap_unbind_s(ld);

        switch_xml_toxml_buf(xml, buf, 0, 0, 1);
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"XML providing:\n%s\n", buf);
        switch_safe_free(buf);
        switch_safe_free(dir_exten);
        switch_safe_free(dir_domain);

        if (ret != SWITCH_STATUS_SUCCESS) {
                switch_xml_free(xml);
                return NULL;
        }

        return xml;
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
