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

#ifndef TPORT_TLS_H
/** Defined when <tport_tls.h> has been included. */
#define TPORT_TLS_H
/**@internal
 * @file tport_tls.h
 * @brief Internal TLS interface
 *
 * @author Mikko Haataja <ext-Mikko.A.Haataja@nokia.com>
 *
 * Copyright 2001, 2002 Nokia Research Center.  All rights reserved.
 *
 */

#ifndef SU_TYPES_H
#include <sofia-sip/su_types.h>
#endif

#include "tport_internal.h"

SOFIA_BEGIN_DECLS

#define TLS_MAX_HOSTS (16)

typedef struct tls_s tls_t;

extern char const tls_version[];

typedef struct tls_issues_s {
  unsigned policy;      /* refer to tport_tag.h, tport_tls_verify_policy */
  unsigned verify_depth;/* if 0, revert to default (2) */
  unsigned verify_date; /* if 0, notBefore and notAfter dates are ignored */
  int   configured;	/* If non-zero, complain about certificate errors */
  char *cert;		/* CERT file name. File format is PEM         */
  char *key;		/* Private key file. PEM format               */
  char *randFile;       /* Seed file for the PRNG (default: tls_seed.dat) */
  char *CAfile;		/* PEM file of CA's                           */
  char *CApath;		/* PEM file path of CA's		      */
  char *cipher;         /* Should be one of the above defined ciphers *
			 * or NULL (default: "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH
                         */
  int   version;	/* For tls1, version is 1. When ssl3/ssl2 is
			 * used, it is 0. */
} tls_issues_t;

typedef struct tport_tls_s {
  tport_t  tlstp_tp[1];
  tls_t   *tlstp_context;
  char    *tlstp_buffer;
} tport_tls_t;

typedef struct tport_tls_primary_s {
  tport_primary_t tlspri_pri[1];
  tls_t *tlspri_master;
} tport_tls_primary_t;

tls_t *tls_init_master(tls_issues_t *tls_issues);
tls_t *tls_init_secondary(tls_t *tls_master, int sock, int accept);
void tls_free(tls_t *tls);
int tls_get_socket(tls_t *tls);
ssize_t tls_read(tls_t *tls);
void *tls_read_buffer(tls_t *tls, size_t N);
int tls_want_read(tls_t *tls, int events);
int tls_pending(tls_t const *tls);

int tls_connect(su_root_magic_t *magic, su_wait_t *w, tport_t *self);
ssize_t tls_write(tls_t *tls, void *buf, size_t size);
int tls_want_write(tls_t *tls, int events);

int tls_events(tls_t const *tls, int flags);

SOFIA_END_DECLS

#endif
