/*
 * test.h for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2013, Grasshopper
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
 * The Original Code is test.h for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is Grasshopper
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Chris Rienzo <chris.rienzo@grasshopper.com>
 *
 * test.h -- simple unit testing macros
 *
 */
#ifndef TEST_H
#define TEST_H

#define assert_equals(test, expected_str, expected, actual, file, line) \
{ \
	int actual_val = actual; \
	if (expected != actual_val) { \
		printf("TEST\t%s\tFAIL\t%s\t%i\t!=\t%i\t%s:%i\n", test, expected_str, expected, actual_val, file, line); \
		exit(1); \
	} else { \
		printf("TEST\t%s\tPASS\n", test); \
	} \
}

#define assert_string_equals(test, expected, actual, file, line) \
{ \
	const char *actual_str = actual; \
	if (!actual_str || strcmp(expected, actual_str)) { \
		printf("TEST\t%s\tFAIL\t\t%s\t!=\t%s\t%s:%i\n", test, expected, actual_str, file, line); \
		exit(1); \
	} else { \
		printf("TEST\t%s\tPASS\n", test); \
	} \
}

#define assert_not_null(test, actual, file, line) \
{ \
	const void *actual_val = actual; \
	if (!actual_val) { \
		printf("TEST\t%s\tFAIL\t\t\t\t\t%s:%i\n", test, file, line); \
		exit(1); \
	} else { \
		printf("TEST\t%s\tPASS\n", test); \
	} \
}

#define assert_null(test, actual, file, line) \
{ \
	const void *actual_val = actual; \
	if (actual_val) { \
		printf("TEST\t%s\tFAIL\t\t\t\t\t%s:%i\n", test, file, line); \
		exit(1); \
	} else { \
		printf("TEST\t%s\tPASS\n", test); \
	} \
}

#define assert_true(test, actual, file, line) \
{ \
	int actual_val = actual; \
	if (!actual_val) { \
		printf("TEST\t%s\tFAIL\t\t\t\t\t%s:%i\n", test, file, line); \
		exit(1); \
	} else { \
		printf("TEST\t%s\tPASS\n", test); \
	} \
}

#define assert_false(test, actual, file, line) \
{ \
	int actual_val = actual; \
	if (actual_val) { \
		printf("TEST\t%s\tFAIL\t\t\t\t\t%s:%i\n", test, file, line); \
		exit(1); \
	} else { \
		printf("TEST\t%s\tPASS\n", test); \
	} \
}

#define ASSERT_EQUALS(expected, actual) assert_equals(#actual, #expected, expected, actual, __FILE__, __LINE__)
#define ASSERT_STRING_EQUALS(expected, actual) assert_string_equals(#actual, expected, actual, __FILE__, __LINE__)
#define ASSERT_NOT_NULL(actual) assert_not_null(#actual " not null", actual, __FILE__, __LINE__)
#define ASSERT_NULL(actual) assert_null(#actual " is null", actual, __FILE__, __LINE__)
#define ASSERT_TRUE(actual) assert_true(#actual " is true", actual, __FILE__, __LINE__)
#define ASSERT_FALSE(actual) assert_false(#actual " is false", actual, __FILE__, __LINE__)

#define SKIP_ASSERT_EQUALS(expected, actual) if (0) { ASSERT_EQUALS(expected, actual); }

#define TEST(name) printf("TEST BEGIN\t" #name "\n"); name(); printf("TEST END\t"#name "\tPASS\n");

#define SKIP_TEST(name) if (0) { TEST(name) };

#define TEST_INIT const char *err; switch_core_init(0, SWITCH_TRUE, &err);

#endif
