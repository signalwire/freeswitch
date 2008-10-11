#include <switch.h>
#include <stdlib.h>
#include <string.h>
#include <lber.h>
#include <ldap.h>

#define PCACHE_TTL 300
#define NCACHE_TTL 900

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
} xml_binding_t;


SWITCH_MODULE_LOAD_FUNCTION(mod_xml_ldap_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_xml_ldap_shutdown);
SWITCH_MODULE_DEFINITION(mod_xml_ldap, mod_xml_ldap_load, mod_xml_ldap_shutdown, NULL);


static switch_xml_t xml_ldap_search(const char *section, const char *tag_name, const char *key_name, const char *key_value, switch_event_t *params,
                                    void *user_data);

static switch_status_t do_config(void);
static switch_status_t trysearch( switch_xml_t *pxml, int *xoff, LDAP *ld, char *basedn, char *filter);
void rec( switch_xml_t *, int*, LDAP *ld, char *);

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

static switch_status_t do_config(void) {
	char *cf = "xml_ldap.conf";
	switch_xml_t cfg, xml, bindings_tag, binding_tag, param;
	xml_binding_t *binding = NULL;
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

		if (!(binding = malloc(sizeof(*binding)))) {
			goto done;
		}
		memset(binding, 0, sizeof(*binding));

		for (param = switch_xml_child(binding_tag, "param"); param; param = param->next) {

			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");

			if (!strcasecmp(var, "filter")) {
				binding->bindings = (char *) switch_xml_attr_soft(param, "bindings");

				if (!strncmp(binding->bindings, "configuration",strlen(binding->bindings))) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "setting type XML_LDAP_CONFIG\n");
					binding->bt = XML_LDAP_CONFIG;
				} else if (!strncmp(binding->bindings, "directory",strlen(binding->bindings))) {
					binding->bt = XML_LDAP_DIRECTORY;
				} else if (!strncmp(binding->bindings, "dialplain",strlen(binding->bindings))) {
					binding->bt = XML_LDAP_DIALPLAN;
				} else if (!strncmp(binding->bindings, "phrases",strlen(binding->bindings))) {
					binding->bt = XML_LDAP_PHRASE;
				}

				if (val) {
					binding->filter = strdup(val);
					printf("binding filter %s to %s\n", binding->filter, binding->bindings);
				}
			} else if (!strncasecmp(var, "basedn", strlen(val))) {
				binding->basedn = strdup(val);
			} else if (!strncasecmp(var, "binddn", strlen(val))) {
				binding->binddn = strdup(val);
			} else if (!strncasecmp(var, "bindpass", strlen(val))) {
				binding->bindpass = strdup(val);
			} else if (!strncasecmp(var, "url", strlen(val))) {
				binding->url = strdup(val);
			}

		}

		if (!binding->basedn || !binding->filter || !binding->url) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
							  "You must define \"basedn\", and \"filter\" in mod_xml_ldap.conf.xml\n");
			continue;
		}


		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Binding [%s] XML Fetch Function [%s] (%s) [%s]\n",
						  switch_strlen_zero(bname) ? "N/A" : bname, binding->basedn, binding->filter, binding->bindings ? binding->bindings : "all");

		switch_xml_bind_search_function(xml_ldap_search, switch_xml_parse_section_string(bname), binding);

		x++;
		binding = NULL;
	}

  done:
	switch_xml_free(xml);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t trysearch ( switch_xml_t *pxml, int *xoff, LDAP *ld, char *basedn, char *filter) {
	switch_status_t ret;
	int off = *xoff;
	char *key = NULL;
	char *dn = NULL;
	char **val = NULL;
	BerElement *ber = NULL;
	switch_xml_t xml = *pxml;
	LDAPMessage *msg, *entry;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "trying search in base %s with filter %s\n", basedn, filter);

	ldap_search_s(ld, basedn,  LDAP_SCOPE_ONE, filter, NULL, 0, &msg);

	if ( ldap_count_entries(ld, msg) > 0 ) {
		ret = SWITCH_STATUS_SUCCESS;
		for ( 
			entry = ldap_first_entry(ld, msg); 
			entry != NULL; 
			entry = ldap_next_entry(ld, entry) ) {

			val = ldap_get_values(ld,entry,"fstag" );
			xml = switch_xml_add_child_d(xml, val[0], off);
			ldap_value_free(val);

			for (
				key = ldap_first_attribute(ld, entry, &ber);
				key != NULL;
				key = ldap_next_attribute(ld, entry, ber) ) {

				if ( !strncasecmp(key,"fstag",strlen(key)) || !strncasecmp(key,"objectclass",strlen(key)) ) {
					ldap_memfree(key);
					continue;
				}

				val = ldap_get_values(ld,entry,key);
				switch_xml_set_attr_d(xml, key, val[0]);

				ldap_memfree(key);
				ldap_value_free(val);

			}
			ber_free(ber,0);

			dn = ldap_get_dn(ld,entry);
		    rec(&xml,&off,ld,dn);

			*xoff=1;
		}

		ldap_msgfree(entry);
		ldap_msgfree(msg);
	} else {
		ret = SWITCH_STATUS_FALSE;
	}

	switch_safe_free(filter);
	switch_safe_free(key);

	return ret;
	
}




