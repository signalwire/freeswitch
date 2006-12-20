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
 * $Id: mailimap_types.c,v 1.26 2006/10/25 23:10:59 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailimap_types.h"
#include "mmapstring.h"
#include "mail.h"
#include "mailimap_extension.h"

#include <stdlib.h>
#include <stdio.h>

/* ************************************************************************* */
/* ************************************************************************* */
/* ************************************************************************* */
/* ************************************************************************* */
/* ************************************************************************* */
/* ************************************************************************* */






/* from parser */


uint32_t * mailimap_number_alloc_new(uint32_t number)
{
  uint32_t * pnumber;

  pnumber = malloc(sizeof(* pnumber));
  if (pnumber == NULL)
    return NULL;

  * pnumber = number;

  return pnumber;
}

void mailimap_number_alloc_free(uint32_t * pnumber)
{
  free(pnumber);
}


/* ************************************************************************* */


struct mailimap_address *
mailimap_address_new(char * ad_personal_name, char * ad_source_route,
		     char * ad_mailbox_name, char * ad_host_name)
{
  struct mailimap_address * addr;

  addr = malloc(sizeof(* addr));
  if (addr == NULL)
    return NULL;

  addr->ad_personal_name = ad_personal_name;
  addr->ad_source_route = ad_source_route;
  addr->ad_mailbox_name = ad_mailbox_name;
  addr->ad_host_name = ad_host_name;

  return addr;
}

void mailimap_address_free(struct mailimap_address * addr)
{
  mailimap_addr_host_free(addr->ad_host_name);
  mailimap_addr_mailbox_free(addr->ad_mailbox_name);
  mailimap_addr_adl_free(addr->ad_source_route);
  mailimap_addr_name_free(addr->ad_personal_name);
  free(addr);
}

void mailimap_addr_host_free(char * addr_host)
{
  mailimap_nstring_free(addr_host);
}

void mailimap_addr_mailbox_free(char * addr_mailbox)
{
  mailimap_nstring_free(addr_mailbox);
}

void mailimap_addr_adl_free(char * addr_adl)
{
  mailimap_nstring_free(addr_adl);
}

void mailimap_addr_name_free(char * addr_name)
{
  mailimap_nstring_free(addr_name);
}





/*
struct mailimap_astring *
mailimap_astring_new(gint type,
		     gchar * atom_astring,
		     gchar * string)
{
  struct mailimap_astring * astring;

  astring = g_new(struct mailimap_astring, 1);
  if (astring == NULL)
    return FALSE;

  astring->type = type;
  astring->atom_astring = atom_astring;
  astring->string = string;
  
  return astring;
}

void mailimap_astring_free(struct mailimap_astring * astring)
{
  if (astring->atom_astring)
    mailimap_atom_astring_free(astring->atom_astring);
  if (astring->string)
    mailimap_string_free(astring->string);
  free(astring);
}
*/

void mailimap_astring_free(char * astring)
{
  if (mmap_string_unref(astring) != 0)
    free(astring);
}

static void mailimap_custom_string_free(char * str)
{
  free(str);
}


void mailimap_atom_free(char * atom)
{
  free(atom);
}




void mailimap_auth_type_free(char * auth_type)
{
  mailimap_atom_free(auth_type);
}





void mailimap_base64_free(char * base64)
{
  free(base64);
}




struct mailimap_body *
mailimap_body_new(int bd_type,
		  struct mailimap_body_type_1part * bd_body_1part,
		  struct mailimap_body_type_mpart * bd_body_mpart)
{
  struct mailimap_body * body;
  
  body = malloc(sizeof(* body));
  if (body == NULL)
    return NULL;

  body->bd_type = bd_type;
  switch (bd_type) {
  case MAILIMAP_BODY_1PART:
    body->bd_data.bd_body_1part = bd_body_1part;
    break;
  case MAILIMAP_BODY_MPART:
    body->bd_data.bd_body_mpart = bd_body_mpart;
    break;
  }
  
  return body;
}

void mailimap_body_free(struct mailimap_body * body)
{
  switch (body->bd_type) {
  case MAILIMAP_BODY_1PART:
    mailimap_body_type_1part_free(body->bd_data.bd_body_1part);
    break;
  case MAILIMAP_BODY_MPART:
    mailimap_body_type_mpart_free(body->bd_data.bd_body_mpart);
    break;
  }
  free(body);
}


struct mailimap_body_extension *
mailimap_body_extension_new(int ext_type, char * ext_nstring,
			    uint32_t ext_number,
    clist * ext_body_extension_list)
{
  struct mailimap_body_extension * body_extension;

  body_extension = malloc(sizeof(* body_extension));
  if (body_extension == NULL)
    return NULL;

  body_extension->ext_type = ext_type;
  switch (ext_type) {
  case MAILIMAP_BODY_EXTENSION_NSTRING:
    body_extension->ext_data.ext_nstring = ext_nstring;
    break;
  case MAILIMAP_BODY_EXTENSION_NUMBER:
    body_extension->ext_data.ext_number = ext_number;
    break;
  case MAILIMAP_BODY_EXTENSION_LIST:
    body_extension->ext_data.ext_body_extension_list = ext_body_extension_list;
    break;
  }
  
  return body_extension;
}

static void
mailimap_body_ext_list_free(clist * body_ext_list);

void mailimap_body_extension_free(struct mailimap_body_extension * be)
{
  switch (be->ext_type) {
  case MAILIMAP_BODY_EXTENSION_NSTRING:
    mailimap_nstring_free(be->ext_data.ext_nstring);
    break;
  case MAILIMAP_BODY_EXTENSION_LIST:
    mailimap_body_ext_list_free(be->ext_data.ext_body_extension_list);
    break;
  }
  
  free(be);
}


static void
mailimap_body_ext_list_free(clist * body_ext_list)
{
  clist_foreach(body_ext_list, (clist_func) mailimap_body_extension_free,
		NULL);
  clist_free(body_ext_list);
}


struct mailimap_body_ext_1part *
mailimap_body_ext_1part_new(char * bd_md5,
			    struct mailimap_body_fld_dsp * bd_disposition,
			    struct mailimap_body_fld_lang * bd_language,
			    clist * bd_extension_list)
{
  struct mailimap_body_ext_1part * body_ext_1part;
  
  body_ext_1part = malloc(sizeof(* body_ext_1part));
  if (body_ext_1part == NULL)
    return NULL;

  body_ext_1part->bd_md5 = bd_md5;
  body_ext_1part->bd_disposition = bd_disposition;
  body_ext_1part->bd_language = bd_language;
  body_ext_1part->bd_extension_list = bd_extension_list;

  return body_ext_1part;
}

void
mailimap_body_ext_1part_free(struct mailimap_body_ext_1part * body_ext_1part)
{
  mailimap_body_fld_md5_free(body_ext_1part->bd_md5);
  if (body_ext_1part->bd_disposition)
    mailimap_body_fld_dsp_free(body_ext_1part->bd_disposition);
  if (body_ext_1part->bd_language)
    mailimap_body_fld_lang_free(body_ext_1part->bd_language);
  if (body_ext_1part->bd_extension_list)
    mailimap_body_ext_list_free(body_ext_1part->bd_extension_list);

  free(body_ext_1part);
}

struct mailimap_body_ext_mpart *
mailimap_body_ext_mpart_new(struct mailimap_body_fld_param * bd_parameter,
    struct mailimap_body_fld_dsp * bd_disposition,
    struct mailimap_body_fld_lang * bd_language,
    clist * bd_extension_list)
{
  struct mailimap_body_ext_mpart * body_ext_mpart;

  body_ext_mpart = malloc(sizeof(* body_ext_mpart));
  if (body_ext_mpart == NULL)
    return NULL;

  body_ext_mpart->bd_parameter = bd_parameter;
  body_ext_mpart->bd_disposition = bd_disposition;
  body_ext_mpart->bd_language = bd_language;
  body_ext_mpart->bd_extension_list = bd_extension_list;

  return body_ext_mpart;
}

void
mailimap_body_ext_mpart_free(struct mailimap_body_ext_mpart * body_ext_mpart)
{
  if (body_ext_mpart->bd_parameter != NULL)
    mailimap_body_fld_param_free(body_ext_mpart->bd_parameter);
  if (body_ext_mpart->bd_disposition)
    mailimap_body_fld_dsp_free(body_ext_mpart->bd_disposition);
  if (body_ext_mpart->bd_language)
    mailimap_body_fld_lang_free(body_ext_mpart->bd_language);
  if (body_ext_mpart->bd_extension_list)
    mailimap_body_ext_list_free(body_ext_mpart->bd_extension_list);
  free(body_ext_mpart);
}


struct mailimap_body_fields *
mailimap_body_fields_new(struct mailimap_body_fld_param * bd_parameter,
			 char * bd_id,
			 char * bd_description,
			 struct mailimap_body_fld_enc * bd_encoding,
			 uint32_t bd_size)
{
  struct mailimap_body_fields * body_fields;

  body_fields = malloc(sizeof(* body_fields));
  if (body_fields == NULL)
    return NULL;
  body_fields->bd_parameter = bd_parameter;
  body_fields->bd_id = bd_id;
  body_fields->bd_description = bd_description;
  body_fields->bd_encoding = bd_encoding;
  body_fields->bd_size = bd_size;

  return body_fields;
}

void
mailimap_body_fields_free(struct mailimap_body_fields * body_fields)
{
  if (body_fields->bd_parameter != NULL)
    mailimap_body_fld_param_free(body_fields->bd_parameter);
  mailimap_body_fld_id_free(body_fields->bd_id);
  mailimap_body_fld_desc_free(body_fields->bd_description);
  mailimap_body_fld_enc_free(body_fields->bd_encoding);
  free(body_fields);
}






void mailimap_body_fld_desc_free(char * body_fld_desc)
{
  mailimap_nstring_free(body_fld_desc);
}




struct mailimap_body_fld_dsp *
mailimap_body_fld_dsp_new(char * dsp_type,
    struct mailimap_body_fld_param * dsp_attributes)
{
  struct mailimap_body_fld_dsp * body_fld_dsp;

  body_fld_dsp = malloc(sizeof(* body_fld_dsp));
  if (body_fld_dsp == NULL)
    return NULL;

  body_fld_dsp->dsp_type = dsp_type;
  body_fld_dsp->dsp_attributes = dsp_attributes;

  return body_fld_dsp;
}

void mailimap_body_fld_dsp_free(struct mailimap_body_fld_dsp * bfd)
{
  if (bfd->dsp_type != NULL)
    mailimap_string_free(bfd->dsp_type);
  if (bfd->dsp_attributes != NULL)
    mailimap_body_fld_param_free(bfd->dsp_attributes);
  free(bfd);
}



struct mailimap_body_fld_enc *
mailimap_body_fld_enc_new(int enc_type, char * enc_value)
{
  struct mailimap_body_fld_enc * body_fld_enc;

  body_fld_enc = malloc(sizeof(* body_fld_enc));
  if (body_fld_enc == NULL)
    return NULL;
  
  body_fld_enc->enc_type = enc_type;
  body_fld_enc->enc_value = enc_value;

  return body_fld_enc;
}

