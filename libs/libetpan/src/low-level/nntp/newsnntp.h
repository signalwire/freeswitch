/*
 * libEtPan! -- a mail stuff library
 *
 * Copyright (C) 2001, 2005 - DINH Viet Hoa
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the libEtPan! project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * $Id: newsnntp.h,v 1.19 2006/05/22 13:39:42 hoa Exp $
 */

#ifndef NEWSNNTP_H

#define NEWSNNTP_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_INTTYPES_H
#	include <inttypes.h>
#endif
#include <sys/types.h>
#include <time.h>

#include <libetpan/clist.h>
#include <libetpan/mailstream.h>
#include <libetpan/newsnntp_socket.h>
#include <libetpan/newsnntp_ssl.h>
#include <libetpan/newsnntp_types.h>


newsnntp * newsnntp_new(size_t nntp_progr_rate,
    progress_function * nntp_progr_fun);
void newsnntp_free(newsnntp * f);

int newsnntp_quit(newsnntp * f);
int newsnntp_connect(newsnntp * f, mailstream * s);

int newsnntp_head(newsnntp * f, uint32_t index, char ** result,
		  size_t * result_len);
int newsnntp_article(newsnntp * f, uint32_t index, char ** result,
		     size_t * result_len);
int newsnntp_body(newsnntp * f, uint32_t index, char ** result,
		  size_t * result_len);

void newsnntp_head_free(char * str);
void newsnntp_article_free(char * str);
void newsnntp_body_free(char * str);

int newsnntp_mode_reader(newsnntp * f);

int newsnntp_date(newsnntp * f, struct tm * tm);

int newsnntp_authinfo_generic(newsnntp * f, const char * authentificator,
			       const char * arguments);

int newsnntp_authinfo_username(newsnntp * f, const char * username);
int newsnntp_authinfo_password(newsnntp * f, const char * password);

int newsnntp_post(newsnntp * f, const char * message, size_t size);






/******************* requests ******************************/

int newsnntp_group(newsnntp * f, const char * groupname,
		    struct newsnntp_group_info ** info);
void newsnntp_group_free(struct newsnntp_group_info * info);

/*
  elements are struct newsnntp_group_info *
 */

int newsnntp_list(newsnntp * f, clist ** result);
void newsnntp_list_free(clist * l);

/*
  elements are char *
*/

int newsnntp_list_overview_fmt(newsnntp * f, clist ** result);
void newsnntp_list_overview_fmt_free(clist * l);

/*
  elements are struct newsnntp_group_info *
*/

int newsnntp_list_active(newsnntp * f, const char * wildcard, clist ** result);
void newsnntp_list_active_free(clist * l);

/*
  elements are struct newsnntp_group_time *
*/

int newsnntp_list_active_times(newsnntp * f, clist ** result);
void newsnntp_list_active_times_free(clist * l);

/*
  elements are struct newsnntp_distrib_value_meaning *
*/

int newsnntp_list_distribution(newsnntp * f, clist ** result);
void newsnntp_list_distribution_free(clist * l);

/*
  elements are struct newsnntp_distrib_default_value *
*/

int newsnntp_list_distrib_pats(newsnntp * f, clist ** result);
void newsnntp_list_distrib_pats_free(clist * l);

/*
  elements are struct newsnntp_group_description *
*/

int newsnntp_list_newsgroups(newsnntp * f, const char * pattern,
			      clist ** result);
void newsnntp_list_newsgroups_free(clist * l);

/*
  elements are char *
*/

int newsnntp_list_subscriptions(newsnntp * f, clist ** result);
void newsnntp_list_subscriptions_free(clist * l);

/*
  elements are uint32_t *
*/

int newsnntp_listgroup(newsnntp * f, const char * group_name,
		       clist ** result);
void newsnntp_listgroup_free(clist * l);

/*
  elements are struct newsnntp_xhdr_resp_item *
*/

int newsnntp_xhdr_single(newsnntp * f, const char * header, uint32_t article,
			  clist ** result);
int newsnntp_xhdr_range(newsnntp * f, const char * header,
			 uint32_t rangeinf, uint32_t rangesup,
			 clist ** result);
void newsnntp_xhdr_free(clist * l);

/*
  elements are struct newsnntp_xover_resp_item *
*/

int newsnntp_xover_single(newsnntp * f, uint32_t article,
			   struct newsnntp_xover_resp_item ** result);
int newsnntp_xover_range(newsnntp * f, uint32_t rangeinf, uint32_t rangesup,
			  clist ** result);
void xover_resp_item_free(struct newsnntp_xover_resp_item * n);
void newsnntp_xover_resp_list_free(clist * l);

#ifdef __cplusplus
}
#endif

#endif
