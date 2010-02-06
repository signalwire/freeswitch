/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2010, Anthony Minessale II <anthm@freeswitch.org>
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
 * Mathieu Rene <mathieu.rene@gmail.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Mathieu Rene <mathieu.rene@gmail.com>
 *
 *
 * switch_xml_config.h - Generic configuration parser
 *
 */

#ifndef SWITCH_XML_CONFIG_H
#define SWITCH_XML_CONFIG_H

#include <switch.h>

SWITCH_BEGIN_EXTERN_C
/*! \brief Type of value to parse */
	typedef enum {
	SWITCH_CONFIG_INT,			/*< (ptr=int* default=int data=NULL) Integer */
	SWITCH_CONFIG_STRING,		/*< (ptr=[char* or char ** (for alloc)] default=char* data=switch_xml_config_string_options_t*) Zero-terminated C-string */
	SWITCH_CONFIG_BOOL,			/*< (ptr=switch_bool_t* default=switch_bool_t data=NULL) Yes and no */
	SWITCH_CONFIG_CUSTOM,		/*< (ptr=<custom function data> default=<custom function data> data=switch_xml_config_callback_t) Custom, get value through function pointer  */
	SWITCH_CONFIG_ENUM,			/*< (ptr=int* default=int data=switch_xml_config_enum_item_t*) */
	SWITCH_CONFIG_FLAG,			/*< (ptr=int32_t* default=switch_bool_t data=int (flag index) */
	SWITCH_CONFIG_FLAGARRAY,	/*< (ptr=int8_t* default=switch_bool_t data=int (flag index) */

	/* No more past that line */
	SWITCH_CONFIG_LAST
} switch_xml_config_type_t;

typedef struct {
	char *key;					/*< The item's key or NULL if this is the last one in the list */
	int value;					/*< The item's value */
} switch_xml_config_enum_item_t;

typedef struct {
	switch_memory_pool_t *pool;	/*< If set, the string will be allocated on the pool (unless the length param is > 0, then you misread this file) */
	switch_size_t length;		/*< Length of the char array, or 0 if memory has to be allocated dynamically */
	char *validation_regex;		/*< Enforce validation using this regular expression */
} switch_xml_config_string_options_t;

SWITCH_DECLARE_DATA extern switch_xml_config_string_options_t switch_config_string_strdup;	/*< String options structure for strdup, no validation */

typedef struct {
	switch_bool_t enforce_min;
	int min;
	switch_bool_t enforce_max;
	int max;
} switch_xml_config_int_options_t;

struct switch_xml_config_item;
typedef struct switch_xml_config_item switch_xml_config_item_t;

typedef enum {
	CONFIG_LOAD,
	CONFIG_RELOAD,
	CONFIG_SHUTDOWN
} switch_config_callback_type_t;

typedef enum {
	CONFIG_RELOADABLE = (1 << 0),
	CONFIG_REQUIRED = (1 << 1)
} switch_config_flags_t;

typedef switch_status_t (*switch_xml_config_callback_t) (switch_xml_config_item_t *item, const char *newvalue, switch_config_callback_type_t callback_type,
														 switch_bool_t changed);

/*!
 * \brief A configuration instruction read by switch_xml_config_parse 
*/
struct switch_xml_config_item {
	const char *key;			/*< The key of the element, or NULL to indicate the end of the list */
	switch_xml_config_type_t type;	/*< The type of variable */
	int flags;					/*< True if the var can be changed on reload */
	void *ptr;					/*< Ptr to the var to be changed */
	const void *defaultvalue;	/*< Default value */
	void *data;					/*< Custom data (depending on the type) */
	switch_xml_config_callback_t function;	/*< Callback to be called after the var is parsed */
	const char *syntax;			/*< Optional syntax documentation for this setting */
	const char *helptext;		/*< Optional documentation text for this setting */
};

#define SWITCH_CONFIG_ITEM(_key, _type, _flags, _ptr, _defaultvalue, _data, _syntax, _helptext)	{ _key, _type, _flags, _ptr, (void*)_defaultvalue, (void*)_data, NULL, _syntax, _helptext }
#define SWITCH_CONFIG_ITEM_STRING_STRDUP(_key, _flags, _ptr, _defaultvalue, _syntax, _helptext)	{ (_key), SWITCH_CONFIG_STRING, (_flags), (_ptr), ((void*)_defaultvalue), (NULL), (NULL), (_syntax), (_helptext) }
#define SWITCH_CONFIG_ITEM_CALLBACK(_key, _type, _flags, _ptr, _defaultvalue, _function, _functiondata, _syntax, _helptext)	{ _key, _type, _flags, _ptr, (void*)_defaultvalue, _functiondata, _function, _syntax, _helptext }
#define SWITCH_CONFIG_ITEM_END() { NULL, SWITCH_CONFIG_LAST, 0, NULL, NULL, NULL, NULL, NULL, NULL }