void mailimap_body_fld_enc_free(struct mailimap_body_fld_enc * bfe)
{
  if (bfe->enc_value)
    mailimap_string_free(bfe->enc_value);
  free(bfe);
}



void mailimap_body_fld_id_free(char * body_fld_id)
{
  mailimap_nstring_free(body_fld_id);
}



struct mailimap_body_fld_lang *
mailimap_body_fld_lang_new(int lg_type, char * lg_single, clist * lg_list)
{
  struct mailimap_body_fld_lang * fld_lang;

  fld_lang = malloc(sizeof(* fld_lang));
  if (fld_lang == NULL)
    return NULL;
  
  fld_lang->lg_type = lg_type;
  switch (lg_type) {
  case MAILIMAP_BODY_FLD_LANG_SINGLE:
    fld_lang->lg_data.lg_single = lg_single;
    break;
  case MAILIMAP_BODY_FLD_LANG_LIST:
    fld_lang->lg_data.lg_list = lg_list;
    break;
  }

  return fld_lang;
}

void
mailimap_body_fld_lang_free(struct mailimap_body_fld_lang * fld_lang)
{
  switch (fld_lang->lg_type) {
  case MAILIMAP_BODY_FLD_LANG_SINGLE:
    mailimap_nstring_free(fld_lang->lg_data.lg_single);
    break;
  case MAILIMAP_BODY_FLD_LANG_LIST:
    clist_foreach(fld_lang->lg_data.lg_list,
        (clist_func) mailimap_string_free, NULL);
    clist_free(fld_lang->lg_data.lg_list);
    break;
  }
  free(fld_lang);
}



void mailimap_body_fld_md5_free(char * body_fld_md5)
{
  mailimap_nstring_free(body_fld_md5);
}



struct mailimap_single_body_fld_param *
mailimap_single_body_fld_param_new(char * pa_name, char * pa_value)
{
  struct mailimap_single_body_fld_param * param;

  param = malloc(sizeof(* param));
  if (param == NULL)
    return NULL;
  param->pa_name = pa_name;
  param->pa_value = pa_value;

  return param;
}

void
mailimap_single_body_fld_param_free(struct mailimap_single_body_fld_param * p)
{
  mailimap_string_free(p->pa_name);
  mailimap_string_free(p->pa_value);
  free(p);
}


struct mailimap_body_fld_param *
mailimap_body_fld_param_new(clist * pa_list)
{
  struct mailimap_body_fld_param * fld_param;

  fld_param = malloc(sizeof(* fld_param));
  if (fld_param == NULL)
    return NULL;
  fld_param->pa_list = pa_list;

  return fld_param;
}

void
mailimap_body_fld_param_free(struct mailimap_body_fld_param * fld_param)
{
  clist_foreach(fld_param->pa_list,
		(clist_func) mailimap_single_body_fld_param_free, NULL);
  clist_free(fld_param->pa_list);
  free(fld_param);
}


struct mailimap_body_type_1part *
mailimap_body_type_1part_new(int bd_type,
			     struct mailimap_body_type_basic * bd_type_basic,
			     struct mailimap_body_type_msg * bd_type_msg,
			     struct mailimap_body_type_text * bd_type_text,
			     struct mailimap_body_ext_1part * bd_ext_1part)
{
  struct mailimap_body_type_1part * body_type_1part;

  body_type_1part = malloc(sizeof(* body_type_1part));
  if (body_type_1part == NULL)
    return NULL;
  
  body_type_1part->bd_type = bd_type;
  switch (bd_type) {
  case MAILIMAP_BODY_TYPE_1PART_BASIC:
    body_type_1part->bd_data.bd_type_basic = bd_type_basic;
    break;
  case MAILIMAP_BODY_TYPE_1PART_MSG:
    body_type_1part->bd_data.bd_type_msg = bd_type_msg;
    break;
  case MAILIMAP_BODY_TYPE_1PART_TEXT:
    body_type_1part->bd_data.bd_type_text = bd_type_text;
    break;
  }
  body_type_1part->bd_ext_1part = bd_ext_1part;

  return body_type_1part;
}

void
mailimap_body_type_1part_free(struct mailimap_body_type_1part * bt1p)
{
  switch (bt1p->bd_type) {
  case MAILIMAP_BODY_TYPE_1PART_BASIC:
    mailimap_body_type_basic_free(bt1p->bd_data.bd_type_basic);
    break;
  case MAILIMAP_BODY_TYPE_1PART_MSG:
    mailimap_body_type_msg_free(bt1p->bd_data.bd_type_msg);
    break;
  case MAILIMAP_BODY_TYPE_1PART_TEXT:
    mailimap_body_type_text_free(bt1p->bd_data.bd_type_text);
    break;
  }
  if (bt1p->bd_ext_1part)
    mailimap_body_ext_1part_free(bt1p->bd_ext_1part);

  free(bt1p);
}



struct mailimap_body_type_basic *
mailimap_body_type_basic_new(struct mailimap_media_basic * bd_media_basic,
			     struct mailimap_body_fields * bd_fields)
{
  struct mailimap_body_type_basic * body_type_basic;

  body_type_basic = malloc(sizeof(* body_type_basic));
  if (body_type_basic == NULL)
    return NULL;

  body_type_basic->bd_media_basic = bd_media_basic;
  body_type_basic->bd_fields = bd_fields;
 
  return body_type_basic;
}

void mailimap_body_type_basic_free(struct mailimap_body_type_basic *
    body_type_basic)
{
  mailimap_media_basic_free(body_type_basic->bd_media_basic);
  mailimap_body_fields_free(body_type_basic->bd_fields);
  free(body_type_basic);
}


struct mailimap_body_type_mpart *
mailimap_body_type_mpart_new(clist * bd_list, char * bd_media_subtype,
			     struct mailimap_body_ext_mpart * bd_ext_mpart)
{
  struct mailimap_body_type_mpart * body_type_mpart;

  body_type_mpart = malloc(sizeof(* body_type_mpart));
  if (body_type_mpart == NULL)
    return NULL;

  body_type_mpart->bd_list = bd_list;
  body_type_mpart->bd_media_subtype = bd_media_subtype;
  body_type_mpart->bd_ext_mpart = bd_ext_mpart;

  return body_type_mpart;
}

void mailimap_body_type_mpart_free(struct mailimap_body_type_mpart *
				   body_type_mpart)
{
  clist_foreach(body_type_mpart->bd_list,
		(clist_func) mailimap_body_free, NULL);
  clist_free(body_type_mpart->bd_list);
  mailimap_media_subtype_free(body_type_mpart->bd_media_subtype);
  if (body_type_mpart->bd_ext_mpart)
    mailimap_body_ext_mpart_free(body_type_mpart->bd_ext_mpart);

  free(body_type_mpart);
}


struct mailimap_body_type_msg *
mailimap_body_type_msg_new(struct mailimap_body_fields * bd_fields,
			   struct mailimap_envelope * bd_envelope,
			   struct mailimap_body * bd_body,
			   uint32_t bd_lines)
{
  struct mailimap_body_type_msg * body_type_msg;

  body_type_msg = malloc(sizeof(* body_type_msg));
  if (body_type_msg == NULL)
    return NULL;

  body_type_msg->bd_fields = bd_fields;
  body_type_msg->bd_envelope = bd_envelope;
  body_type_msg->bd_body = bd_body;
  body_type_msg->bd_lines = bd_lines;

  return body_type_msg;
}

void
mailimap_body_type_msg_free(struct mailimap_body_type_msg * body_type_msg)
{
  mailimap_body_fields_free(body_type_msg->bd_fields);
  mailimap_envelope_free(body_type_msg->bd_envelope);
  mailimap_body_free(body_type_msg->bd_body);
  free(body_type_msg);
}



struct mailimap_body_type_text *
mailimap_body_type_text_new(char * bd_media_text,
			    struct mailimap_body_fields * bd_fields,
			    uint32_t bd_lines)
{
  struct mailimap_body_type_text * body_type_text;

  body_type_text = malloc(sizeof(* body_type_text));
  if (body_type_text == NULL)
    return NULL;

  body_type_text->bd_media_text = bd_media_text;
  body_type_text->bd_fields = bd_fields;
  body_type_text->bd_lines = bd_lines;

  return body_type_text;
}

void
mailimap_body_type_text_free(struct mailimap_body_type_text * body_type_text)
{
  mailimap_media_text_free(body_type_text->bd_media_text);
  mailimap_body_fields_free(body_type_text->bd_fields);
  free(body_type_text);
}



struct mailimap_capability *
mailimap_capability_new(int cap_type, char * cap_auth_type, char * cap_name)
{
  struct mailimap_capability * cap;

  cap = malloc(sizeof(* cap));
  if (cap == NULL)
    return NULL;
  cap->cap_type = cap_type;
  switch (cap_type) {
  case MAILIMAP_CAPABILITY_AUTH_TYPE:
    cap->cap_data.cap_auth_type = cap_auth_type;
    break;
  case MAILIMAP_CAPABILITY_NAME:
    cap->cap_data.cap_name = cap_name;
    break;
  }
  
  return cap;
}

void mailimap_capability_free(struct mailimap_capability * c)
{
  switch (c->cap_type) {
  case MAILIMAP_CAPABILITY_AUTH_TYPE:
    free(c->cap_data.cap_auth_type);
    break;
  case MAILIMAP_CAPABILITY_NAME:
    free(c->cap_data.cap_name);
    break;
  }
  free(c);
}


struct mailimap_capability_data *
mailimap_capability_data_new(clist * cap_list)
{
  struct mailimap_capability_data * cap_data;

  cap_data = malloc(sizeof(* cap_data));
  if (cap_data == NULL)
    return NULL;

  cap_data->cap_list = cap_list;

  return cap_data;
}

void
mailimap_capability_data_free(struct mailimap_capability_data * cap_data)
{
  if (cap_data->cap_list) {
    clist_foreach(cap_data->cap_list,
        (clist_func) mailimap_capability_free, NULL);
    clist_free(cap_data->cap_list);
  }
  free(cap_data);
}




struct mailimap_continue_req *
mailimap_continue_req_new(int cr_type, struct mailimap_resp_text * cr_text,
    char * cr_base64)
{
  struct mailimap_continue_req * cont_req;

  cont_req = malloc(sizeof(* cont_req));
  if (cont_req == NULL)
    return NULL;
  cont_req->cr_type = cr_type;
  switch (cr_type) {
  case MAILIMAP_CONTINUE_REQ_TEXT:
    cont_req->cr_data.cr_text = cr_text;
    break;
  case MAILIMAP_CONTINUE_REQ_BASE64:
    cont_req->cr_data.cr_base64 = cr_base64;
    break;
  }
  
  return cont_req;
}

void mailimap_continue_req_free(struct mailimap_continue_req * cont_req)
{
  switch (cont_req->cr_type) {
  case MAILIMAP_CONTINUE_REQ_TEXT:
    mailimap_resp_text_free(cont_req->cr_data.cr_text);
    break;
  case MAILIMAP_CONTINUE_REQ_BASE64:
    mailimap_base64_free(cont_req->cr_data.cr_base64);
    break;
  }
  free(cont_req);
}

