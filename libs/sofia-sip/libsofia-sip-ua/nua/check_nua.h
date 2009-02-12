#ifndef CHECK_NUA_H

#include <s2check.h>

void check_session_cases(Suite *suite, int threading);
void check_register_cases(Suite *suite, int threading);
void check_etsi_cases(Suite *suite, int threading);
void check_simple_cases(Suite *suite, int threading);

#endif

