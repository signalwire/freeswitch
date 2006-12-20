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
 * $Id: mailimap_print.c,v 1.14 2004/11/21 21:53:36 hoa Exp $
 */
#ifdef DEBUG
#include "mailimap_print.h"

#include <stdio.h>

static void mailimap_body_fields_print(struct mailimap_body_fields *
				       body_fields);
static void mailimap_envelope_print(struct mailimap_envelope * env);
static void mailimap_body_print(struct mailimap_body * body);
static void mailimap_body_fld_enc_print(struct mailimap_body_fld_enc *
					fld_enc);

static int indent_size = 0;

static void indent()
{
  indent_size ++;
}

static void unindent()
{
  indent_size --;
}

static void print_indent()
{
  int i;

  for (i = 0 ; i < indent_size ; i++)
    printf("  ");
}


static void mailimap_body_fld_lang_print(struct mailimap_body_fld_lang *
					 fld_lang)
{
  clistiter * cur;

  print_indent();
  printf("body-fld-lang { ");

  switch (fld_lang->lg_type) {
  case MAILIMAP_BODY_FLD_LANG_SINGLE:
    printf("%s ", fld_lang->lg_data.lg_single);
    break;

  case MAILIMAP_BODY_FLD_LANG_LIST:
    for(cur = clist_begin(fld_lang->lg_data.lg_list) ;
        cur != NULL ; cur = clist_next(cur)) {
      char * lang;

      lang = clist_content(cur);

      printf("%s ", lang);
    }
    break;
  }

  print_indent();
  printf("}\n");
}

static void
mailimap_single_body_fld_param_print(struct mailimap_single_body_fld_param *
				     single)
{
  printf("(%s = %s)", single->pa_name, single->pa_value);
}

static void mailimap_body_fld_param_print(struct mailimap_body_fld_param *
					  fld_param)
{
  clistiter * cur;

  print_indent();
  printf("body-fld-param { ");

  for(cur = clist_begin(fld_param->pa_list) ; cur != NULL ;
      cur = clist_next(cur)) {
    struct mailimap_single_body_fld_param * single;
    
    single = clist_content(cur);

    mailimap_single_body_fld_param_print(single);
    printf(" ");
  }
  printf("\n");
}

static void mailimap_body_fld_dsp_print(struct mailimap_body_fld_dsp * fld_dsp)
{
  print_indent();
  printf("body-fld-dsp {\n");
  indent();

  print_indent();
  printf("name { %s }\n", fld_dsp->dsp_type);

  mailimap_body_fld_param_print(fld_dsp->dsp_attributes);

  unindent();
  print_indent();
  printf("}\n");
}

static void mailimap_body_extension_list_print(clist * ext_list);

static void mailimap_body_extension_print(struct mailimap_body_extension * ext)
{
  print_indent();
  printf("body-extention {\n");
  indent();

  switch (ext->ext_type) {
  case MAILIMAP_BODY_EXTENSION_NSTRING:
    print_indent();
    printf("%s\n", ext->ext_data.ext_nstring);
    break;
  case MAILIMAP_BODY_EXTENSION_NUMBER:
    print_indent();
    printf("%i\n", ext->ext_data.ext_number);
    break;
  case MAILIMAP_BODY_EXTENSION_LIST:
    mailimap_body_extension_list_print(ext->ext_data.ext_body_extension_list);
    break;
  }

  unindent();
  print_indent();
  printf("}\n");

}

static void mailimap_body_extension_list_print(clist * ext_list)
{
  clistiter * cur;

  print_indent();
  printf("body-extention-list {\n");
  indent();

  for (cur = clist_begin(ext_list) ; cur != NULL ;
       cur = clist_next(cur)) {
    struct mailimap_body_extension * ext;
    
    ext = clist_content(cur);
    
    mailimap_body_extension_print(ext);
  }

  unindent();
  print_indent();
  printf("}");
}

static void mailimap_body_ext_1part_print(struct mailimap_body_ext_1part *
					  body_ext_1part)
{
  print_indent();
  printf("body-type-1part {\n");
  indent();

  print_indent();
  printf("md5 { %s }\n", body_ext_1part->bd_md5);
  if (body_ext_1part->bd_disposition) {
    mailimap_body_fld_dsp_print(body_ext_1part->bd_disposition);
    if (body_ext_1part->bd_language) {
      mailimap_body_fld_lang_print(body_ext_1part->bd_language);

      if (body_ext_1part->bd_extension_list)
	mailimap_body_extension_list_print(body_ext_1part->bd_extension_list);
    }
  }

  unindent();
  print_indent();
  printf("}\n");
}

static void mailimap_body_type_text_print(struct mailimap_body_type_text *
					  body_type_text)
{
  print_indent();
  printf("body-type-text {\n");
  indent();

  print_indent();
  printf("media-text { %s }\n", body_type_text->bd_media_text);
  mailimap_body_fields_print(body_type_text->bd_fields);
  print_indent();
  printf("lines { %i }\n", body_type_text->bd_lines);

  unindent();
  print_indent();
  printf("}\n");
}