struct mailimap_date_time *
mailimap_date_time_new(int dt_day, int dt_month, int dt_year, int dt_hour,
    int dt_min, int dt_sec, int dt_zone)
{
  struct mailimap_date_time * date_time;

  date_time = malloc(sizeof(* date_time));
  if (date_time == NULL)
    return NULL;

  date_time->dt_day = dt_day;
  date_time->dt_month = dt_month;
  date_time->dt_year = dt_year;
  date_time->dt_hour = dt_hour;
  date_time->dt_min = dt_min;
  date_time->dt_day = dt_sec;
  date_time->dt_zone = dt_zone;

  return date_time;
}

void mailimap_date_time_free(struct mailimap_date_time * date_time)
{
  free(date_time);
}



struct mailimap_envelope *
mailimap_envelope_new(char * env_date, char * env_subject,
		      struct mailimap_env_from * env_from,
		      struct mailimap_env_sender * env_sender,
		      struct mailimap_env_reply_to * env_reply_to,
		      struct mailimap_env_to * env_to,
		      struct mailimap_env_cc* env_cc,
		      struct mailimap_env_bcc * env_bcc,
		      char * env_in_reply_to, char * env_message_id)
{
  struct mailimap_envelope * env;

  env = malloc(sizeof(* env));
  if (env == NULL)
    return NULL;

  env->env_date = env_date;
  env->env_subject = env_subject;
  env->env_from = env_from;
  env->env_sender = env_sender;
  env->env_reply_to = env_reply_to;
  env->env_to = env_to;
  env->env_cc = env_cc;
  env->env_bcc = env_bcc;
  env->env_in_reply_to = env_in_reply_to;
  env->env_message_id = env_message_id;

  return env;
}


void mailimap_envelope_free(struct mailimap_envelope * env)
{
  if (env->env_date)
    mailimap_env_date_free(env->env_date);
  if (env->env_subject)
    mailimap_env_subject_free(env->env_subject);
  if (env->env_from)
    mailimap_env_from_free(env->env_from);
  if (env->env_sender)
    mailimap_env_sender_free(env->env_sender);
  if (env->env_reply_to)
    mailimap_env_reply_to_free(env->env_reply_to);
  if (env->env_to)
    mailimap_env_to_free(env->env_to);
  if (env->env_cc)
    mailimap_env_cc_free(env->env_cc);
  if (env->env_bcc)
    mailimap_env_bcc_free(env->env_bcc);
  if (env->env_in_reply_to)
    mailimap_env_in_reply_to_free(env->env_in_reply_to);
  if (env->env_message_id)
    mailimap_env_message_id_free(env->env_message_id);

  free(env);
}


static void mailimap_address_list_free(clist * addr_list)
{
  if (addr_list != NULL) {
    clist_foreach(addr_list, (clist_func) mailimap_address_free, NULL);
    clist_free(addr_list);
  }
}


struct mailimap_env_bcc * mailimap_env_bcc_new(clist * bcc_list)
{
  struct mailimap_env_bcc * env_bcc;

  env_bcc = malloc(sizeof(* env_bcc));
  if (env_bcc == NULL)
    return NULL;
  env_bcc->bcc_list = bcc_list;

  return env_bcc;
}

void mailimap_env_bcc_free(struct mailimap_env_bcc * env_bcc)
{
  mailimap_address_list_free(env_bcc->bcc_list);
  free(env_bcc);
}


struct mailimap_env_cc * mailimap_env_cc_new(clist * cc_list)
{
  struct mailimap_env_cc * env_cc;

  env_cc = malloc(sizeof(* env_cc));
  if (env_cc == NULL)
    return NULL;
  env_cc->cc_list = cc_list;

  return env_cc;
}

void mailimap_env_cc_free(struct mailimap_env_cc * env_cc)
{
  mailimap_address_list_free(env_cc->cc_list);
  free(env_cc);
}


void mailimap_env_date_free(char * date)
{
  mailimap_nstring_free(date);
}


struct mailimap_env_from * mailimap_env_from_new(clist * frm_list)
{
  struct mailimap_env_from * env_from;

  env_from = malloc(sizeof(* env_from));
  if (env_from == NULL)
    return NULL;
  env_from->frm_list = frm_list;

  return env_from;
}

void mailimap_env_from_free(struct mailimap_env_from * env_from)
{
  mailimap_address_list_free(env_from->frm_list);
  free(env_from);
}


void mailimap_env_in_reply_to_free(char * in_reply_to)
{
  mailimap_nstring_free(in_reply_to);
}

void mailimap_env_message_id_free(char * message_id)
{
  mailimap_nstring_free(message_id);
}

struct mailimap_env_reply_to * mailimap_env_reply_to_new(clist * rt_list)
{
  struct mailimap_env_reply_to * env_reply_to;

  env_reply_to = malloc(sizeof(* env_reply_to));
  if (env_reply_to == NULL)
    return NULL;
  env_reply_to->rt_list = rt_list;

  return env_reply_to;
}

void
mailimap_env_reply_to_free(struct mailimap_env_reply_to * env_reply_to)
{
  mailimap_address_list_free(env_reply_to->rt_list);
  free(env_reply_to);
}

struct mailimap_env_sender * mailimap_env_sender_new(clist * snd_list)
{
  struct mailimap_env_sender * env_sender;

  env_sender = malloc(sizeof(* env_sender));
  if (env_sender == NULL)
    return NULL;
  env_sender->snd_list = snd_list;

  return env_sender;
}

void mailimap_env_sender_free(struct mailimap_env_sender * env_sender)
{
  mailimap_address_list_free(env_sender->snd_list);
  free(env_sender);
}

void mailimap_env_subject_free(char * subject)
{
  mailimap_nstring_free(subject);
}

struct mailimap_env_to * mailimap_env_to_new(clist * to_list)
{
  struct mailimap_env_to * env_to;

  env_to = malloc(sizeof(* env_to));
  if (env_to == NULL)
    return NULL;
  env_to->to_list = to_list;

  return env_to;
}

void mailimap_env_to_free(struct mailimap_env_to * env_to)
{
  mailimap_address_list_free(env_to->to_list);
  free(env_to);
}



struct mailimap_flag * mailimap_flag_new(int fl_type,
    char * fl_keyword, char * fl_extension)
{
  struct mailimap_flag * f;

  f = malloc(sizeof(* f));
  if (f == NULL)
    return NULL;
  f->fl_type = fl_type;
  switch (fl_type) {
  case MAILIMAP_FLAG_KEYWORD:
    f->fl_data.fl_keyword = fl_keyword;
    break;
  case MAILIMAP_FLAG_EXTENSION:
    f->fl_data.fl_extension = fl_extension;
    break;
  }

  return f;
}

void mailimap_flag_free(struct mailimap_flag * f)
{
  switch (f->fl_type) {
  case MAILIMAP_FLAG_KEYWORD:
    mailimap_flag_keyword_free(f->fl_data.fl_keyword);
    break;
  case MAILIMAP_FLAG_EXTENSION:
    mailimap_flag_extension_free(f->fl_data.fl_extension);
    break;
  }
  free(f);
}



void mailimap_flag_extension_free(char * flag_extension)
{
  mailimap_atom_free(flag_extension);
}



struct mailimap_flag_fetch *
mailimap_flag_fetch_new(int fl_type, struct mailimap_flag * fl_flag)
{
  struct mailimap_flag_fetch * flag_fetch;

  flag_fetch = malloc(sizeof(* flag_fetch));
  if (flag_fetch == NULL)
    return NULL;

  flag_fetch->fl_type = fl_type;
  flag_fetch->fl_flag = fl_flag;

  return flag_fetch;
}

void mailimap_flag_fetch_free(struct mailimap_flag_fetch * flag_fetch)
{
  if (flag_fetch->fl_flag)
    mailimap_flag_free(flag_fetch->fl_flag);
  free(flag_fetch);
}



void mailimap_flag_keyword_free(char * flag_keyword)
{
  mailimap_atom_free(flag_keyword);
}




struct mailimap_flag_list *
mailimap_flag_list_new(clist * fl_list)
{
  struct mailimap_flag_list * flag_list;

  flag_list = malloc(sizeof(* flag_list));
  if (flag_list == NULL)
    return NULL;
  flag_list->fl_list = fl_list;

  return flag_list;
}

void mailimap_flag_list_free(struct mailimap_flag_list * flag_list)
{
  clist_foreach(flag_list->fl_list, (clist_func) mailimap_flag_free, NULL);
  clist_free(flag_list->fl_list);
  free(flag_list);
}





struct mailimap_flag_perm *
mailimap_flag_perm_new(int fl_type, struct mailimap_flag * fl_flag)
{
  struct mailimap_flag_perm * flag_perm;

  flag_perm = malloc(sizeof(* flag_perm));
  if (flag_perm == NULL)
    return NULL;

  flag_perm->fl_type = fl_type;
  flag_perm->fl_flag = fl_flag;

  return flag_perm;
}

void mailimap_flag_perm_free(struct mailimap_flag_perm * flag_perm)
{
  if (flag_perm->fl_flag != NULL)
    mailimap_flag_free(flag_perm->fl_flag);
  free(flag_perm);
}




struct mailimap_greeting *
mailimap_greeting_new(int gr_type,
    struct mailimap_resp_cond_auth * gr_auth,
    struct mailimap_resp_cond_bye * gr_bye)
{
  struct mailimap_greeting * greeting;

  greeting = malloc(sizeof(* greeting));
  if (greeting == NULL)
    return NULL;
  greeting->gr_type = gr_type;
  switch (gr_type) {
  case MAILIMAP_GREETING_RESP_COND_AUTH:
    greeting->gr_data.gr_auth = gr_auth;
    break;
  case MAILIMAP_GREETING_RESP_COND_BYE:
    greeting->gr_data.gr_bye = gr_bye;
    break;
  }
  
  return greeting;
}

void mailimap_greeting_free(struct mailimap_greeting * greeting)
{
  switch (greeting->gr_type) {
  case MAILIMAP_GREETING_RESP_COND_AUTH:
    mailimap_resp_cond_auth_free(greeting->gr_data.gr_auth);
    break;
  case MAILIMAP_GREETING_RESP_COND_BYE:
    mailimap_resp_cond_bye_free(greeting->gr_data.gr_bye);
    break;
  }
  free(greeting);
}



void
mailimap_header_fld_name_free(char * header_fld_name)
{
  mailimap_astring_free(header_fld_name);
}



struct mailimap_header_list *
mailimap_header_list_new(clist * hdr_list)
{
  struct mailimap_header_list * header_list;

  header_list = malloc(sizeof(* header_list));
  if (header_list == NULL)
    return NULL;

  header_list->hdr_list = hdr_list;

  return header_list;
}