void rec( switch_xml_t *pxml, int *xoff, LDAP *ld, char *dn) {
	int off = *xoff;
	char *key;
	char **val;

	switch_xml_t xml = *pxml, new;

	LDAPMessage *msg, *entry;
	BerElement *ber;

	ldap_search_s(ld, dn, LDAP_SCOPE_ONE, NULL, NULL, 0, &msg);
	switch_safe_free(dn);

	if ( ldap_count_entries(ld, msg) > 0 ) {

		for ( 
			entry = ldap_first_entry(ld, msg);
			entry != NULL;
			entry = ldap_next_entry(ld, entry) ) {

			val = ldap_get_values(ld,entry,"fstag" );
			new  = switch_xml_add_child_d(xml, val[0], off);
			ldap_value_free(val);

			for (
				key = ldap_first_attribute(ld, entry,&ber);
				key != NULL;
				key = ldap_next_attribute(ld,entry,ber) ) {

				if ( !strncasecmp("fstag",key,5) || !strncasecmp("objectclass",key,10) ) {
					ldap_memfree(key);
					continue;
				}

                val = ldap_get_values(ld,entry,key);
                switch_xml_set_attr_d(new, key, val[0]);
				ldap_memfree(key);
				ldap_value_free(val);
			}
			ber_free(ber,0);
			rec( &new, xoff , ld, ldap_get_dn(ld,entry) );
		}

		ldap_msgfree(entry);

	}
	ldap_msgfree(msg);
}


static switch_xml_t xml_ldap_search(const char *section, const char *tag_name, const char *key_name, const char *key_value, switch_event_t *params,
									void *user_data) {

	xml_binding_t *binding = (xml_binding_t *) user_data;
	switch_event_header_t *hi;

	int desired_version = LDAP_VERSION3;
	int auth_method = LDAP_AUTH_SIMPLE;

	char *basedn = NULL, *filter = NULL;
	char *dir_domain = NULL, *dir_exten = NULL;
	
	char *buf;        
	buf = malloc(4096);

	LDAP *ld;
	switch_xml_t xml = NULL;

	int xoff = 0;

	if (params) {
		if ((hi = params->headers)) {
			for (; hi; hi = hi->next) {
				switch (binding->bt) {
				case XML_LDAP_CONFIG:
					break;

				case XML_LDAP_DIRECTORY:
					if (!strncmp(hi->name, "user", strlen(hi->name))) {
						dir_exten = strdup(hi->value);
					} else if (!strncmp(hi->name, "domain", strlen(hi->name))) {
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
	switch (binding->bt) {
		case XML_LDAP_CONFIG:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "humm %s", binding->filter);
			filter = switch_mprintf(binding->filter,key_name,key_value);
			basedn = switch_mprintf(binding->basedn,tag_name);
			break;

		case XML_LDAP_DIRECTORY:
			if(!dir_exten) {
				filter = switch_mprintf(binding->filter,"objectclass","*","(!(objectclass=fsuser))");
				basedn = switch_mprintf(binding->basedn,dir_domain);
			} else {
				filter = switch_mprintf(binding->filter,key_name,key_value,"object_class=*");
				basedn = switch_mprintf(binding->basedn,dir_domain);
			}
			break;

		case XML_LDAP_DIALPLAN:
			break;

		case XML_LDAP_PHRASE:
			break;
	}


	ldap_initialize(&ld,binding->url);
	ldap_set_option(ld, LDAP_OPT_PROTOCOL_VERSION, &desired_version);
	ldap_bind_s(ld, binding->binddn, binding->bindpass, auth_method);

	xml = switch_xml_new("document");
	switch_xml_set_attr_d(xml, "type", "freeswitch/xml");


	
	trysearch(&xml,&xoff,ld, basedn, filter);

	ldap_unbind_s(ld);



    switch_xml_toxml_buf(xml,buf,0,0,1);
    printf("providing:\n%s\n", buf);
	switch_safe_free(buf);
	
	return xml;
}