static void mailimap_body_type_msg_print(struct mailimap_body_type_msg *
					 body_type_msg)
{
  print_indent();
  printf("body-type-msg {\n");
  indent();

  mailimap_body_fields_print(body_type_msg->bd_fields);
  mailimap_envelope_print(body_type_msg->bd_envelope);
  mailimap_body_print(body_type_msg->bd_body);

  print_indent();
  printf("lines { %i }\n", body_type_msg->bd_lines);

  unindent();
  print_indent();
  printf("}\n");
}


static void mailimap_body_fld_enc_print(struct mailimap_body_fld_enc * fld_enc)
{
  print_indent();
  printf("body-fld-enc { ");

  switch (fld_enc->enc_type) {
  case MAILIMAP_BODY_FLD_ENC_7BIT:
    print_indent();
    printf("7bit");
    break;
  case MAILIMAP_BODY_FLD_ENC_8BIT:
    printf("8bit");
    break;
  case MAILIMAP_BODY_FLD_ENC_BINARY:
    printf("binary");
    break;
  case MAILIMAP_BODY_FLD_ENC_BASE64:
    printf("base64");
    break;
  case MAILIMAP_BODY_FLD_ENC_QUOTED_PRINTABLE:
    printf("quoted-printable");
    break;
  case MAILIMAP_BODY_FLD_ENC_OTHER:
    printf("%s", fld_enc->enc_value);
    break;
  }

  printf("}\n");
}

static void mailimap_body_fields_print(struct mailimap_body_fields *
				       body_fields)
{
  print_indent();
  printf("body-fields {\n");
  indent();

  mailimap_body_fld_param_print(body_fields->bd_parameter);

  print_indent();
  printf("body-fld-id { %s }\n", body_fields->bd_id);
  printf("body-fld-desc { %s }\n", body_fields->bd_description);
  mailimap_body_fld_enc_print(body_fields->bd_encoding);
  printf("body-fld-octets { %i }\n", body_fields->bd_size);

  unindent();
  print_indent();
  printf("}\n");
}

static void mailimap_media_basic_print(struct mailimap_media_basic *
				       media_basic)
{
  print_indent();
  printf("media-basic {");

  switch (media_basic->med_type) {
  case MAILIMAP_MEDIA_BASIC_APPLICATION:
    printf("application");
    break;
  case MAILIMAP_MEDIA_BASIC_AUDIO:
    printf("audio");
    break;
  case MAILIMAP_MEDIA_BASIC_IMAGE:
    printf("image");
    break;
  case MAILIMAP_MEDIA_BASIC_MESSAGE:
    printf("message");
    break;
  case MAILIMAP_MEDIA_BASIC_VIDEO:
    printf("video");
    break;
  case MAILIMAP_MEDIA_BASIC_OTHER:
    printf("%s", media_basic->med_basic_type);
    break;
  }
  printf(" / %s }\n", media_basic->med_subtype);
}

static void mailimap_body_type_basic_print(struct mailimap_body_type_basic *
					   body_type_basic)
{
  print_indent();
  printf("body-type-basic {\n");
  indent();

  mailimap_media_basic_print(body_type_basic->bd_media_basic);
  mailimap_body_fields_print(body_type_basic->bd_fields);

  unindent();
  print_indent();
  printf("}\n");
}

static void mailimap_body_type_1part_print(struct mailimap_body_type_1part *
					   body_type_1part)
{
  print_indent();
  printf("body-type-1part {\n");
  indent();

  switch (body_type_1part->bd_type) {
  case MAILIMAP_BODY_TYPE_1PART_BASIC:
    mailimap_body_type_basic_print(body_type_1part->bd_data.bd_type_basic);
    break;
    
  case MAILIMAP_BODY_TYPE_1PART_MSG:
    mailimap_body_type_msg_print(body_type_1part->bd_data.bd_type_msg);
    break;

  case MAILIMAP_BODY_TYPE_1PART_TEXT:
    mailimap_body_type_text_print(body_type_1part->bd_data.bd_type_text);
    break;
  }

  if (body_type_1part->bd_ext_1part != NULL)
    mailimap_body_ext_1part_print(body_type_1part->bd_ext_1part);

  unindent();
  print_indent();
  printf("\n");
}

static void mailimap_body_ext_mpart(struct mailimap_body_ext_mpart * ext_mpart)
{
  print_indent();
  printf("body-ext-mpart {\n");
  indent();

  mailimap_body_fld_param_print(ext_mpart->bd_parameter);
  if (ext_mpart->bd_disposition) {
    mailimap_body_fld_dsp_print(ext_mpart->bd_disposition);
    if (ext_mpart->bd_language) {
      mailimap_body_fld_lang_print(ext_mpart->bd_language);

      if (ext_mpart->bd_extension_list)
	mailimap_body_extension_list_print(ext_mpart->bd_extension_list);
    }
  }

  unindent();
  print_indent();
  printf("\n");
}

static void mailimap_body_type_mpart_print(struct mailimap_body_type_mpart *
					   mpart)
{
  clistiter * cur;

  print_indent();
  printf("body-type-mpart {\n");
  indent();

  for(cur = clist_begin(mpart->bd_list) ; cur != NULL ;
      cur = clist_next(cur)) {
    struct mailimap_body * body;

    body = clist_content(cur);
    
    mailimap_body_print(body);
  }

  printf("media-subtype { %s }\n", mpart->bd_media_subtype);

  if (mpart->bd_ext_mpart)
    mailimap_body_ext_mpart(mpart->bd_ext_mpart);

  unindent();
  print_indent();
  printf("}\n");
}


