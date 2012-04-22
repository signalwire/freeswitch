/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2005 Nokia Corporation.
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

#ifndef S2_LOCALINFO_H
/** Defined when <s2_localinfo.h> has been included. */
#define S2_LOCALINFO_H


/**@internal
 * @file s2_localinfo.h - Test su_localinfo() users
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 */

#ifndef SU_LOCALINFO_H
#include <sofia-sip/su_localinfo.h>
#endif

SOFIA_BEGIN_DECLS

int s2_getlocalinfo(su_localinfo_t const *hints, su_localinfo_t **res);
void s2_freelocalinfo(su_localinfo_t *);
char const *s2_gli_strerror(int error);
su_localinfo_t *s2_copylocalinfo(su_localinfo_t const *li0);
int s2_sockaddr_scope(su_sockaddr_t const *su, socklen_t sulen);

void s2_localinfo_ifaces(char const **ifaces);

#define S2_LOCALINFO_STUBS(static)					\
  static int su_getlocalinfo(su_localinfo_t const *hints, su_localinfo_t **res)	\
  { return s2_getlocalinfo(hints, res); }				\
  static void su_freelocalinfo(su_localinfo_t *li)			\
  { s2_freelocalinfo(li); }						\
  static char const *su_gli_strerror(int error)				\
  { return s2_gli_strerror(error); }					\
  static su_localinfo_t *su_copylocalinfo(su_localinfo_t const *li0)	\
  { return s2_copylocalinfo(li0); }					\
  static int su_sockaddr_scope(su_sockaddr_t const *su, socklen_t sulen) \
  { return s2_sockaddr_scope(su, sulen); }				\
  static int su_localinfo_stubs						\

SOFIA_END_DECLS

#endif /* !defined(S2_LOCALINFO_H) */
