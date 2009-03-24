/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2008 Nokia Corporation.
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
 */

#ifndef S2BASE_H
#define S2BASE_H

#include <sofia-sip/su_wait.h>
#include <sofia-sip/su_alloc.h>
#include "s2util.h"

extern struct s2base
{
  su_home_t home[1];
  su_root_t *root;
} *s2base;

extern char const *s2_tester, *_s2_suite, *_s2_case;
extern int s2_start_stop;

void s2_suite(char const *label);
void s2_setup(char const *label);

void s2_step(void);

void s2_case(char const *tag,
	     char const *title,
	     char const *description,
	     char const *function);

#if HAVE_FUNC
#define S2_CASE(n, t, d) s2_case((n),(t),(d), __func__)
#elif HAVE_FUNCTION
#define S2_CASE(n, t, d) s2_case((n),(t),(d), __FUNCTION__)
#else
#define S2_CASE(n, t, d) s2_case((n),(t),(d), "")
#endif

void s2_teardown_started(char const *label);
void s2_teardown(void);

#endif