static void mailimap_body_print(struct mailimap_body * body)
{
  print_indent();
  printf("body {\n");
  indent();

  switch (body->bd_type) {
  case MAILIMAP_BODY_1PART:
    mailimap_body_type_1part_print(body->bd_data.bd_body_1part);
    break;
  case MAILIMAP_BODY_MPART:
    mailimap_body_type_mpart_print(body->bd_data.bd_body_mpart);
    break;
  }

  unindent();
  print_indent();
  printf("}\n");
}

static void mailimap_date_time_print(struct mailimap_date_time * date_time)
{
  print_indent();
  printf("date-time { %i/%i/%i - %i:%i:%i %i }\n",
	 date_time->dt_day, date_time->dt_month, date_time->dt_year,
	 date_time->dt_hour, date_time->dt_min, date_time->dt_month,
	 date_time->dt_zone);
}

static void mailimap_address_print(struct mailimap_address * address)
{
  print_indent();
  printf("address { name: %s, addr: %s, mailbox: %s, host: %s) }\n",
	 address->ad_personal_name, address->ad_source_route,
	 address->ad_mailbox_name, address->ad_host_name);
}

static void mailimap_envelope_address_list_print(clist * address)
{
  clistiter * cur;

  print_indent();
  printf("envelope-address-list {\n");
  indent();

  for(cur = clist_begin(address) ; cur != NULL ;
      cur = clist_next(cur)) {
    struct mailimap_address * addr;

    addr = clist_content(cur);

    mailimap_address_print(addr);
  }

  unindent();
  print_indent();
  printf("}\n");
}

static void mailimap_envelope_print(struct mailimap_envelope * env)
{
  print_indent();
  printf("envelope {\n");
  indent();

  print_indent();
  printf("date { %s }\n", env->env_date);

  print_indent();
  printf("subject { %s }\n", env->env_subject);

  print_indent();
  printf("from {\n");
  indent();
  mailimap_envelope_address_list_print(env->env_from->frm_list);
  unindent();
  print_indent();
  printf("}\n");

  print_indent();
  printf("sender {\n");
  indent();
  mailimap_envelope_address_list_print(env->env_sender->snd_list);
  unindent();
  print_indent();
  printf("}\n");

  print_indent();
  printf("reply-to {\n");
  indent();
  mailimap_envelope_address_list_print(env->env_reply_to->rt_list);
  unindent();
  print_indent();
  printf("}\n");

  print_indent();
  printf("to {\n");
  indent();
  mailimap_envelope_address_list_print(env->env_to->to_list);
  unindent();
  print_indent();
  printf("}\n");

  print_indent();
  printf("cc {\n");
  indent();
  mailimap_envelope_address_list_print(env->env_cc->cc_list);
  unindent();
  print_indent();
  printf("}\n");

  print_indent();
  printf("bcc {\n");
  indent();
  mailimap_envelope_address_list_print(env->env_bcc->bcc_list);
  unindent();
  print_indent();
  printf("}\n");

  printf("in-reply-to { %s }\n", env->env_in_reply_to);
  printf("message-id { %s }\n", env->env_message_id);

  unindent();
  print_indent();
  printf("}\n");
}

static void mailimap_header_list_print(struct mailimap_header_list *
				       header_list)
{
  clistiter * cur;

  print_indent();
  printf("header-list { ");
  for(cur = clist_begin(header_list->hdr_list) ; cur != NULL ;
      cur = clist_next(cur))
    printf("%s ", (char *) clist_content(cur));
  printf("}\n");
}

static void mailimap_section_msgtext_print(struct mailimap_section_msgtext *
					   section_msgtext)
{
  print_indent();
  printf("section-msgtext {\n");
  indent();

  switch(section_msgtext->sec_type) {
  case MAILIMAP_SECTION_MSGTEXT_HEADER:
    print_indent();
    printf("header\n");
    break;

  case MAILIMAP_SECTION_MSGTEXT_HEADER_FIELDS:
    print_indent();
    printf("header fields {");
    indent();
    mailimap_header_list_print(section_msgtext->sec_header_list);
    unindent();
    print_indent();
    printf("}\n");
    break;

  case MAILIMAP_SECTION_MSGTEXT_HEADER_FIELDS_NOT:
    print_indent();
    printf("header fields not {");
    indent();
    mailimap_header_list_print(section_msgtext->sec_header_list);
    unindent();
    print_indent();
    printf("}\n");
    break;

  case MAILIMAP_SECTION_MSGTEXT_TEXT:
    print_indent();
    printf("text\n");
    break;
  }

  unindent();
  print_indent();
  printf("}\n");
}

static void mailimap_section_part_print(struct mailimap_section_part *
					section_part)
{
  clistiter * cur;

  print_indent();
  printf("section-part { ");

  for(cur = clist_begin(section_part->sec_id) ;
      cur != NULL ; cur = clist_next(cur)) {
    printf("%i", * ((uint32_t *) clist_content(cur)));
    if (clist_next(cur) != NULL)
      printf(".");
  }
  printf(" }\n");
}