void
mailimap_header_list_free(struct mailimap_header_list * header_list)
{
  clist_foreach(header_list->hdr_list,
      (clist_func) mailimap_header_fld_name_free,
      NULL);
  clist_free(header_list->hdr_list);
  free(header_list);
}



void mailimap_literal_free(char * literal)
{
  /*  free(literal); */
  mmap_string_unref(literal);
}

void mailimap_mailbox_free(char * mb)
{
  mailimap_astring_free(mb);
}




struct mailimap_status_info *
mailimap_status_info_new(int st_att, uint32_t st_value)
{
  struct mailimap_status_info * info;

  info = malloc(sizeof(* info));
  if (info == NULL)
    return NULL;
  info->st_att = st_att;
  info->st_value = st_value;

  return info;
}

void mailimap_status_info_free(struct mailimap_status_info * info)
{
  free(info);
}



struct mailimap_mailbox_data_status *
mailimap_mailbox_data_status_new(char * st_mailbox,
    clist * st_info_list)
{
  struct mailimap_mailbox_data_status * mb_data_status;

  mb_data_status = malloc(sizeof(* mb_data_status));
  if (mb_data_status == NULL)
    return NULL;
  mb_data_status->st_mailbox = st_mailbox;
  mb_data_status->st_info_list = st_info_list;

  return mb_data_status;
}

void
mailimap_mailbox_data_search_free(clist * data_search)
{
  clist_foreach(data_search, (clist_func) mailimap_number_alloc_free, NULL);
  clist_free(data_search);
}

void
mailimap_mailbox_data_status_free(struct mailimap_mailbox_data_status * info)
{
  mailimap_mailbox_free(info->st_mailbox);
  clist_foreach(info->st_info_list, (clist_func) mailimap_status_info_free,
		 NULL);
  clist_free(info->st_info_list);
  free(info);
}


static void
mailimap_mailbox_data_flags_free(struct mailimap_flag_list * flag_list)
{
  mailimap_flag_list_free(flag_list);
}

static void
mailimap_mailbox_data_list_free(struct mailimap_mailbox_list * mb_list)
{
  mailimap_mailbox_list_free(mb_list);
}

static void
mailimap_mailbox_data_lsub_free(struct mailimap_mailbox_list * mb_lsub)
{
  mailimap_mailbox_list_free(mb_lsub);
}






struct mailimap_mailbox_data *
mailimap_mailbox_data_new(int mbd_type, struct mailimap_flag_list * mbd_flags,
    struct mailimap_mailbox_list * mbd_list,
    struct mailimap_mailbox_list * mbd_lsub,
    clist * mbd_search,
    struct mailimap_mailbox_data_status * mbd_status,
    uint32_t mbd_exists,
    uint32_t mbd_recent,
    struct mailimap_extension_data * mbd_extension)
{
  struct mailimap_mailbox_data * data;

  data = malloc(sizeof(* data));
  if (data == NULL)
    return NULL;

  data->mbd_type = mbd_type;
  switch (mbd_type) {
  case MAILIMAP_MAILBOX_DATA_FLAGS:
    data->mbd_data.mbd_flags = mbd_flags;
    break;
  case MAILIMAP_MAILBOX_DATA_LIST:
    data->mbd_data.mbd_list = mbd_list;
    break;
  case MAILIMAP_MAILBOX_DATA_LSUB:
    data->mbd_data.mbd_lsub = mbd_lsub;
    break;
  case MAILIMAP_MAILBOX_DATA_SEARCH:
    data->mbd_data.mbd_search = mbd_search;
    break;
  case MAILIMAP_MAILBOX_DATA_STATUS:
    data->mbd_data.mbd_status = mbd_status;
    break;
  case MAILIMAP_MAILBOX_DATA_EXISTS:
    data->mbd_data.mbd_exists = mbd_exists;
    break;
  case MAILIMAP_MAILBOX_DATA_RECENT:
    data->mbd_data.mbd_recent = mbd_recent;
    break;
  case MAILIMAP_MAILBOX_DATA_EXTENSION_DATA:
    data->mbd_data.mbd_extension = mbd_extension;
    break;
  }
  
  return data;
}

void
mailimap_mailbox_data_free(struct mailimap_mailbox_data * mb_data)
{
  switch (mb_data->mbd_type) {
  case MAILIMAP_MAILBOX_DATA_FLAGS:
    if (mb_data->mbd_data.mbd_flags != NULL)
      mailimap_mailbox_data_flags_free(mb_data->mbd_data.mbd_flags);
    break;
  case MAILIMAP_MAILBOX_DATA_LIST:
    if (mb_data->mbd_data.mbd_list != NULL)
      mailimap_mailbox_data_list_free(mb_data->mbd_data.mbd_list);
    break;
  case MAILIMAP_MAILBOX_DATA_LSUB:
    if (mb_data->mbd_data.mbd_lsub != NULL)
      mailimap_mailbox_data_lsub_free(mb_data->mbd_data.mbd_lsub);
    break;
  case MAILIMAP_MAILBOX_DATA_SEARCH:
    if (mb_data->mbd_data.mbd_search != NULL)
      mailimap_mailbox_data_search_free(mb_data->mbd_data.mbd_search);
    break;
  case MAILIMAP_MAILBOX_DATA_STATUS:
    if (mb_data->mbd_data.mbd_status != NULL)
      mailimap_mailbox_data_status_free(mb_data->mbd_data.mbd_status);
    break;
  case MAILIMAP_MAILBOX_DATA_EXTENSION_DATA:
    if (mb_data->mbd_data.mbd_extension != NULL)
      mailimap_extension_data_free(mb_data->mbd_data.mbd_extension);
    break;
  }
  free(mb_data);
}





struct mailimap_mbx_list_flags *
mailimap_mbx_list_flags_new(int mbf_type, clist * mbf_oflags,
			    int mbf_sflag)
{
  struct mailimap_mbx_list_flags * mbx_list_flags;

  mbx_list_flags = malloc(sizeof(* mbx_list_flags));
  if (mbx_list_flags == NULL)
    return NULL;

  mbx_list_flags->mbf_type = mbf_type;
  mbx_list_flags->mbf_oflags = mbf_oflags;
  mbx_list_flags->mbf_sflag = mbf_sflag;
  
  return mbx_list_flags;
}

void
mailimap_mbx_list_flags_free(struct mailimap_mbx_list_flags * mbx_list_flags)
{
  clist_foreach(mbx_list_flags->mbf_oflags,
      (clist_func) mailimap_mbx_list_oflag_free,
      NULL);
  clist_free(mbx_list_flags->mbf_oflags);
  
  free(mbx_list_flags);
}


struct mailimap_mbx_list_oflag *
mailimap_mbx_list_oflag_new(int of_type, char * of_flag_ext)
{
  struct mailimap_mbx_list_oflag * oflag;

  oflag = malloc(sizeof(* oflag));
  if (oflag == NULL)
    return NULL;

  oflag->of_type = of_type;
  oflag->of_flag_ext = of_flag_ext;

  return oflag;
}

void
mailimap_mbx_list_oflag_free(struct mailimap_mbx_list_oflag * oflag)
{
  if (oflag->of_flag_ext != NULL)
    mailimap_flag_extension_free(oflag->of_flag_ext);
  free(oflag);
}



struct mailimap_mailbox_list *
mailimap_mailbox_list_new(struct mailimap_mbx_list_flags * mbx_flags,
			  char mb_delimiter, char * mb_name)
{
  struct mailimap_mailbox_list * mb_list;

  mb_list = malloc(sizeof(* mb_list));
  if (mb_list == NULL)
    return NULL;
  
  mb_list->mb_flag = mbx_flags;
  mb_list->mb_delimiter = mb_delimiter;
  mb_list->mb_name = mb_name;

  return mb_list;
}

void
mailimap_mailbox_list_free(struct mailimap_mailbox_list * mb_list)
{
  if (mb_list->mb_flag != NULL)
    mailimap_mbx_list_flags_free(mb_list->mb_flag);
  if (mb_list->mb_name != NULL)
    mailimap_mailbox_free(mb_list->mb_name);
  free(mb_list);
}



struct mailimap_media_basic *
mailimap_media_basic_new(int med_type,
    char * med_basic_type, char * med_subtype)
{
  struct mailimap_media_basic * media_basic;

  media_basic = malloc(sizeof(* media_basic));
  if (media_basic == NULL)
    return NULL;
  media_basic->med_type = med_type;
  media_basic->med_basic_type = med_basic_type;
  media_basic->med_subtype = med_subtype;

  return media_basic;
}

void
mailimap_media_basic_free(struct mailimap_media_basic * media_basic)
{
  mailimap_string_free(media_basic->med_basic_type);
  mailimap_media_subtype_free(media_basic->med_subtype);
  free(media_basic);
}



void mailimap_media_subtype_free(char * media_subtype)
{
  mmap_string_unref(media_subtype);
}


void mailimap_media_text_free(char * media_text)
{
  mailimap_media_subtype_free(media_text);
}



struct mailimap_message_data *
mailimap_message_data_new(uint32_t mdt_number, int mdt_type,
			  struct mailimap_msg_att * mdt_msg_att)
{
  struct mailimap_message_data * msg_data;

  msg_data = malloc(sizeof(* msg_data));
  if (msg_data == NULL)
    free(msg_data);

  msg_data->mdt_number = mdt_number;
  msg_data->mdt_type = mdt_type;
  msg_data->mdt_msg_att = mdt_msg_att;

  return msg_data;
}

void
mailimap_message_data_free(struct mailimap_message_data * msg_data)
{
  if (msg_data->mdt_msg_att != NULL)
    mailimap_msg_att_free(msg_data->mdt_msg_att);
  free(msg_data);
}




struct mailimap_msg_att_item *
mailimap_msg_att_item_new(int att_type,
			  struct mailimap_msg_att_dynamic * att_dyn,
			  struct mailimap_msg_att_static * att_static)
{
  struct mailimap_msg_att_item * item;

  item = malloc(sizeof(* item));
  if (item == NULL)
    return item;

  item->att_type = att_type;
  switch (att_type) {
  case MAILIMAP_MSG_ATT_ITEM_DYNAMIC:
    item->att_data.att_dyn = att_dyn;
    break;
  case MAILIMAP_MSG_ATT_ITEM_STATIC:
    item->att_data.att_static = att_static;
    break;
  }

  return item;
}

void
mailimap_msg_att_item_free(struct mailimap_msg_att_item * item)
{
  switch (item->att_type) {
  case MAILIMAP_MSG_ATT_ITEM_DYNAMIC:
    mailimap_msg_att_dynamic_free(item->att_data.att_dyn);
    break;
  case MAILIMAP_MSG_ATT_ITEM_STATIC:
    mailimap_msg_att_static_free(item->att_data.att_static);
    break;
  }
  free(item);
}


struct mailimap_msg_att *
mailimap_msg_att_new(clist * att_list)
{
  struct mailimap_msg_att * msg_att;

  msg_att = malloc(sizeof(* msg_att));
  if (msg_att == NULL)
    return NULL;

  msg_att->att_list = att_list;
  msg_att->att_number = 0;

  return msg_att;
}