#define SWITCH_CONFIG_SET_ITEM(_item, _key, _type, _flags, _ptr, _defaultvalue, _data, _syntax, _helptext)  switch_config_perform_set_item(&(_item), _key, _type, _flags, _ptr, (void*)(_defaultvalue), _data, NULL, _syntax, _helptext)
#define SWITCH_CONFIG_SET_ITEM_CALLBACK(_item, _key, _type, _flags, _ptr, _defaultvalue, _data, _function, _syntax, _helptext)  switch_config_perform_set_item(&(_item), _key, _type, _flags, _ptr, (void*)(_defaultvalue), _data, _function, _syntax, _helptext)

SWITCH_DECLARE(void) switch_config_perform_set_item(switch_xml_config_item_t *item, const char *key, switch_xml_config_type_t type, int flags, void *ptr,
													const void *defaultvalue, void *data, switch_xml_config_callback_t function, const char *syntax,
													const char *helptext);

/*! 
 * \brief Gets the int representation of an enum
 * \param enum_options the switch_xml_config_enum_item_t array for this enum
 * \param value string value to search 
 */
SWITCH_DECLARE(switch_status_t) switch_xml_config_enum_str2int(switch_xml_config_enum_item_t *enum_options, const char *value, int *out);

/*! 
 * \brief Gets the string representation of an enum
 * \param enum_options the switch_xml_config_enum_item_t array for this enum
 * \param value int value to search 
 */
SWITCH_DECLARE(const char *) switch_xml_config_enum_int2str(switch_xml_config_enum_item_t *enum_options, int value);

/*!
 * \brief Prints out an item's documentation on the console 
 * \param level loglevel to use
 * \param item item which the doc should be printed
 */
SWITCH_DECLARE(void) switch_xml_config_item_print_doc(int level, switch_xml_config_item_t *item);

/*! 
 * \brief Parses all the xml elements, following a ruleset defined by an array of switch_xml_config_item_t 
 * \param xml The first element of the list to parse
 * \param reload true to skip all non-reloadable options
 * \param instructions instrutions on how to parse the elements
 * \see switch_xml_config_item_t
 */
SWITCH_DECLARE(switch_status_t) switch_xml_config_parse(switch_xml_t xml, switch_bool_t reload, switch_xml_config_item_t *instructions);

/*!
 * \brief Parses a module's settings
 * \param reload true to skip all non-reloadable options
 * \param file the configuration file to look for
 * \param instructions the instructions 
 */
SWITCH_DECLARE(switch_status_t) switch_xml_config_parse_module_settings(const char *file, switch_bool_t reload, switch_xml_config_item_t *instructions);

/*! 
 * \brief Parses all of an event's elements, following a ruleset defined by an array of switch_xml_config_item_t 
 * \param event The event structure containing the key and values to parse
 * \param reload true to skip all non-reloadable options
 * \param instructions instrutions on how to parse the elements
 * \see switch_xml_config_item_t
 */
SWITCH_DECLARE(switch_status_t) switch_xml_config_parse_event(switch_event_t *event, int count, switch_bool_t reload,
															  switch_xml_config_item_t *instructions);

/*!
 * \brief Parses a list of xml elements into an event  
 * \param xml First element of the xml list to parse
 * \param keyname Name of the key attribute
 * \param keyvalue Name of the value attribute 
 * \param event [out] event (if *event is NOT NULL, the headers will be appended to the existing event)
 */
SWITCH_DECLARE(switch_size_t) switch_event_import_xml(switch_xml_t xml, const char *keyname, const char *valuename, switch_event_t **event);

/*!
 * \brief Free any memory allocated by the configuration
 * \param instructions instrutions on how to parse the elements
 */
SWITCH_DECLARE(void) switch_xml_config_cleanup(switch_xml_config_item_t *instructions);

SWITCH_END_EXTERN_C
#endif /* !defined(SWITCH_XML_CONFIG_H) */
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