static void mailimap_section_text_print(struct mailimap_section_text *
					section_text)
{
  print_indent();
  printf("section-text {\n");
  indent();

  switch (section_text->sec_type) {
  case MAILIMAP_SECTION_TEXT_MIME:
    print_indent();
    printf("MIME");
    break;
  case MAILIMAP_SECTION_TEXT_SECTION_MSGTEXT:
    mailimap_section_msgtext_print(section_text->sec_msgtext);
    break;
  }

  unindent();
  print_indent();
  printf("}\n");
}

static void mailimap_section_spec_print(struct mailimap_section_spec *
					section_spec)
{
  print_indent();
  printf("section-spec {");
  indent();

  switch(section_spec->sec_type) {
  case MAILIMAP_SECTION_SPEC_SECTION_MSGTEXT:
    mailimap_section_msgtext_print(section_spec->sec_data.sec_msgtext);
    break;
  case MAILIMAP_SECTION_SPEC_SECTION_PART:
    mailimap_section_part_print(section_spec->sec_data.sec_part);
    if (section_spec->sec_text != NULL)
      mailimap_section_text_print(section_spec->sec_text);
    break;
  }

  unindent();
  print_indent();
  printf("}\n");
}

static void mailimap_section_print(struct mailimap_section * section)
{
  print_indent();
  printf("section {\n");
  indent();

  if (section != NULL)
    if (section->sec_spec != NULL)
      mailimap_section_spec_print(section->sec_spec);

  unindent();
  print_indent();
  printf("}\n");
}

static void mailimap_msg_att_body_section_print(struct
						mailimap_msg_att_body_section *
						msg_att_body_section)
{
  print_indent();
  printf("msg-att-body-section {\n");
  indent();

  mailimap_section_print(msg_att_body_section->sec_section);
  printf("origin-octet: %i\n", msg_att_body_section->sec_origin_octet);
  printf("body-part: %s\n", msg_att_body_section->sec_body_part);

  unindent();
  print_indent();
  printf("}\n");
}


static void mailimap_msg_att_static_print(struct mailimap_msg_att_static *
					  msg_att_static)
{
  print_indent();
  printf("msg-att-static {\n");
  indent();

  switch (msg_att_static->att_type) {

  case MAILIMAP_MSG_ATT_ENVELOPE:
    print_indent();
    printf("envelope {\n");
    indent();
    print_indent();
    mailimap_envelope_print(msg_att_static->att_data.att_env);
    unindent();
    print_indent();
    printf("}\n");
    break;

  case MAILIMAP_MSG_ATT_INTERNALDATE:
    print_indent();
    printf("internaldate {\n");
    indent();
    print_indent();
    mailimap_date_time_print(msg_att_static->att_data.att_internal_date);
    unindent();
    print_indent();
    printf("}\n");
    break;

  case MAILIMAP_MSG_ATT_RFC822:
    print_indent();
    printf("rfc822 {\n");
    printf("%s\n", msg_att_static->att_data.att_rfc822.att_content);
    print_indent();
    printf("}\n");
    break;

  case MAILIMAP_MSG_ATT_RFC822_HEADER:
    print_indent();
    printf("rfc822-header {\n");
    printf("%s\n", msg_att_static->att_data.att_rfc822_header.att_content);
    print_indent();
    printf("}\n");
    break;

  case MAILIMAP_MSG_ATT_RFC822_TEXT:
    print_indent();
    printf("rfc822-text {\n");
    printf("%s\n", msg_att_static->att_data.att_rfc822_text.att_content);
    print_indent();
    printf("}\n");
    break;

  case MAILIMAP_MSG_ATT_RFC822_SIZE:
    print_indent();
    printf("rfc822-size { %i }\n", msg_att_static->att_data.att_rfc822_size);
    break;

  case MAILIMAP_MSG_ATT_BODY:
    print_indent();
    printf("body {\n");
    indent();
    print_indent();
    mailimap_body_print(msg_att_static->att_data.att_body);
    unindent();
    print_indent();
    printf("}\n");
    break;

  case MAILIMAP_MSG_ATT_BODYSTRUCTURE:
    print_indent();
    printf("bodystructure {\n");
    indent();
    print_indent();
    mailimap_body_print(msg_att_static->att_data.att_bodystructure);
    unindent();
    print_indent();
    printf("}\n");
    break;

  case MAILIMAP_MSG_ATT_BODY_SECTION:
    print_indent();
    printf("body-section {\n");
    indent();
    print_indent();
    mailimap_msg_att_body_section_print(msg_att_static->att_data.att_body_section);
    unindent();
    print_indent();
    printf("}\n");
    break;

  case MAILIMAP_MSG_ATT_UID:
    printf("uid { %i }\n", msg_att_static->att_data.att_uid);
    break;
  }

  unindent();
  print_indent();
  printf("}\n");
}

static void mailimap_flag_print(struct mailimap_flag * flag);

static void mailimap_flag_fetch_print(struct mailimap_flag_fetch * flag)
{
  print_indent();
  printf("flag fetch {\n");
  indent();

  switch (flag->fl_type) {
  case MAILIMAP_FLAG_FETCH_RECENT:
    printf("recent\n");
    break;
  case MAILIMAP_FLAG_FETCH_OTHER:
    print_indent();
    printf("flag {\n");
    indent();
    mailimap_flag_print(flag->fl_flag);
    unindent();
    print_indent();
    printf("}\n");
    break;
  }

  unindent();
  print_indent();
  printf("}\n");
}