void mailimap_msg_att_free(struct mailimap_msg_att * msg_att)
{
  clist_foreach(msg_att->att_list,
      (clist_func) mailimap_msg_att_item_free, NULL);
  clist_free(msg_att->att_list);
  free(msg_att);
}



struct mailimap_msg_att_dynamic *
mailimap_msg_att_dynamic_new(clist * att_list)
{
  struct mailimap_msg_att_dynamic * msg_att_dyn;

  msg_att_dyn = malloc(sizeof(* msg_att_dyn));
  if (msg_att_dyn == NULL)
    return NULL;

  msg_att_dyn->att_list = att_list;
  
  return msg_att_dyn;
}

void
mailimap_msg_att_dynamic_free(struct mailimap_msg_att_dynamic * msg_att_dyn)
{
  if (msg_att_dyn->att_list != NULL) {
    clist_foreach(msg_att_dyn->att_list,
        (clist_func) mailimap_flag_fetch_free,
        NULL);
    clist_free(msg_att_dyn->att_list);
  }
  free(msg_att_dyn);
}


struct mailimap_msg_att_body_section *
mailimap_msg_att_body_section_new(struct mailimap_section * sec_section,
				  uint32_t sec_origin_octet,
				  char * sec_body_part,
				  size_t sec_length)
{
  struct mailimap_msg_att_body_section * msg_att_body_section;

  msg_att_body_section = malloc(sizeof(* msg_att_body_section));
  if (msg_att_body_section == NULL)
    return NULL;

  msg_att_body_section->sec_section = sec_section;
  msg_att_body_section->sec_origin_octet = sec_origin_octet;
  msg_att_body_section->sec_body_part = sec_body_part;
  msg_att_body_section->sec_length = sec_length;

  return msg_att_body_section;
}

void
mailimap_msg_att_body_section_free(struct mailimap_msg_att_body_section * 
    msg_att_body_section)
{
  if (msg_att_body_section->sec_section != NULL)
    mailimap_section_free(msg_att_body_section->sec_section);
  if (msg_att_body_section->sec_body_part != NULL)
    mailimap_nstring_free(msg_att_body_section->sec_body_part);
  free(msg_att_body_section);
}






void mailimap_msg_att_envelope_free(struct mailimap_envelope * env)
{
  mailimap_envelope_free(env);
}

void
mailimap_msg_att_internaldate_free(struct mailimap_date_time * date_time)
{
  mailimap_date_time_free(date_time);
}

void
mailimap_msg_att_rfc822_free(char * str)
{
  mailimap_nstring_free(str);
}


void
mailimap_msg_att_rfc822_header_free(char * str)
{
  mailimap_nstring_free(str);
}

void
mailimap_msg_att_rfc822_text_free(char * str)
{
  mailimap_nstring_free(str);
}

void
mailimap_msg_att_body_free(struct mailimap_body * body)
{
  mailimap_body_free(body);
}

void
mailimap_msg_att_bodystructure_free(struct mailimap_body * body)
{
  mailimap_body_free(body);
}



struct mailimap_msg_att_static *
mailimap_msg_att_static_new(int att_type, struct mailimap_envelope * att_env,
    struct mailimap_date_time * att_internal_date,
    char * att_rfc822,
    char * att_rfc822_header,
    char * att_rfc822_text,
    size_t att_length,
    uint32_t att_rfc822_size,
    struct mailimap_body * att_bodystructure,
    struct mailimap_body * att_body,
    struct mailimap_msg_att_body_section * att_body_section,
    uint32_t att_uid)
{
  struct mailimap_msg_att_static * item;

  item = malloc(sizeof(* item));
  if (item == NULL)
    return FALSE;

  item->att_type = att_type;
  switch (att_type) {
  case MAILIMAP_MSG_ATT_ENVELOPE:
    item->att_data.att_env = att_env;
    break;
  case MAILIMAP_MSG_ATT_INTERNALDATE:
    item->att_data.att_internal_date = att_internal_date;
    break;
  case MAILIMAP_MSG_ATT_RFC822:
    item->att_data.att_rfc822.att_content = att_rfc822;
    item->att_data.att_rfc822.att_length = att_length;
    break;
  case MAILIMAP_MSG_ATT_RFC822_HEADER:
    item->att_data.att_rfc822_header.att_content = att_rfc822_header;
    item->att_data.att_rfc822_header.att_length = att_length;
    break;
  case MAILIMAP_MSG_ATT_RFC822_TEXT:
    item->att_data.att_rfc822_text.att_content = att_rfc822_text;
    item->att_data.att_rfc822_text.att_length = att_length;
    break;
  case MAILIMAP_MSG_ATT_RFC822_SIZE:
    item->att_data.att_rfc822_size = att_rfc822_size;
    break;
  case MAILIMAP_MSG_ATT_BODY:
    item->att_data.att_body = att_body;
    break;
  case MAILIMAP_MSG_ATT_BODYSTRUCTURE:
    item->att_data.att_bodystructure = att_bodystructure;
    break;
  case MAILIMAP_MSG_ATT_BODY_SECTION:
    item->att_data.att_body_section = att_body_section;
    break;
  case MAILIMAP_MSG_ATT_UID:
    item->att_data.att_uid = att_uid;
    break;
  }

  return item;
}

void
mailimap_msg_att_static_free(struct mailimap_msg_att_static * item)
{
  switch (item->att_type) {
  case MAILIMAP_MSG_ATT_ENVELOPE:
    if (item->att_data.att_env != NULL)
      mailimap_msg_att_envelope_free(item->att_data.att_env);
    break;
  case MAILIMAP_MSG_ATT_INTERNALDATE:
    if (item->att_data.att_internal_date != NULL)
      mailimap_msg_att_internaldate_free(item->att_data.att_internal_date);
    break;
  case MAILIMAP_MSG_ATT_RFC822:
    if (item->att_data.att_rfc822.att_content != NULL)
      mailimap_msg_att_rfc822_free(item->att_data.att_rfc822.att_content);
    break;
  case MAILIMAP_MSG_ATT_RFC822_HEADER:
    if (item->att_data.att_rfc822_header.att_content != NULL)
      mailimap_msg_att_rfc822_header_free(item->att_data.att_rfc822_header.att_content);
    break;
  case MAILIMAP_MSG_ATT_RFC822_TEXT:
    if (item->att_data.att_rfc822_text.att_content != NULL)
      mailimap_msg_att_rfc822_text_free(item->att_data.att_rfc822_text.att_content);
    break;
  case MAILIMAP_MSG_ATT_BODYSTRUCTURE:
    if (item->att_data.att_bodystructure != NULL)
      mailimap_msg_att_bodystructure_free(item->att_data.att_bodystructure);
    break;
  case MAILIMAP_MSG_ATT_BODY:
    if (item->att_data.att_body != NULL)
      mailimap_msg_att_body_free(item->att_data.att_body);
    break;
  case MAILIMAP_MSG_ATT_BODY_SECTION:
    if (item->att_data.att_body_section != NULL)
      mailimap_msg_att_body_section_free(item->att_data.att_body_section);
    break;
  }
  free(item);
}
 



void mailimap_nstring_free(char * str)
{
  if (str != NULL)
    mailimap_string_free(str);
}







struct mailimap_cont_req_or_resp_data *
mailimap_cont_req_or_resp_data_new(int rsp_type,
    struct mailimap_continue_req * rsp_cont_req,
    struct mailimap_response_data * rsp_resp_data)
{
  struct mailimap_cont_req_or_resp_data * cont_req_or_resp_data;

  cont_req_or_resp_data = malloc(sizeof(* cont_req_or_resp_data));
  if (cont_req_or_resp_data == NULL)
    return NULL;

  cont_req_or_resp_data->rsp_type = rsp_type;
  switch (rsp_type) {
  case MAILIMAP_RESP_CONT_REQ:
    cont_req_or_resp_data->rsp_data.rsp_cont_req = rsp_cont_req;
    break;
  case MAILIMAP_RESP_RESP_DATA:
    cont_req_or_resp_data->rsp_data.rsp_resp_data = rsp_resp_data;
    break;
  }
  
  return cont_req_or_resp_data;
}

void
mailimap_cont_req_or_resp_data_free(struct mailimap_cont_req_or_resp_data *
				    cont_req_or_resp_data)
{
  switch (cont_req_or_resp_data->rsp_type) {
  case MAILIMAP_RESP_CONT_REQ:
    if (cont_req_or_resp_data->rsp_data.rsp_cont_req != NULL)
      mailimap_continue_req_free(cont_req_or_resp_data->rsp_data.rsp_cont_req);
    break;
  case MAILIMAP_RESP_RESP_DATA:
    if (cont_req_or_resp_data->rsp_data.rsp_resp_data != NULL)
      mailimap_response_data_free(cont_req_or_resp_data->rsp_data.rsp_resp_data);
    break;
  }
  free(cont_req_or_resp_data);
}




struct mailimap_response *
mailimap_response_new(clist * rsp_cont_req_or_resp_data_list,
    struct mailimap_response_done * rsp_resp_done)
{
  struct mailimap_response * resp;

  resp = malloc(sizeof(* resp));
  if (resp == NULL)
    return NULL;

  resp->rsp_cont_req_or_resp_data_list = rsp_cont_req_or_resp_data_list;
  resp->rsp_resp_done = rsp_resp_done;

  return resp;
}

void
mailimap_response_free(struct mailimap_response * resp)
{
  if (resp->rsp_cont_req_or_resp_data_list != NULL) {
    clist_foreach(resp->rsp_cont_req_or_resp_data_list,
        (clist_func) mailimap_cont_req_or_resp_data_free, NULL);
    clist_free(resp->rsp_cont_req_or_resp_data_list);
  }
  mailimap_response_done_free(resp->rsp_resp_done);
  free(resp);
}



struct mailimap_response_data *
mailimap_response_data_new(int rsp_type,
    struct mailimap_resp_cond_state * rsp_cond_state,
    struct mailimap_resp_cond_bye * rsp_bye,
    struct mailimap_mailbox_data * rsp_mailbox_data,
    struct mailimap_message_data * rsp_message_data,
    struct mailimap_capability_data * rsp_capability_data,
    struct mailimap_extension_data * rsp_extension_data)
{
  struct mailimap_response_data * resp_data;

  resp_data = malloc(sizeof(* resp_data));
  if (resp_data == NULL)
    return NULL;
  resp_data->rsp_type = rsp_type;

  switch (rsp_type) {
  case MAILIMAP_RESP_DATA_TYPE_COND_STATE:
    resp_data->rsp_data.rsp_cond_state = rsp_cond_state;
    break;
  case MAILIMAP_RESP_DATA_TYPE_COND_BYE:
    resp_data->rsp_data.rsp_bye = rsp_bye;
    break;
  case MAILIMAP_RESP_DATA_TYPE_MAILBOX_DATA:
    resp_data->rsp_data.rsp_mailbox_data = rsp_mailbox_data;
    break;
  case MAILIMAP_RESP_DATA_TYPE_MESSAGE_DATA:
    resp_data->rsp_data.rsp_message_data = rsp_message_data;
    break;
  case MAILIMAP_RESP_DATA_TYPE_CAPABILITY_DATA:
    resp_data->rsp_data.rsp_capability_data = rsp_capability_data;
    break;
  case MAILIMAP_RESP_DATA_TYPE_EXTENSION_DATA:
    resp_data->rsp_data.rsp_extension_data = rsp_extension_data;
    break;
  }
  
