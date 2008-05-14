#ifndef CHECK_NUA_H

#include <check.h>

#undef tcase_add_test
#define tcase_add_test(tc, tf) \
  check_nua_tcase_add_test(tc, tf, "" #tf "")

void check_nua_tcase_add_test(TCase *, TFun, char const *name);

void check_session_cases(Suite *suite);
void check_register_cases(Suite *suite);

#endif