static void mailimap_msg_att_dynamic_print(struct mailimap_msg_att_dynamic *
					   dynamic)
{
  clistiter * cur;

  print_indent();
  printf("msg-att-dynamic {\n");
  indent();

  for(cur = clist_begin(dynamic->att_list) ; cur != NULL ;
      cur = clist_next(cur)) {
    struct mailimap_flag_fetch * flag;

    flag = (struct mailimap_flag_fetch *) clist_content(cur);
    mailimap_flag_fetch_print(flag);
  }

  unindent();
  print_indent();
  printf("}\n");
}

static void mailimap_msg_att_item_print(struct mailimap_msg_att_item * item)
{
  print_indent();
  printf("msg-att-item {\n");
  indent();

  switch (item->att_type) {
  case MAILIMAP_MSG_ATT_ITEM_DYNAMIC:
    mailimap_msg_att_dynamic_print(item->att_data.att_dyn);
    break;
  case MAILIMAP_MSG_ATT_ITEM_STATIC:
    mailimap_msg_att_static_print(item->att_data.att_static);
    break;
  }

  unindent();
  print_indent();
  printf("}\n");
}

static void mailimap_msg_att_print(struct mailimap_msg_att * msg_att)
{
  clistiter * cur;

  print_indent();
  printf("msg-att {\n");
  indent();

  for(cur = clist_begin(msg_att->att_list) ; cur != NULL ;
      cur = clist_next(cur)) {
    struct mailimap_msg_att_item * item;

    item = clist_content(cur);
    
    mailimap_msg_att_item_print(item);
  }

  unindent();
  print_indent();
  printf("}\n");
}

static void mailimap_message_data_print(struct mailimap_message_data *
					msg_data)
{
  print_indent();
  printf("message-data {\n");
  indent();

  switch (msg_data->mdt_type) {
  case MAILIMAP_MESSAGE_DATA_EXPUNGE:
    print_indent();
    printf("expunged { %i }\n", msg_data->mdt_number);
    break;
  case MAILIMAP_MESSAGE_DATA_FETCH:
    print_indent();
    printf("message-number { %i }\n", msg_data->mdt_number);
    mailimap_msg_att_print(msg_data->mdt_msg_att);
    break;
  }

  unindent();
  print_indent();
  printf("}\n");
}

static void mailimap_status_att_print(int status_att)
{
  print_indent();
  printf("status-att { ");

  switch(status_att) {
  case MAILIMAP_STATUS_ATT_MESSAGES:
    printf("messages");
    break;
  case MAILIMAP_STATUS_ATT_RECENT:
    printf("recent");
    break;
  case MAILIMAP_STATUS_ATT_UIDNEXT:
    printf("uidnext");
    break;
  case MAILIMAP_STATUS_ATT_UIDVALIDITY:
    printf("status att uidvalidity");
    break;
  case MAILIMAP_STATUS_ATT_UNSEEN:
    printf("status att unseen");
    break;
  }

  printf(" \n");
}

static void
mailimap_status_info_print(struct mailimap_status_info * info)
{
  print_indent();
  printf("status-info {\n");
  indent();

  mailimap_status_att_print(info->st_att);

  print_indent();
  printf("value { %i }\n", info->st_value);

  unindent();
  print_indent();
  printf("}\n");
}

static void
mailimap_mailbox_data_status_print(struct mailimap_mailbox_data_status *
				   mb_data_status)
{
  clistiter * cur;

  print_indent();
  printf("mailbox-data-status {\n");
  indent();

  print_indent();
  printf("mailbox { %s }\n", mb_data_status->st_mailbox);

  for(cur = clist_begin(mb_data_status->st_info_list) ;
      cur != NULL ; cur = clist_next(cur)) {
    struct mailimap_status_info * info;

    info = clist_content(cur);

    mailimap_status_info_print(info);
  }

  unindent();
  print_indent();
  printf("}\n");
}

static void mailimap_mbx_list_oflag_print(struct mailimap_mbx_list_oflag *
					  oflag)
{
  print_indent();
  printf("mbx-list-oflag { ");

  switch (oflag->of_type) {
  case MAILIMAP_MBX_LIST_OFLAG_NOINFERIORS:
    printf("noinferiors");
    break;
  case MAILIMAP_MBX_LIST_OFLAG_FLAG_EXT:
    printf("%s", oflag->of_flag_ext);
    break;
  }

  printf(" }\n");
}

static void mailimap_mbx_list_sflag_print(int sflag)
{
  print_indent();
  printf("mbx-list-sflag { ");

  switch (sflag) {
  case MAILIMAP_MBX_LIST_SFLAG_MARKED:
    printf("marked");
    break;
  case MAILIMAP_MBX_LIST_SFLAG_NOSELECT:
    printf("noselected");
    break;
  case MAILIMAP_MBX_LIST_SFLAG_UNMARKED:
    printf("unmarked");
    break;
  }

  printf(" }\n");
}