  return resp_data;
}

void
mailimap_response_data_free(struct mailimap_response_data * resp_data)
{
  switch (resp_data->rsp_type) {
  case MAILIMAP_RESP_DATA_TYPE_COND_STATE:
    if (resp_data->rsp_data.rsp_cond_state != NULL)
      mailimap_resp_cond_state_free(resp_data->rsp_data.rsp_cond_state);
    break;
  case MAILIMAP_RESP_DATA_TYPE_COND_BYE:
    if (resp_data->rsp_data.rsp_bye != NULL)
      mailimap_resp_cond_bye_free(resp_data->rsp_data.rsp_bye);
    break;
  case MAILIMAP_RESP_DATA_TYPE_MAILBOX_DATA:
    if (resp_data->rsp_data.rsp_mailbox_data != NULL)
      mailimap_mailbox_data_free(resp_data->rsp_data.rsp_mailbox_data);
    break;
  case MAILIMAP_RESP_DATA_TYPE_MESSAGE_DATA:
    if (resp_data->rsp_data.rsp_message_data != NULL)
      mailimap_message_data_free(resp_data->rsp_data.rsp_message_data);
    break;
  case MAILIMAP_RESP_DATA_TYPE_CAPABILITY_DATA:
    if (resp_data->rsp_data.rsp_capability_data != NULL)
      mailimap_capability_data_free(resp_data->rsp_data.rsp_capability_data);
    break;
  case MAILIMAP_RESP_DATA_TYPE_EXTENSION_DATA:
    if (resp_data->rsp_data.rsp_extension_data != NULL)
      mailimap_extension_data_free(resp_data->rsp_data.rsp_extension_data);
    break;
  }
  free(resp_data);
}



struct mailimap_response_done *
mailimap_response_done_new(int rsp_type,
    struct mailimap_response_tagged * rsp_tagged,
    struct mailimap_response_fatal * rsp_fatal)
{
  struct mailimap_response_done * resp_done;
    
  resp_done = malloc(sizeof(* resp_done));
  if (resp_done == NULL)
    return NULL;

  resp_done->rsp_type = rsp_type;
  switch (rsp_type) {
  case MAILIMAP_RESP_DONE_TYPE_TAGGED:
    resp_done->rsp_data.rsp_tagged = rsp_tagged;
    break;
  case MAILIMAP_RESP_DONE_TYPE_FATAL:
    resp_done->rsp_data.rsp_fatal = rsp_fatal;
    break;
  }

  return resp_done;
}

void mailimap_response_done_free(struct mailimap_response_done *
				 resp_done)
{
  switch (resp_done->rsp_type) {
  case MAILIMAP_RESP_DONE_TYPE_TAGGED:
    mailimap_response_tagged_free(resp_done->rsp_data.rsp_tagged);
    break;
  case MAILIMAP_RESP_DONE_TYPE_FATAL:
    mailimap_response_fatal_free(resp_done->rsp_data.rsp_fatal);
    break;
  }
  free(resp_done);
}

struct mailimap_response_fatal *
mailimap_response_fatal_new(struct mailimap_resp_cond_bye * rsp_bye)
{
  struct mailimap_response_fatal * resp_fatal;

  resp_fatal = malloc(sizeof(* resp_fatal));
  if (resp_fatal == NULL)
    return NULL;

  resp_fatal->rsp_bye = rsp_bye;

  return NULL;
}

void mailimap_response_fatal_free(struct mailimap_response_fatal * resp_fatal)
{
  mailimap_resp_cond_bye_free(resp_fatal->rsp_bye);
  free(resp_fatal);
}

struct mailimap_response_tagged *
mailimap_response_tagged_new(char * rsp_tag,
    struct mailimap_resp_cond_state * rsp_cond_state)
{
  struct mailimap_response_tagged * resp_tagged;

  resp_tagged = malloc(sizeof(* resp_tagged));
  if (resp_tagged == NULL)
    return NULL;

  resp_tagged->rsp_tag = rsp_tag;
  resp_tagged->rsp_cond_state = rsp_cond_state;

  return resp_tagged;
}

void
mailimap_response_tagged_free(struct mailimap_response_tagged * tagged)
{
  mailimap_tag_free(tagged->rsp_tag);
  mailimap_resp_cond_state_free(tagged->rsp_cond_state);
  free(tagged);
}



struct mailimap_resp_cond_auth *
mailimap_resp_cond_auth_new(int rsp_type,
    struct mailimap_resp_text * rsp_text)
{
  struct mailimap_resp_cond_auth * cond_auth;

  cond_auth = malloc(sizeof(* cond_auth));
  if (cond_auth == NULL)
    return NULL;

  cond_auth->rsp_type = rsp_type;
  cond_auth->rsp_text = rsp_text;

  return cond_auth;
}

void
mailimap_resp_cond_auth_free(struct mailimap_resp_cond_auth * cond_auth)
{
  mailimap_resp_text_free(cond_auth->rsp_text);
  free(cond_auth);
}



struct mailimap_resp_cond_bye *
mailimap_resp_cond_bye_new(struct mailimap_resp_text * rsp_text)
{
  struct mailimap_resp_cond_bye * cond_bye;

  cond_bye = malloc(sizeof(* cond_bye));
  if (cond_bye == NULL)
    return NULL;

  cond_bye->rsp_text = rsp_text;

  return cond_bye;
}


void
mailimap_resp_cond_bye_free(struct mailimap_resp_cond_bye * cond_bye)
{
  mailimap_resp_text_free(cond_bye->rsp_text);
  free(cond_bye);
}


struct mailimap_resp_cond_state *
mailimap_resp_cond_state_new(int rsp_type,
    struct mailimap_resp_text * rsp_text)
{
  struct mailimap_resp_cond_state * cond_state;

  cond_state = malloc(sizeof(* cond_state));
  if (cond_state == NULL)
    return NULL;

  cond_state->rsp_type = rsp_type;
  cond_state->rsp_text = rsp_text;

  return cond_state;
}

void
mailimap_resp_cond_state_free(struct mailimap_resp_cond_state * cond_state)
{
  mailimap_resp_text_free(cond_state->rsp_text);
  free(cond_state);
}


struct mailimap_resp_text *
mailimap_resp_text_new(struct mailimap_resp_text_code * rsp_code,
    char * rsp_text)
{
  struct mailimap_resp_text * resp_text;

  resp_text = malloc(sizeof(* resp_text));
  if (resp_text == NULL)
    return NULL;

  resp_text->rsp_code = rsp_code;
  resp_text->rsp_text = rsp_text;

  return resp_text;
}

void mailimap_resp_text_free(struct mailimap_resp_text * resp_text)
{
  if (resp_text->rsp_code)
    mailimap_resp_text_code_free(resp_text->rsp_code);
  if (resp_text->rsp_text)
    mailimap_text_free(resp_text->rsp_text);
  free(resp_text);
}




struct mailimap_resp_text_code *
mailimap_resp_text_code_new(int rc_type, clist * rc_badcharset,
    struct mailimap_capability_data * rc_cap_data,
    clist * rc_perm_flags,
    uint32_t rc_uidnext, uint32_t rc_uidvalidity,
    uint32_t rc_first_unseen, char * rc_atom, char * rc_atom_value,
    struct mailimap_extension_data * rc_ext_data)
{
  struct mailimap_resp_text_code * resp_text_code;

  resp_text_code = malloc(sizeof(* resp_text_code));
  if (resp_text_code == NULL)
    return NULL;

  resp_text_code->rc_type = rc_type;
  switch (rc_type) {
  case MAILIMAP_RESP_TEXT_CODE_BADCHARSET:
    resp_text_code->rc_data.rc_badcharset = rc_badcharset;
    break;
  case MAILIMAP_RESP_TEXT_CODE_CAPABILITY_DATA:
    resp_text_code->rc_data.rc_cap_data = rc_cap_data;
    break;
  case MAILIMAP_RESP_TEXT_CODE_PERMANENTFLAGS:
    resp_text_code->rc_data.rc_perm_flags = rc_perm_flags;
    break;
  case MAILIMAP_RESP_TEXT_CODE_UIDNEXT:
    resp_text_code->rc_data.rc_uidnext = rc_uidnext;
    break;
  case MAILIMAP_RESP_TEXT_CODE_UIDVALIDITY:
    resp_text_code->rc_data.rc_uidvalidity = rc_uidvalidity;
    break;
  case MAILIMAP_RESP_TEXT_CODE_UNSEEN:
    resp_text_code->rc_data.rc_first_unseen = rc_first_unseen;
    break;
  case MAILIMAP_RESP_TEXT_CODE_OTHER:
    resp_text_code->rc_data.rc_atom.atom_name = rc_atom;
    resp_text_code->rc_data.rc_atom.atom_value = rc_atom_value;
    break;
  case MAILIMAP_RESP_TEXT_CODE_EXTENSION:
    resp_text_code->rc_data.rc_ext_data = rc_ext_data;
    break;
  }

  return resp_text_code;
}

void
mailimap_resp_text_code_free(struct mailimap_resp_text_code * resp_text_code)
{
  switch (resp_text_code->rc_type) {
  case MAILIMAP_RESP_TEXT_CODE_BADCHARSET:
    if (resp_text_code->rc_data.rc_badcharset != NULL) {
      clist_foreach(resp_text_code->rc_data.rc_badcharset,
          (clist_func) mailimap_astring_free,
          NULL);
      clist_free(resp_text_code->rc_data.rc_badcharset);
    }
    break;
  case MAILIMAP_RESP_TEXT_CODE_CAPABILITY_DATA:
    if (resp_text_code->rc_data.rc_cap_data != NULL)
      mailimap_capability_data_free(resp_text_code->rc_data.rc_cap_data);
    break;
  case MAILIMAP_RESP_TEXT_CODE_PERMANENTFLAGS:
    if (resp_text_code->rc_data.rc_perm_flags != NULL) {
      clist_foreach(resp_text_code->rc_data.rc_perm_flags,
          (clist_func) mailimap_flag_perm_free, NULL);
      clist_free(resp_text_code->rc_data.rc_perm_flags);
    }
    break;
  case MAILIMAP_RESP_TEXT_CODE_OTHER:
    if (resp_text_code->rc_data.rc_atom.atom_name != NULL)
      mailimap_atom_free(resp_text_code->rc_data.rc_atom.atom_name);
    if (resp_text_code->rc_data.rc_atom.atom_value != NULL)
      mailimap_custom_string_free(resp_text_code->rc_data.rc_atom.atom_value);
    break;
  case MAILIMAP_RESP_TEXT_CODE_EXTENSION:
    if (resp_text_code->rc_data.rc_ext_data != NULL)
      mailimap_extension_data_free(resp_text_code->rc_data.rc_ext_data);
    break;
  }
  free(resp_text_code);
}


