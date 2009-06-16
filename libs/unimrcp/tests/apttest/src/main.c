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

#include "apt_test_suite.h"
#include "apt_log.h"

apt_test_suite_t* task_test_suite_create(apr_pool_t *pool);
apt_test_suite_t* consumer_task_test_suite_create(apr_pool_t *pool);

int main(int argc, const char * const *argv)
{
	apt_test_framework_t *test_framework;
	apt_test_suite_t *test_suite;
	apr_pool_t *pool;
	
	/* one time apr global initialization */
	if(apr_initialize() != APR_SUCCESS) {
		return 0;
	}

	/* create test framework */
	test_framework = apt_test_framework_create();
	pool = apt_test_framework_pool_get(test_framework);

	/* create test suites and add them to test framework */
	test_suite = task_test_suite_create(pool);
	apt_test_framework_suite_add(test_framework,test_suite);

	test_suite = consumer_task_test_suite_create(pool);
	apt_test_framework_suite_add(test_framework,test_suite);

	/* run tests */
	apt_test_framework_run(test_framework,argc,argv);

	/* destroy test framework */
	apt_test_framework_destroy(test_framework);

	/* final apr global termination */
	apr_terminate();
	return 0;
}