static void mailimap_mbx_list_flags_print(struct mailimap_mbx_list_flags *
					  mbx_list_flags)
{
  clistiter * cur;

  print_indent();
  printf("mbx-list-flags {");
  indent();

  if (mbx_list_flags->mbf_type == MAILIMAP_MBX_LIST_FLAGS_SFLAG)
    mailimap_mbx_list_sflag_print(mbx_list_flags->mbf_sflag);

  for(cur = clist_begin(mbx_list_flags->mbf_oflags) ; cur != NULL ;
      cur = clist_next(cur)) {
    struct mailimap_mbx_list_oflag * oflag;

    oflag = clist_content(cur);

    mailimap_mbx_list_oflag_print(oflag);
  }

  unindent();
  print_indent();
  printf("}\n");
}

static void mailimap_mailbox_list_print(struct mailimap_mailbox_list * mb_list)
{
  print_indent();
  printf("mailbox-list {\n");
  indent();

  mailimap_mbx_list_flags_print(mb_list->mb_flag);
  printf("dir-separator { %c }\n", mb_list->mb_delimiter);
  printf("mailbox { %s }\n", mb_list->mb_name);

  unindent();
  print_indent();
  printf("}\n");
}

static void mailimap_flag_list_print(struct mailimap_flag_list * flag_list)
{
  clistiter * cur;

  print_indent();
  printf("flag-list {\n");
  indent();

  for(cur = clist_begin(flag_list->fl_list) ; cur != NULL ;
      cur = clist_next(cur)) {
    struct mailimap_flag * flag;

    flag = clist_content(cur);
    
    print_indent();
    mailimap_flag_print(flag);
    printf("\n");
  }

  unindent();
  print_indent();
  printf("}\n");
}


static void mailimap_mailbox_data_print(struct mailimap_mailbox_data * mb_data)
{
  clistiter * cur;

  print_indent();
  printf("mailbox-data {\n");
  indent();

  switch (mb_data->mbd_type) {
  case MAILIMAP_MAILBOX_DATA_FLAGS:
    print_indent();
    printf("flags {\n");
    indent();
    mailimap_flag_list_print(mb_data->mbd_data.mbd_flags);
    unindent();
    print_indent();
    printf("}\n");
    break;

  case MAILIMAP_MAILBOX_DATA_LIST:
    print_indent();
    printf("list {\n");
    indent();
    mailimap_mailbox_list_print(mb_data->mbd_data.mbd_list);
    unindent();
    print_indent();
    printf("}\n");
    break;

  case MAILIMAP_MAILBOX_DATA_LSUB:
    print_indent();
    printf("lsub {\n");
    indent();
    mailimap_mailbox_list_print(mb_data->mbd_data.mbd_lsub);
    unindent();
    print_indent();
    printf("}\n");
    break;

  case MAILIMAP_MAILBOX_DATA_SEARCH:
    print_indent();
    printf("search { ");
    for(cur = clist_begin(mb_data->mbd_data.mbd_search) ;
        cur != NULL ; cur = clist_next(cur)) {
      uint32_t * id;

      id = clist_content(cur);
      printf("%i ", * id);
    }
    printf(" }\n");
    break;

  case MAILIMAP_MAILBOX_DATA_STATUS:
    print_indent();
    printf("status {\n");
    indent();
    mailimap_mailbox_data_status_print(mb_data->mbd_data.mbd_status);
    unindent();
    print_indent();
    printf("}\n");
    break;

  case MAILIMAP_MAILBOX_DATA_EXISTS:
    print_indent();
    printf("exists { %i }\n", mb_data->mbd_data.mbd_exists);
    break;

  case MAILIMAP_MAILBOX_DATA_RECENT:
    print_indent();
    printf("recent { %i }\n", mb_data->mbd_data.mbd_recent);
    break;
  }

  unindent();
  print_indent();
  printf("}\n");
}

static void
mailimap_resp_text_code_print(struct mailimap_resp_text_code * text_code);

static void mailimap_resp_text_print(struct mailimap_resp_text * resp_text);

static void mailimap_resp_cond_bye_print(struct mailimap_resp_cond_bye *
					 resp_cond_bye)
{
  print_indent();
  printf("resp-cond-bye {\n");
  indent();
  mailimap_resp_text_print(resp_cond_bye->rsp_text);
  unindent();
  print_indent();
  printf("}\n");
}

static void mailimap_resp_cond_state_print(struct mailimap_resp_cond_state *
					   resp_cond_state)
{
  print_indent();
  printf("resp-cond-state {\n");
  indent();

  switch(resp_cond_state->rsp_type) {
  case MAILIMAP_RESP_COND_STATE_OK:
    print_indent();
    printf("OK\n");
    break;
  case MAILIMAP_RESP_COND_STATE_NO:
    print_indent();
    printf("NO\n");
    break;
  case MAILIMAP_RESP_COND_STATE_BAD:
    print_indent();
    printf("BAD\n");
    break;
  }

  mailimap_resp_text_print(resp_cond_state->rsp_text);

  unindent();
  print_indent();
  printf("}\n");
}

static void mailimap_capability_data_print(struct mailimap_capability_data *
					   cap_data);