struct mailimap_section *
mailimap_section_new(struct mailimap_section_spec * sec_spec)
{
  struct mailimap_section * section;

  section = malloc(sizeof(* section));
  if (section == NULL)
    return NULL;
  
  section->sec_spec = sec_spec;

  return section;
}

void mailimap_section_free(struct mailimap_section * section)
{
  if (section->sec_spec != NULL)
    mailimap_section_spec_free(section->sec_spec);
  free(section);
}



struct mailimap_section_msgtext *
mailimap_section_msgtext_new(int sec_type,
    struct mailimap_header_list * sec_header_list)
{
  struct mailimap_section_msgtext * msgtext;

  msgtext = malloc(sizeof(* msgtext));
  if (msgtext == NULL)
    return FALSE;

  msgtext->sec_type = sec_type;
  msgtext->sec_header_list = sec_header_list;

  return msgtext;
}

void
mailimap_section_msgtext_free(struct mailimap_section_msgtext * msgtext)
{
  if (msgtext->sec_header_list != NULL)
    mailimap_header_list_free(msgtext->sec_header_list);
  free(msgtext);
}


struct mailimap_section_part *
mailimap_section_part_new(clist * sec_id)
{
  struct mailimap_section_part * section_part;

  section_part = malloc(sizeof(* section_part));
  if (section_part == NULL)
    return NULL;

  section_part->sec_id = sec_id;

  return section_part;
}

void
mailimap_section_part_free(struct mailimap_section_part * section_part)
{
  clist_foreach(section_part->sec_id,
      (clist_func) mailimap_number_alloc_free, NULL);
  clist_free(section_part->sec_id);
  free(section_part);
}


struct mailimap_section_spec *
mailimap_section_spec_new(int sec_type,
    struct mailimap_section_msgtext * sec_msgtext,
    struct mailimap_section_part * sec_part,
    struct mailimap_section_text * sec_text)
{
  struct mailimap_section_spec * section_spec;

  section_spec = malloc(sizeof(* section_spec));
  if (section_spec == NULL)
    return NULL;

  section_spec->sec_type = sec_type;
  switch (sec_type) {
  case MAILIMAP_SECTION_SPEC_SECTION_MSGTEXT:
    section_spec->sec_data.sec_msgtext = sec_msgtext;
    break;
  case MAILIMAP_SECTION_SPEC_SECTION_PART:
    section_spec->sec_data.sec_part = sec_part;
    break;
  }
  section_spec->sec_text = sec_text;

  return section_spec;
}

void
mailimap_section_spec_free(struct mailimap_section_spec * section_spec)
{
  if (section_spec->sec_text)
    mailimap_section_text_free(section_spec->sec_text);
  
  switch (section_spec->sec_type) {
  case MAILIMAP_SECTION_SPEC_SECTION_PART:
    if (section_spec->sec_data.sec_part != NULL)
      mailimap_section_part_free(section_spec->sec_data.sec_part);
    break;
  case MAILIMAP_SECTION_SPEC_SECTION_MSGTEXT:
    /* handle case where it can be detached */
    if (section_spec->sec_data.sec_msgtext != NULL)
      mailimap_section_msgtext_free(section_spec->sec_data.sec_msgtext);
    break;
  }
  free(section_spec);
}


struct mailimap_section_text *
mailimap_section_text_new(int sec_type,
    struct mailimap_section_msgtext * sec_msgtext)
{
  struct mailimap_section_text * section_text;
  
  section_text = malloc(sizeof(* section_text));
  if (section_text == NULL)
    return NULL;

  section_text->sec_type = sec_type;
  section_text->sec_msgtext = sec_msgtext;
  
  return section_text;
}

void
mailimap_section_text_free(struct mailimap_section_text * section_text)
{
  if (section_text->sec_msgtext != NULL)
    mailimap_section_msgtext_free(section_text->sec_msgtext);
  free(section_text);
}




void
mailimap_string_free(char * str)
{
  mmap_string_unref(str);
}





void mailimap_tag_free(char * tag)
{
  mailimap_custom_string_free(tag);
}


void mailimap_text_free(char * text)
{
  mailimap_custom_string_free(text);
}






















/* ************************************************************************* */
/* ************************************************************************* */
/* ************************************************************************* */
/* ************************************************************************* */
/* ************************************************************************* */
/* ************************************************************************* */






/* sender only */


/* COPY FETCH SEARCH STORE */
/* set */

struct mailimap_set_item *
mailimap_set_item_new(uint32_t set_first, uint32_t set_last)
{
  struct mailimap_set_item * item;

  item = malloc(sizeof(* item));
  if (item == NULL)
    return NULL;

  item->set_first = set_first;
  item->set_last = set_last;

  return item;
}

void mailimap_set_item_free(struct mailimap_set_item * set_item)
{
  free(set_item);
}

struct mailimap_set * mailimap_set_new(clist * set_list)
{
  struct mailimap_set * set;

  set = malloc(sizeof(* set));
  if (set == NULL)
    return NULL;

  set->set_list = set_list;

  return set;
}

void mailimap_set_free(struct mailimap_set * set)
{
  clist_foreach(set->set_list, (clist_func) mailimap_set_item_free, NULL);
  clist_free(set->set_list);
  free(set);
}

/* SEARCH with date key */
/* date */

struct mailimap_date *
mailimap_date_new(int dt_day, int dt_month, int dt_year)
{
  struct mailimap_date * date;

  date = malloc(sizeof(* date));
  if (date == NULL)
    return NULL;

  date->dt_day = dt_day;
  date->dt_month = dt_month;
  date->dt_year = dt_year;

  return date;
}

void mailimap_date_free(struct mailimap_date * date)
{
  free(date);
}



struct mailimap_fetch_att *
mailimap_fetch_att_new(int att_type, struct mailimap_section * att_section,
    uint32_t att_offset, uint32_t att_size)
{
  struct mailimap_fetch_att * fetch_att;

  fetch_att = malloc(sizeof(* fetch_att));
  if (fetch_att == NULL)
    return NULL;
  fetch_att->att_type = att_type;
  fetch_att->att_section = att_section;
  fetch_att->att_offset = att_offset;
  fetch_att->att_size = att_size;

  return fetch_att;
}

void mailimap_fetch_att_free(struct mailimap_fetch_att * fetch_att)
{
  if (fetch_att->att_section != NULL)
    mailimap_section_free(fetch_att->att_section);
  free(fetch_att);
}



struct mailimap_fetch_type *
mailimap_fetch_type_new(int ft_type,
    struct mailimap_fetch_att * ft_fetch_att,
    clist * ft_fetch_att_list)
{
  struct mailimap_fetch_type * fetch_type;

  fetch_type = malloc(sizeof(* fetch_type));
  if (fetch_type == NULL)
    return NULL;
  fetch_type->ft_type = ft_type;
  switch (ft_type) {
  case MAILIMAP_FETCH_TYPE_FETCH_ATT:
    fetch_type->ft_data.ft_fetch_att = ft_fetch_att;
    break;
  case MAILIMAP_FETCH_TYPE_FETCH_ATT_LIST:
    fetch_type->ft_data.ft_fetch_att_list = ft_fetch_att_list;
    break;
  }
  
  return fetch_type;
}

void mailimap_fetch_type_free(struct mailimap_fetch_type * fetch_type)
{
  switch (fetch_type->ft_type) {
  case MAILIMAP_FETCH_TYPE_FETCH_ATT:
    mailimap_fetch_att_free(fetch_type->ft_data.ft_fetch_att);
    break;
  case MAILIMAP_FETCH_TYPE_FETCH_ATT_LIST:
    clist_foreach(fetch_type->ft_data.ft_fetch_att_list,
        (clist_func) mailimap_fetch_att_free, NULL);
    clist_free(fetch_type->ft_data.ft_fetch_att_list);
    break;
  }
  free(fetch_type);
}




struct mailimap_store_att_flags *
mailimap_store_att_flags_new(int fl_sign, int fl_silent,
    struct mailimap_flag_list * fl_flag_list)
{
  struct mailimap_store_att_flags * store_att_flags;

  store_att_flags = malloc(sizeof(* store_att_flags));
  if (store_att_flags == NULL)
    return NULL;

  store_att_flags->fl_sign = fl_sign;
  store_att_flags->fl_silent = fl_silent;
  store_att_flags->fl_flag_list = fl_flag_list;
  
  return store_att_flags;
}

void mailimap_store_att_flags_free(struct mailimap_store_att_flags *
				   store_att_flags)
{
  mailimap_flag_list_free(store_att_flags->fl_flag_list);
  free(store_att_flags);
}


struct mailimap_search_key *
mailimap_search_key_new(int sk_type,
    char * sk_bcc, struct mailimap_date * sk_before, char * sk_body,
    char * sk_cc, char * sk_from, char * sk_keyword,
    struct mailimap_date * sk_on, struct mailimap_date * sk_since,
    char * sk_subject, char * sk_text, char * sk_to,
    char * sk_unkeyword, char * sk_header_name,
    char * sk_header_value, uint32_t sk_larger,
    struct mailimap_search_key * sk_not,
    struct mailimap_search_key * sk_or1,
    struct mailimap_search_key * sk_or2,
    struct mailimap_date * sk_sentbefore,
    struct mailimap_date * sk_senton,
    struct mailimap_date * sk_sentsince,
    uint32_t sk_smaller, struct mailimap_set * sk_uid,
    struct mailimap_set * sk_set, clist * sk_multiple)
{
  struct mailimap_search_key * key;

  key = malloc(sizeof(* key));
  if (key == NULL)
    return NULL;
  
