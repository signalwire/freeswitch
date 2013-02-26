/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _plstr_h
#define _plstr_h

#include "cpr_types.h"

__BEGIN_DECLS

/*
 * PL_strtok_r
 *
 * Splits the string s1 into tokens, separated by one or more characters
 * from the separator string s2.  The argument lasts points to a
 * user-supplied char * pointer in which PL_strtok_r stores information
 * for it to continue scanning the same string.
 *
 * In the first call to PL_strtok_r, s1 points to a string and the value
 * of *lasts is ignored.  PL_strtok_r returns a pointer to the first
 * token, writes '\0' into the character following the first token, and
 * updates *lasts.
 *
 * In subsequent calls, s1 is null and lasts must stay unchanged from the
 * previous call.  The separator string s2 may be different from call to
 * call.  PL_strtok_r returns a pointer to the next token in s1.  When no
 * token remains in s1, PL_strtok_r returns null.
 */

char * PL_strtok_r(char *s1, const char *s2, char **lasts);

/*
 * Things not (yet?) included: strspn/strcspn, strsep.
 * memchr, memcmp, memcpy, memccpy, index, rindex, bcmp, bcopy, bzero.
 * Any and all i18n/l10n stuff.
 */

__END_DECLS

#endif /* _plstr_h */