static void mailimap_response_data_print(struct mailimap_response_data *
					 resp_data)
{
  print_indent();
  printf("response-data {\n");
  indent();

  switch (resp_data->rsp_type) {
  case MAILIMAP_RESP_DATA_TYPE_COND_STATE:
    mailimap_resp_cond_state_print(resp_data->rsp_data.rsp_cond_state);
    break;
  case MAILIMAP_RESP_DATA_TYPE_COND_BYE:
    mailimap_resp_cond_bye_print(resp_data->rsp_data.rsp_bye);
    break;
  case MAILIMAP_RESP_DATA_TYPE_MAILBOX_DATA:
    mailimap_mailbox_data_print(resp_data->rsp_data.rsp_mailbox_data);
    break;
  case MAILIMAP_RESP_DATA_TYPE_MESSAGE_DATA:
    mailimap_message_data_print(resp_data->rsp_data.rsp_message_data);
    break;
  case MAILIMAP_RESP_DATA_TYPE_CAPABILITY_DATA:
    mailimap_capability_data_print(resp_data->rsp_data.rsp_capability_data);
    break;
  }

  unindent();
  print_indent();
  printf("}\n");
}

static void mailimap_flag_print(struct mailimap_flag * flag)
{
  printf("flag { ");

  switch (flag->fl_type) {
  case MAILIMAP_FLAG_ANSWERED:
    printf("answered");
    break;

  case MAILIMAP_FLAG_FLAGGED:
    printf("flagged");
    break;

  case MAILIMAP_FLAG_DELETED:
    printf("deleted");
    break;

  case MAILIMAP_FLAG_SEEN:
    printf("seen");
    break;

  case MAILIMAP_FLAG_DRAFT:
    printf("flag draft");
    break;

  case MAILIMAP_FLAG_KEYWORD:
    printf("keyword { %s }", flag->fl_data.fl_keyword);
    break;

  case MAILIMAP_FLAG_EXTENSION:
    printf("extention { %s }", flag->fl_data.fl_extension);
    break;
  }

  printf(" }");
}

static void mailimap_flag_perm_print(struct mailimap_flag_perm * flag_perm)
{
  print_indent();
  printf("flag-perm { ");

  switch (flag_perm->fl_type) {
  case MAILIMAP_FLAG_PERM_FLAG:
    mailimap_flag_print(flag_perm->fl_flag);
    break;

  case MAILIMAP_FLAG_PERM_ALL:
    printf("all");
    break;
  }

  printf(" }\n");
}
 
static void mailimap_capability_print(struct mailimap_capability * cap)
{
  print_indent();
  printf("capability { ");

  switch (cap->cap_type) {
  case MAILIMAP_CAPABILITY_AUTH_TYPE:
    printf("auth { %s }", cap->cap_data.cap_auth_type);
    break;
  case MAILIMAP_CAPABILITY_NAME:
    printf("atom { %s }", cap->cap_data.cap_name);
    break;
  }

  printf(" }\n");
}

static void mailimap_capability_data_print(struct mailimap_capability_data *
					   cap_data)
{
  clistiter * cur;

  print_indent();
  printf("capability-data {\n");
  indent();

  for(cur = clist_begin(cap_data->cap_list) ; cur != NULL ;
      cur = clist_next(cur)) {
    struct mailimap_capability * cap;

    cap = clist_content(cur);

    mailimap_capability_print(cap);
  }

  unindent();
  print_indent();
  printf("}\n");
}

static void
mailimap_resp_text_code_print(struct mailimap_resp_text_code * text_code)
{
  clistiter * cur;

  print_indent();
  printf("resp-text-code {\n");
  indent();

  switch (text_code->rc_type) {
  case MAILIMAP_RESP_TEXT_CODE_BADCHARSET:
    print_indent();
    printf("badcharset { ");
    for(cur = clist_begin(text_code->rc_data.rc_badcharset) ; cur != NULL ;
	cur = clist_next(cur))
      printf("%s ", (char *) clist_content(cur));
    printf("}\n");
    break;

  case MAILIMAP_RESP_TEXT_CODE_CAPABILITY_DATA:
    print_indent();
    printf("capability {\n");
    indent();
    mailimap_capability_data_print(text_code->rc_data.rc_cap_data);
    unindent();
    print_indent();
    printf("}\n");
    break;

  case MAILIMAP_RESP_TEXT_CODE_PERMANENTFLAGS:
    print_indent();
    printf("permanent-flags {\n");
    indent();
    cur = clist_begin(text_code->rc_data.rc_perm_flags);
    while (cur != NULL) {
      mailimap_flag_perm_print(clist_content(cur));
      cur = clist_next(cur);
    }
    unindent();
    print_indent();
    printf("}\n");
    break;

  case MAILIMAP_RESP_TEXT_CODE_READ_ONLY:
    print_indent();
    printf("readonly\n");
    break;

  case MAILIMAP_RESP_TEXT_CODE_READ_WRITE:
    print_indent();
    printf("readwrite\n");
    break;

  case MAILIMAP_RESP_TEXT_CODE_TRY_CREATE:
    print_indent();
    printf("trycreate\n");
    break;

  case MAILIMAP_RESP_TEXT_CODE_UIDNEXT:
    print_indent();
    printf("uidnext { %i }\n", text_code->rc_data.rc_uidnext);
    break;

  case MAILIMAP_RESP_TEXT_CODE_UIDVALIDITY:
    print_indent();
    printf("uidvalidity { %i }\n", text_code->rc_data.rc_uidvalidity);
    break;

  case MAILIMAP_RESP_TEXT_CODE_UNSEEN:
    print_indent();
    printf("unseen { %i }\n", text_code->rc_data.rc_first_unseen);
    break;

  case MAILIMAP_RESP_TEXT_CODE_OTHER:
    print_indent();
    printf("other { %s = %s }\n",
	   text_code->rc_data.rc_atom.atom_name, 
	   text_code->rc_data.rc_atom.atom_value);
    break;
  }

  unindent();
  print_indent();
  printf("}\n");
}

