/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2009 Nokia Corporation.
 *
 * Contact: Pekka Pessi <pekka.pessi@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#ifndef S2UTIL_H
/** Defined when <s2util.h> has been included. */
#define S2UTIL_H

/**@internal @file s2util.h
 *
 * @brief Miscellaneous testing utilities
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 */

#include <sofia-sip/su_wait.h>
#include <stdarg.h>

SOFIA_BEGIN_DECLS

void s2_fast_forward(unsigned long seconds, su_root_t *root);
void s2_timed_logger(void *stream, char const *fmt, va_list ap);

SOFIA_END_DECLS

#endif /* S2UTIL_H */
