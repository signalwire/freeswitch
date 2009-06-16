/*
 * Copyright 2008 Arsen Chaloyan
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "apt_pool.h"
#include "apt_obj_list.h"
#include "apt_test_suite.h"
#include "apt_log.h"

struct apt_test_framework_t{
	apr_pool_t     *pool;
	apt_obj_list_t *suites;
};

APT_DECLARE(apt_test_suite_t*) apt_test_suite_create(apr_pool_t *pool, const char *name, 
													 void *obj, apt_test_f tester)
{
	apt_test_suite_t *suite = apr_palloc(pool,sizeof(apt_test_suite_t));
	suite->pool = pool;
	apt_string_assign(&suite->name,name,pool);
	suite->obj = obj;
	suite->tester = tester;
	return suite;
}

APT_DECLARE(apt_test_framework_t*) apt_test_framework_create()
{
	apt_test_framework_t *framework;
	apr_pool_t* pool = apt_pool_create();
	framework = apr_palloc(pool,sizeof(apt_test_framework_t));
	framework->pool = pool;
	framework->suites = apt_list_create(pool);

	apt_log_instance_create(APT_LOG_OUTPUT_CONSOLE,APT_PRIO_INFO,pool);
	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Create Test Framework");
	return framework;
}

APT_DECLARE(void) apt_test_framework_destroy(apt_test_framework_t *framework)
{
	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Destroy Test Framework");
	apt_log_instance_destroy();
	apr_pool_destroy(framework->pool);
}

APT_DECLARE(apt_bool_t) apt_test_framework_suite_add(apt_test_framework_t *framework, apt_test_suite_t *suite)
{
	apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Add Test Suite [%s]",suite->name);
	return (apt_list_push_back(framework->suites,suite,suite->pool) ? TRUE : FALSE);
}

APT_DECLARE(apr_pool_t*) apt_test_framework_pool_get(apt_test_framework_t *framework)
{
	return framework->pool;
}

static apt_bool_t apt_test_framework_suite_run(apt_test_framework_t *framework, apt_test_suite_t *suite,
											   int argc, const char * const *argv)
{
	apt_bool_t status = FALSE;
	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"----- Run Test Suite [%s] -----",suite->name);
	if(suite->tester) {
		status = suite->tester(suite,argc,argv);
	}
	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"---- Status [%s] ----\n",(status == TRUE) ? "OK" : "Failure");
	return status;
}

APT_DECLARE(apt_bool_t) apt_test_framework_run(apt_test_framework_t *framework, int argc, const char * const *argv)
{
	apt_test_suite_t *suite = NULL;
	apt_list_elem_t *elem = apt_list_first_elem_get(framework->suites);
	if(argc == 1) {
		apt_log(APT_LOG_MARK,APT_PRIO_INFO,"Run All Test Suites");
		/* walk through the list of test suites and run all of them */
		while(elem) {
			suite = apt_list_elem_object_get(elem);
			if(suite) {
				/* run test suite with the default arguments */
				apt_test_framework_suite_run(framework,suite,0,NULL);
			}
			elem = apt_list_next_elem_get(framework->suites,elem);
		}
	}
	else {
		/* walk through the list of test suites find appropriate one and run it */
		apt_bool_t found = FALSE;
		apt_str_t name;
		apt_string_set(&name,argv[1]);
		while(elem) {
			suite = apt_list_elem_object_get(elem);
			if(suite && apt_string_compare(&suite->name,&name) == TRUE) {
				found = TRUE;
				break;
			}
			elem = apt_list_next_elem_get(framework->suites,elem);
		}
		if(found == TRUE) {
			/* run test suite with remaining arguments */
			apt_test_framework_suite_run(framework,suite,argc-2,&argv[2]);
		}
		else {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"No Such Test Suite [%s] to Run", argv[1]);
		}
	}
	return TRUE;
}