  key->sk_type = sk_type;
  switch (sk_type) {
  case MAILIMAP_SEARCH_KEY_BCC:
    key->sk_data.sk_bcc = sk_bcc;
    break;
  case MAILIMAP_SEARCH_KEY_BEFORE:
    key->sk_data.sk_before = sk_before;
    break;
  case MAILIMAP_SEARCH_KEY_BODY:
    key->sk_data.sk_body = sk_body;
    break;
  case MAILIMAP_SEARCH_KEY_CC:
    key->sk_data.sk_cc = sk_cc;
    break;
  case MAILIMAP_SEARCH_KEY_FROM:
    key->sk_data.sk_from = sk_from;
    break;
  case MAILIMAP_SEARCH_KEY_KEYWORD:
    key->sk_data.sk_keyword = sk_keyword;
    break;
  case MAILIMAP_SEARCH_KEY_ON:
    key->sk_data.sk_on = sk_on;
    break;
  case MAILIMAP_SEARCH_KEY_SINCE:
    key->sk_data.sk_since = sk_since;
    break;
  case MAILIMAP_SEARCH_KEY_SUBJECT:
    key->sk_data.sk_subject = sk_subject;
    break;
  case MAILIMAP_SEARCH_KEY_TEXT:
    key->sk_data.sk_text = sk_text;
    break;
  case MAILIMAP_SEARCH_KEY_TO:
    key->sk_data.sk_to = sk_to;
    break;
  case MAILIMAP_SEARCH_KEY_UNKEYWORD:
    key->sk_data.sk_unkeyword = sk_unkeyword;
    break;
  case MAILIMAP_SEARCH_KEY_HEADER:
    key->sk_data.sk_header.sk_header_name = sk_header_name;
    key->sk_data.sk_header.sk_header_value = sk_header_value;
    break;
  case MAILIMAP_SEARCH_KEY_LARGER:
    key->sk_data.sk_larger = sk_larger;
    break;
  case MAILIMAP_SEARCH_KEY_NOT:
    key->sk_data.sk_not = sk_not;
    break;
  case MAILIMAP_SEARCH_KEY_OR:
    key->sk_data.sk_or.sk_or1 = sk_or1;
    key->sk_data.sk_or.sk_or2 = sk_or2;
    break;
  case MAILIMAP_SEARCH_KEY_SENTBEFORE:
    key->sk_data.sk_sentbefore = sk_sentbefore;
    break;
  case MAILIMAP_SEARCH_KEY_SENTON:
    key->sk_data.sk_senton = sk_senton;
    break;
  case MAILIMAP_SEARCH_KEY_SENTSINCE:
    key->sk_data.sk_sentsince = sk_sentsince;
    break;
  case MAILIMAP_SEARCH_KEY_SMALLER:
    key->sk_data.sk_smaller = sk_smaller;
    break;
  case MAILIMAP_SEARCH_KEY_UID:
    key->sk_data.sk_uid = sk_uid;
    break;
  case MAILIMAP_SEARCH_KEY_SET:
    key->sk_data.sk_set = sk_set;
    break;
  case MAILIMAP_SEARCH_KEY_MULTIPLE:
    key->sk_data.sk_multiple = sk_multiple;
    break;
  }
  return key;
}


void mailimap_search_key_free(struct mailimap_search_key * key)
{
  switch (key->sk_type) {
  case MAILIMAP_SEARCH_KEY_BCC:
    mailimap_astring_free(key->sk_data.sk_bcc);
    break;
  case MAILIMAP_SEARCH_KEY_BEFORE:
    mailimap_date_free(key->sk_data.sk_before);
    break;
  case MAILIMAP_SEARCH_KEY_BODY:
    mailimap_astring_free(key->sk_data.sk_body);
    break;
  case MAILIMAP_SEARCH_KEY_CC:
    mailimap_astring_free(key->sk_data.sk_cc);
    break;
  case MAILIMAP_SEARCH_KEY_FROM:
    mailimap_astring_free(key->sk_data.sk_from);
    break;
  case MAILIMAP_SEARCH_KEY_KEYWORD:
    mailimap_flag_keyword_free(key->sk_data.sk_keyword);
    break;
  case MAILIMAP_SEARCH_KEY_ON:
    mailimap_date_free(key->sk_data.sk_on);
    break;
  case MAILIMAP_SEARCH_KEY_SINCE:
    mailimap_date_free(key->sk_data.sk_since);
    break;
  case MAILIMAP_SEARCH_KEY_SUBJECT:
    mailimap_astring_free(key->sk_data.sk_subject);
    break;
  case MAILIMAP_SEARCH_KEY_TEXT:
    mailimap_astring_free(key->sk_data.sk_text);
    break;
  case MAILIMAP_SEARCH_KEY_TO:
    mailimap_astring_free(key->sk_data.sk_to);
    break;
  case MAILIMAP_SEARCH_KEY_UNKEYWORD:
    mailimap_flag_keyword_free(key->sk_data.sk_unkeyword);
    break;
  case MAILIMAP_SEARCH_KEY_HEADER:
    mailimap_header_fld_name_free(key->sk_data.sk_header.sk_header_name);
    mailimap_astring_free(key->sk_data.sk_header.sk_header_value);
    break;
  case MAILIMAP_SEARCH_KEY_NOT:
    mailimap_search_key_free(key->sk_data.sk_not);
    break;
  case MAILIMAP_SEARCH_KEY_OR:
    mailimap_search_key_free(key->sk_data.sk_or.sk_or1);
    mailimap_search_key_free(key->sk_data.sk_or.sk_or2);
    break;
  case MAILIMAP_SEARCH_KEY_SENTBEFORE:
    mailimap_date_free(key->sk_data.sk_sentbefore);
    break;
  case MAILIMAP_SEARCH_KEY_SENTON:
    mailimap_date_free(key->sk_data.sk_senton);
    break;
  case MAILIMAP_SEARCH_KEY_SENTSINCE:
    mailimap_date_free(key->sk_data.sk_sentsince);
    break;
  case MAILIMAP_SEARCH_KEY_UID:
    mailimap_set_free(key->sk_data.sk_uid);
    break;
  case MAILIMAP_SEARCH_KEY_SET:
    mailimap_set_free(key->sk_data.sk_set);
    break;
  case MAILIMAP_SEARCH_KEY_MULTIPLE:
    clist_foreach(key->sk_data.sk_multiple,
        (clist_func) mailimap_search_key_free, NULL);
    clist_free(key->sk_data.sk_multiple);
    break;
  }
  
  free(key);
}








struct mailimap_status_att_list *
mailimap_status_att_list_new(clist * att_list)
{
  struct mailimap_status_att_list * status_att_list;

  status_att_list = malloc(sizeof(* status_att_list));
  if (status_att_list == NULL)
    return NULL;
  status_att_list->att_list = att_list;

  return status_att_list;
}

void mailimap_status_att_list_free(struct mailimap_status_att_list *
				   status_att_list)
{
  clist_foreach(status_att_list->att_list, (clist_func) free, NULL);
  clist_free(status_att_list->att_list);
  free(status_att_list);
}




/* main */


struct mailimap_selection_info *
mailimap_selection_info_new(void)
{
  struct mailimap_selection_info * sel_info;

  sel_info = malloc(sizeof(* sel_info));
  if (sel_info == NULL)
    return NULL;

  sel_info->sel_perm_flags = NULL;
  sel_info->sel_perm = MAILIMAP_MAILBOX_READWRITE;
  sel_info->sel_uidnext = 0;
  sel_info->sel_uidvalidity = 0;
  sel_info->sel_first_unseen = 0;
  sel_info->sel_flags = NULL;
  sel_info->sel_exists = 0;
  sel_info->sel_recent = 0;
  sel_info->sel_unseen = 0;

  return sel_info;
}

void
mailimap_selection_info_free(struct mailimap_selection_info * sel_info)
{
  if (sel_info->sel_perm_flags != NULL) {
    clist_foreach(sel_info->sel_perm_flags,
        (clist_func) mailimap_flag_perm_free, NULL);
    clist_free(sel_info->sel_perm_flags);
  }
  if (sel_info->sel_flags)
    mailimap_flag_list_free(sel_info->sel_flags);

  free(sel_info);
}

struct mailimap_connection_info *
mailimap_connection_info_new(void)
{
  struct mailimap_connection_info * conn_info;

  conn_info = malloc(sizeof(* conn_info));
  if (conn_info == NULL)
    return NULL;
  
  conn_info->imap_capability = NULL;

  return conn_info;
}

void
mailimap_connection_info_free(struct mailimap_connection_info * conn_info)
{
  if (conn_info->imap_capability != NULL)
    mailimap_capability_data_free(conn_info->imap_capability);
  free(conn_info);
}

struct mailimap_response_info *
mailimap_response_info_new(void)
{
  struct mailimap_response_info * resp_info;

  resp_info = malloc(sizeof(* resp_info));
  if (resp_info == NULL)
    goto err;

  resp_info->rsp_alert = NULL;
  resp_info->rsp_parse = NULL;
  resp_info->rsp_badcharset = NULL;
  resp_info->rsp_trycreate = FALSE;
  resp_info->rsp_mailbox_list = clist_new();
  resp_info->rsp_extension_list = clist_new();
  if (resp_info->rsp_extension_list == NULL)
    goto free;
  resp_info->rsp_mailbox_lsub = clist_new();
  if (resp_info->rsp_mailbox_lsub == NULL)
    goto free_mb_list;
  resp_info->rsp_search_result = clist_new();
  if (resp_info->rsp_search_result == NULL)
    goto free_mb_lsub;
  resp_info->rsp_status = NULL;
  resp_info->rsp_expunged = clist_new();
  if (resp_info->rsp_expunged == NULL)
    goto free_search_result;
  resp_info->rsp_fetch_list = clist_new();
  if (resp_info->rsp_fetch_list == NULL)
    goto free_expunged;
  resp_info->rsp_atom = NULL;
  resp_info->rsp_value = NULL;
  
  return resp_info;

 free_expunged:
  clist_free(resp_info->rsp_expunged);
 free_search_result:
  clist_free(resp_info->rsp_search_result);
 free_mb_lsub:
  clist_free(resp_info->rsp_mailbox_lsub);
 free_mb_list:
  clist_free(resp_info->rsp_mailbox_list);
 free:
  free(resp_info);
 err:
  return NULL;
}

void
mailimap_response_info_free(struct mailimap_response_info * resp_info)
{
  free(resp_info->rsp_value);
  free(resp_info->rsp_atom);
  if (resp_info->rsp_alert != NULL)
    free(resp_info->rsp_alert);
  if (resp_info->rsp_parse != NULL)
    free(resp_info->rsp_parse);
  if (resp_info->rsp_badcharset != NULL) {
    clist_foreach(resp_info->rsp_badcharset,
        (clist_func) mailimap_astring_free, NULL);
    clist_free(resp_info->rsp_badcharset);
  }
  if (resp_info->rsp_mailbox_list != NULL) {
    clist_foreach(resp_info->rsp_mailbox_list,
        (clist_func) mailimap_mailbox_list_free, NULL);
    clist_free(resp_info->rsp_mailbox_list);
  }
  if (resp_info->rsp_extension_list != NULL) {
    clist_foreach(resp_info->rsp_extension_list,
      (clist_func) mailimap_extension_data_free, NULL);
    clist_free(resp_info->rsp_extension_list);
  }
  if (resp_info->rsp_mailbox_lsub != NULL) {
    clist_foreach(resp_info->rsp_mailbox_lsub,
        (clist_func) mailimap_mailbox_list_free, NULL);
    clist_free(resp_info->rsp_mailbox_lsub);
  }
  if (resp_info->rsp_search_result != NULL)
    mailimap_mailbox_data_search_free(resp_info->rsp_search_result);
  if (resp_info->rsp_status != NULL)
    mailimap_mailbox_data_status_free(resp_info->rsp_status);
  if (resp_info->rsp_expunged != NULL) {
    clist_foreach(resp_info->rsp_expunged,
		   (clist_func) mailimap_number_alloc_free, NULL);
    clist_free(resp_info->rsp_expunged);
  }
  if (resp_info->rsp_fetch_list != NULL) {
    clist_foreach(resp_info->rsp_fetch_list,
		  (clist_func) mailimap_msg_att_free, NULL);
    clist_free(resp_info->rsp_fetch_list);
  }

  free(resp_info);
}
