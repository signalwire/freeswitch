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
 *
 * mod_expr.c -- Framework Demo Module
 *
 */
#include <switch.h>
#include "expreval.h"

/* Breaker function to break out of long expression functions
   such as the 'for' function */
int breaker(exprObj * o)
{
	/* Return nonzero to break out */
	return -1;
}


SWITCH_STANDARD_API(expr_function)
{
	exprObj *e = NULL;
	exprFuncList *f = NULL;
	exprValList *v = NULL;
	exprValList *c = NULL;
	EXPRTYPE last_expr;
	const char *expr;
	int err;
	char val[512] = "", *p;
	char *m_cmd = NULL;
	size_t len;
	int ec = 0;

	if (zstr(cmd)) {
		goto error;
	}

	len = strlen(cmd) + 3;


	m_cmd = malloc(len);
	switch_assert(m_cmd);
	switch_copy_string(m_cmd, cmd, len);

	for (p = m_cmd; p && *p; p++) {
		if (*p == '|') {
			*p = ';';
		}
	}

	p = m_cmd + (strlen(m_cmd) - 1);
	if (*p != ';') {
		p++;
		*p = ';';
		p++;
		*p = '\0';
	}

	expr = m_cmd;

	/* Create function list */
	err = exprFuncListCreate(&f);
	if (err != EXPR_ERROR_NOERROR)
		goto error;

	/* Init function list with internal functions */
	err = exprFuncListInit(f);
	if (err != EXPR_ERROR_NOERROR)
		goto error;

	/* Add custom function */
	//err = exprFuncListAdd(f, my_func, "myfunc", 1, 1, 1, 1);
	//if (err != EXPR_ERROR_NOERROR)
	//goto error;

	/* Create constant list */
	err = exprValListCreate(&c);
	if (err != EXPR_ERROR_NOERROR)
		goto error;

	/* Init constant list with internal constants */
	err = exprValListInit(c);
	if (err != EXPR_ERROR_NOERROR)
		goto error;

	/* Create variable list */
	err = exprValListCreate(&v);
	if (err != EXPR_ERROR_NOERROR)
		goto error;

	/* Create expression object */
	err = exprCreate(&e, f, v, c, breaker, NULL);
	if (err != EXPR_ERROR_NOERROR)
		goto error;

	/* Parse expression */
	err = exprParse(e, (char *) expr);

	if (err != EXPR_ERROR_NOERROR)
		goto error;

	/* Enable soft errors */
	//exprSetSoftErrors(e, 1);

	do {
		err = exprEval(e, &last_expr);
		if (err) {
			ec++;
		} else {
			ec = 0;
		}
	} while (err && ec < 3);

	if (err) {
		goto error;
	}

	switch_snprintf(val, sizeof(val), "%0.10f", last_expr);
	for (p = (val + strlen(val) - 1); p != val; p--) {
		if (*p != '0') {
			*(p + 1) = '\0';
			break;
		}
	}

	p = val + strlen(val) - 1;
	if (*p == '.') {
		*p = '\0';
	}

	stream->write_function(stream, "%s", val);


	goto done;

  error:
	/* Alert user of error */
	stream->write_function(stream, "!err!");


  done:
	/* Do cleanup */
	if (e) {
		exprFree(e);
	}

	if (f) {
		exprFuncListFree(f);
	}

	if (v) {
		exprValListFree(v);
	}

	if (c) {
		exprValListFree(c);
	}

	switch_safe_free(m_cmd);

	return SWITCH_STATUS_SUCCESS;
}


SWITCH_MODULE_LOAD_FUNCTION(mod_expr_load);
SWITCH_MODULE_DEFINITION(mod_expr, mod_expr_load, NULL, NULL);

SWITCH_MODULE_LOAD_FUNCTION(mod_expr_load)
{
	switch_api_interface_t *commands_api_interface;

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	SWITCH_ADD_API(commands_api_interface, "expr", "Eval an expression", expr_function, "<expr>");


	/* indicate that the module should continue to be loaded */
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
