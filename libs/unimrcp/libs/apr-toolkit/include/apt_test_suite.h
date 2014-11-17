/*
 * Copyright 2008-2014 Arsen Chaloyan
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
 * 
 * $Id: apt_test_suite.h 2136 2014-07-04 06:33:36Z achaloyan@gmail.com $
 */

#ifndef APT_TEST_SUITE_H
#define APT_TEST_SUITE_H

/**
 * @file apt_test_suite.h
 * @brief Test Suite and Framework Definitions
 */ 

#include "apt_string.h"

APT_BEGIN_EXTERN_C


/** Opaque test suite declaration */
typedef struct apt_test_suite_t apt_test_suite_t;

/** Prototype of test function */
typedef apt_bool_t (*apt_test_f)(apt_test_suite_t *suite, int argc, const char * const *argv);

/** Test suite as a base for all kind of tests */
struct apt_test_suite_t {
	/** Memory pool to allocate memory from */
	apr_pool_t *pool;
	/** Unique name of the test suite */
	apt_str_t   name;
	/** External object associated with the test suite */
	void       *obj;
	/** Test function to execute */
	apt_test_f  tester;
};

/**
 * Create test suite.
 * @param pool the pool to allocate memory from
 * @param name the unique name of the test suite
 * @param obj the external object associated with the test suite
 * @param tester the test function to execute
 */
APT_DECLARE(apt_test_suite_t*) apt_test_suite_create(apr_pool_t *pool, const char *name, 
                                                     void *obj, apt_test_f tester);





/** Opaque test framework declaration */
typedef struct apt_test_framework_t apt_test_framework_t;

/**
 * Create test framework.
 */
APT_DECLARE(apt_test_framework_t*) apt_test_framework_create(void);

/**
 * Destroy test framework.
 * @param framework the test framework to destroy
 */
APT_DECLARE(void) apt_test_framework_destroy(apt_test_framework_t *framework);

/**
 * Add test suite to framework.
 * @param framework the test framework to add test suite to
 * @param suite the test suite to add
 */
APT_DECLARE(apt_bool_t) apt_test_framework_suite_add(apt_test_framework_t *framework, apt_test_suite_t *suite);

/**
 * Run test suites.
 * @param framework the test framework
 * @param argc the number of arguments
 * @param argv the array of arguments
 */
APT_DECLARE(apt_bool_t) apt_test_framework_run(apt_test_framework_t *framework, int argc, const char * const *argv);

/**
 * Retrieve the memory pool.
 * @param framework the test framework to retrieve memory pool from
 */
APT_DECLARE(apr_pool_t*) apt_test_framework_pool_get(const apt_test_framework_t *framework);

APT_END_EXTERN_C

#endif /* APT_TEST_SUITE_H */