static void mailimap_resp_text_print(struct mailimap_resp_text * resp_text)
{
  print_indent();
  printf("resp-text {\n");
  indent();

  if (resp_text->rsp_code)
    mailimap_resp_text_code_print(resp_text->rsp_code);
  print_indent();
  printf("text { %s }\n", resp_text->rsp_text);

  unindent();
  print_indent();
  printf("}\n");
}

static void mailimap_continue_req_print(struct mailimap_continue_req *
					cont_req)
{
  print_indent();
  printf("continue-req {\n");
  indent();

  switch (cont_req->cr_type) {
  case MAILIMAP_CONTINUE_REQ_TEXT:
    print_indent();
    printf("resp-text {\n");
    indent();
    mailimap_resp_text_print(cont_req->cr_data.cr_text);
    unindent();
    print_indent();
    printf("}\n");
    break;
  case MAILIMAP_CONTINUE_REQ_BASE64:
    printf("base64 { %s }\n", cont_req->cr_data.cr_base64);
    break;
  }

  unindent();
  print_indent();
  printf("}\n");
}

static void mailimap_cont_req_or_resp_data_print(struct mailimap_cont_req_or_resp_data * cont_req_or_resp_data)
{
  print_indent();
  printf("cont-req-or-resp-data {\n");
  indent();

  switch (cont_req_or_resp_data->rsp_type) {
  case MAILIMAP_RESP_CONT_REQ:
    mailimap_continue_req_print(cont_req_or_resp_data->rsp_data.rsp_cont_req);
    break;
  case MAILIMAP_RESP_RESP_DATA:
    mailimap_response_data_print(cont_req_or_resp_data->rsp_data.rsp_resp_data);
    break;
  }

  unindent();
  print_indent();
  printf("}\n");
}

static void mailimap_response_tagged_print(struct mailimap_response_tagged *
					   tagged)
{
  print_indent();
  printf("response-tagged {\n");
  indent();

  print_indent();
  printf("tag { %s }\n", tagged->rsp_tag);
  mailimap_resp_cond_state_print(tagged->rsp_cond_state);

  unindent();
  print_indent();
  printf("}\n");
}

static void mailimap_response_fatal_print(struct mailimap_response_fatal *
					  fatal)
{
  print_indent();
  printf("response-fatal {\n");
  indent();

  mailimap_resp_cond_bye_print(fatal->rsp_bye);

  unindent();
  print_indent();
  printf("}\n");
}

static void mailimap_response_done_print(struct mailimap_response_done *
					 resp_done)
{
  print_indent();
  printf("response-done {\n");
  indent();

  switch (resp_done->rsp_type) {
  case MAILIMAP_RESP_DONE_TYPE_TAGGED:
    mailimap_response_tagged_print(resp_done->rsp_data.rsp_tagged);
    break;
  case MAILIMAP_RESP_DONE_TYPE_FATAL:
    mailimap_response_fatal_print(resp_done->rsp_data.rsp_fatal);
    break;
  }

  unindent();
  print_indent();
  printf("}\n");
}

void mailimap_response_print(struct mailimap_response * resp)
{
  clistiter * cur;

  print_indent();
  printf("response {\n");
  indent();

  for(cur = clist_begin(resp->rsp_cont_req_or_resp_data_list) ; cur != NULL ;
      cur = clist_next(cur)) {
    struct mailimap_cont_req_or_resp_data * resp;

    resp = clist_content(cur);

    mailimap_cont_req_or_resp_data_print(resp);
  }

  mailimap_response_done_print(resp->rsp_resp_done);

  unindent();
  print_indent();
  printf("}\n");
}

static void mailimap_resp_cond_auth_print(struct mailimap_resp_cond_auth *
					  cond_auth)
{
  print_indent();
  printf("resp-cond-auth {\n");
  indent();

  switch (cond_auth->rsp_type) {
  case MAILIMAP_RESP_COND_AUTH_OK:
    print_indent();
    printf("OK\n");
  case MAILIMAP_RESP_COND_AUTH_PREAUTH:
    print_indent();
    printf("PREAUTH\n");
  }
  mailimap_resp_text_print(cond_auth->rsp_text);

  unindent();
  print_indent();
  printf("}\n");
}

void mailimap_greeting_print(struct mailimap_greeting * greeting)
{
  print_indent();
  printf("greeting {\n");
  indent();

  switch(greeting->gr_type) {
  case MAILIMAP_GREETING_RESP_COND_AUTH:
    mailimap_resp_cond_auth_print(greeting->gr_data.gr_auth);
    break;
  case MAILIMAP_GREETING_RESP_COND_BYE:
    mailimap_resp_cond_bye_print(greeting->gr_data.gr_bye);
    break;
  }

  unindent();
  print_indent();
  printf("}\n");
}
#endif
