#include "readmsg-common.h"

#include <sys/stat.h>
#ifndef _MSC_VER
#	include <sys/mman.h>
#	include <unistd.h>
#endif
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>

/* returns TRUE is given MIME part is a text part */

int etpan_mime_is_text(struct mailmime * build_info)
{
  if (build_info->mm_type == MAILMIME_SINGLE) {
    if (build_info->mm_content_type != NULL) {
      if (build_info->mm_content_type->ct_type->tp_type ==
          MAILMIME_TYPE_DISCRETE_TYPE) {
        if (build_info->mm_content_type->ct_type->tp_data.tp_discrete_type->dt_type ==
            MAILMIME_DISCRETE_TYPE_TEXT)
          return 1;
      }
    }
    else
      return 1;
  }

  return 0;
}


/* display content type */

int show_part_info(FILE * f,
    struct mailmime_single_fields * mime_fields,
    struct mailmime_content * content)
{
  char * description;
  char * filename;
  int col;
  int r;

  description = mime_fields->fld_description;
  filename = mime_fields->fld_disposition_filename;

  col = 0;

  r = fprintf(f, " [ Part ");
  if (r < 0)
    goto err;

  if (content != NULL) {
    r = mailmime_content_type_write(f, &col, content);
    if (r != MAILIMF_NO_ERROR)
      goto err;
  }

  if (filename != NULL) {
    r = fprintf(f, " (%s)", filename);
    if (r < 0)
      goto err;
  }

  if (description != NULL) {
    r = fprintf(f, " : %s", description);
    if (r < 0)
      goto err;
  }

  r = fprintf(f, " ]\n\n");
  if (r < 0)
    goto err;

  return NO_ERROR;
  
 err:
  return ERROR_FILE;
}

/*
  fetch the data of the mailmime_data structure whether it is a file
  or a string.

  data must be freed with mmap_string_unref()
*/

#if 0
static int fetch_data(struct mailmime_data * data,
    char ** result, size_t * result_len)
{
  int fd;
  int r;
  char * text;
  struct stat buf;
  int res;
  MMAPString * mmapstr;

  switch (data->dt_type) {
  case MAILMIME_DATA_TEXT:
    mmapstr = mmap_string_new_len(data->dt_data.dt_text.dt_data,
        data->dt_data.dt_text.dt_length);
    if (mmapstr == NULL) {
      res = ERROR_MEMORY;
      goto err;
    }

    * result = mmapstr->str;
    * result_len = mmapstr->len;

    return NO_ERROR;

  case MAILMIME_DATA_FILE:
    fd = open(data->dt_data.dt_filename, O_RDONLY);
    if (fd < 0) {
      res = ERROR_FILE;
      goto err;
    }

    r = fstat(fd, &buf);
    if (r < 0) {
      res = ERROR_FILE;
      goto close;
    }

    if (buf.st_size != 0) {
      text = mmap(NULL, buf.st_size, PROT_READ, MAP_SHARED, fd, 0);
      if (text == (char *)MAP_FAILED) {
	res = ERROR_FILE;
	goto close;
      }

      mmapstr = mmap_string_new_len(text, buf.st_size);
      if (mmapstr == NULL) {
        res = r;
        goto unmap;
      }
      
      munmap(text, buf.st_size);
    }
    else {
      mmapstr = mmap_string_new("");
      if (mmapstr == NULL) {
        res = r;
        goto close;
      }
    }

    close(fd);

    * result = mmapstr->str;
    * result_len = mmapstr->len;

    return NO_ERROR;

  default:
    return ERROR_INVAL;
  }
  
 unmap:
  munmap(text, buf.st_size);
 close:
  close(fd);
 err:
  return res;
}
#endif

/* fetch message and decode if it is base64 or quoted-printable */

int etpan_fetch_message(mailmessage * msg_info,
    struct mailmime * mime_part,
    struct mailmime_single_fields * fields,
    char ** result, size_t * result_len)
{
  char * data;
  size_t len;
  int r;
  int encoding;
  char * decoded;
  size_t decoded_len;
  size_t cur_token;
  int res;
  int encoded;

  encoded = 0;

  r = mailmessage_fetch_section(msg_info,
      mime_part, &data, &len);
  if (r != MAIL_NO_ERROR) {
    res = ERROR_FETCH;
    goto err;
  }

  encoded = 1;

  /* decode message */

  if (encoded) {
    if (fields->fld_encoding != NULL)
      encoding = fields->fld_encoding->enc_type;
    else 
      encoding = MAILMIME_MECHANISM_8BIT;
  }
  else {
    encoding = MAILMIME_MECHANISM_8BIT;
  }

  cur_token = 0;
  r = mailmime_part_parse(data, len, &cur_token,
			  encoding, &decoded, &decoded_len);
  if (r != MAILIMF_NO_ERROR) {
    res = ERROR_FETCH;
    goto free; 
  }

  mailmessage_fetch_result_free(msg_info, data);
  
  * result = decoded;
  * result_len = decoded_len;
  
  return NO_ERROR;
  
 free:
  mailmessage_fetch_result_free(msg_info, data);
 err:
  return res;
}


/* fetch fields */

struct mailimf_fields * fetch_fields(mailmessage * msg_info,
    struct mailmime * mime)
{
  char * data;
  size_t len;
  int r;
  size_t cur_token;
  struct mailimf_fields * fields;

  r = mailmessage_fetch_section_header(msg_info, mime, &data, &len);
  if (r != MAIL_NO_ERROR)
    return NULL;

  cur_token = 0;
  r = mailimf_fields_parse(data, len, &cur_token, &fields);
  if (r != MAILIMF_NO_ERROR) {
    mailmessage_fetch_result_free(msg_info, data);
    return NULL;
  }

  mailmessage_fetch_result_free(msg_info, data);

  return fields;
}



#define MAX_MAIL_COL 72

/* write decoded mailbox */

static int
etpan_mailbox_write(FILE * f, int * col,
    struct mailimf_mailbox * mb)
{
  int r;

  if (* col > 1) {
    
    if (* col + strlen(mb->mb_addr_spec) >= MAX_MAIL_COL) {
      r = mailimf_string_write(f, col, "\r\n ", 3);
      if (r != MAILIMF_NO_ERROR)
	return ERROR_FILE;
      * col = 1;
    }
  }
  
  if (mb->mb_display_name) {
    char * decoded_from;
    size_t cur_token;

    cur_token = 0;
    r = mailmime_encoded_phrase_parse(DEST_CHARSET,
        mb->mb_display_name, strlen(mb->mb_display_name),
        &cur_token, DEST_CHARSET,
        &decoded_from);
    if (r != MAILIMF_NO_ERROR) {
      decoded_from = strdup(mb->mb_display_name);
      if (decoded_from == NULL)
        return ERROR_MEMORY;
    }

    r = mailimf_quoted_string_write(f, col, decoded_from,
        strlen(decoded_from));
    if (r != MAILIMF_NO_ERROR) {
      free(decoded_from);
      return ERROR_FILE;
    }

    if (* col > 1) {
      
      if (* col + strlen(decoded_from) + 3 >= MAX_MAIL_COL) {
	r = mailimf_string_write(f, col, "\r\n ", 3);
	if (r != MAILIMF_NO_ERROR) {
          free(decoded_from);
	  return r;
        }
	* col = 1;
      }
    }

    free(decoded_from);
    
    r = mailimf_string_write(f, col, " <", 2);
    if (r != MAILIMF_NO_ERROR)
      return ERROR_FILE;

    r = mailimf_string_write(f, col,
        mb->mb_addr_spec, strlen(mb->mb_addr_spec));
    if (r != MAILIMF_NO_ERROR)
      return ERROR_FILE;

    r = mailimf_string_write(f, col, ">", 1);
    if (r != MAILIMF_NO_ERROR)
      return ERROR_FILE;
  }
  else {
    r = mailimf_string_write(f, col,
        mb->mb_addr_spec, strlen(mb->mb_addr_spec));
    if (r != MAILIMF_NO_ERROR)
      return ERROR_FILE;
  }


  return NO_ERROR;

}

/* write decoded mailbox list */

int
etpan_mailbox_list_write(FILE * f, int * col,
    struct mailimf_mailbox_list * mb_list)
{
  clistiter * cur;
  int r;
  int first;

  first = 1;

  for(cur = clist_begin(mb_list->mb_list) ; cur != NULL ;
      cur = clist_next(cur)) {
    struct mailimf_mailbox * mb;

    mb = cur->data;

    if (!first) {
      r = mailimf_string_write(f, col, ", ", 2);
      if (r != MAILIMF_NO_ERROR)
	return ERROR_FILE;
    }
    else {
      first = 0;
    }

    r = etpan_mailbox_write(f, col, mb);
    if (r != NO_ERROR)
      return r;
  }

  return NO_ERROR;
}

/* write decoded group */

static int
etpan_group_write(FILE * f, int * col,
    struct mailimf_group * group)
{
  int r;

  r = mailimf_string_write(f, col, group->grp_display_name,
			   strlen(group->grp_display_name));
  if (r != MAILIMF_NO_ERROR)
    return ERROR_FILE;

  r = mailimf_string_write(f, col, ": ", 2);
  if (r != MAILIMF_NO_ERROR)
    return ERROR_FILE;
  
  if (group->grp_mb_list != NULL) {
    r = etpan_mailbox_list_write(f, col, group->grp_mb_list);
    if (r != NO_ERROR)
      return r;
  }

  r = mailimf_string_write(f, col, ";", 1);
  if (r != MAILIMF_NO_ERROR)
    return ERROR_FILE;

  return NO_ERROR;
}

/* write decoded address */

int
etpan_address_write(FILE * f, int * col,
    struct mailimf_address * addr)
{
  int r;

  switch(addr->ad_type) {
  case MAILIMF_ADDRESS_MAILBOX:
    r = etpan_mailbox_write(f, col, addr->ad_data.ad_mailbox);
    if (r != NO_ERROR)
      return r;

    break;

  case MAILIMF_ADDRESS_GROUP:
    r = etpan_group_write(f, col, addr->ad_data.ad_group);
    if (r != NO_ERROR)
      return r;
    
    break;
  }

  return MAILIMF_NO_ERROR;
}

/* write decoded address list */

int
etpan_address_list_write(FILE * f, int * col,
    struct mailimf_address_list * addr_list)
{
  clistiter * cur;
  int r;
  int first;

  first = 1;

  for(cur = clist_begin(addr_list->ad_list) ; cur != NULL ;
      cur = clist_next(cur)) {
    struct mailimf_address * addr;

    addr = clist_content(cur);

    if (!first) {
      r = mailimf_string_write(f, col, ", ", 2);
      if (r != MAILIMF_NO_ERROR)
	return ERROR_FILE;
    }
    else {
      first = 0;
    }

    r = etpan_address_write(f, col, addr);
    if (r != NO_ERROR)
      return r;
  }

  return NO_ERROR;
}

/* write decoded subject */

static int etpan_subject_write(FILE * f, int * col,
    char * subject)
{
  int r;
  char * decoded_subject;
  size_t cur_token;
  
  r = mailimf_string_write(f, col, "Subject: ", 9);
  if (r != MAILIMF_NO_ERROR) {
    return ERROR_FILE;
  }
  
  cur_token = 0;
  r = mailmime_encoded_phrase_parse(DEST_CHARSET,
      subject, strlen(subject),
      &cur_token, DEST_CHARSET,
      &decoded_subject);
  if (r != MAILIMF_NO_ERROR) {
    decoded_subject = strdup(subject);
    if (decoded_subject == NULL)
      return ERROR_MEMORY;
  }
  
  r = mailimf_string_write(f, col, decoded_subject, strlen(decoded_subject));
  if (r != MAILIMF_NO_ERROR) {
    free(decoded_subject);
    return ERROR_FILE;
  }

  free(decoded_subject);

  r = mailimf_string_write(f, col, "\r\n", 2);
  if (r != MAILIMF_NO_ERROR) {
    return ERROR_FILE;
  }
  * col = 0;

  return NO_ERROR;
}

/* write decoded fields */

int fields_write(FILE * f, int * col,
    struct mailimf_fields * fields)
{
  clistiter * cur;
  int r;
  
  for(cur = clist_begin(fields->fld_list) ; cur != NULL ;
      cur = clist_next(cur)) {
    struct mailimf_field * field;

    field = clist_content(cur);

    switch (field->fld_type) {
    case MAILIMF_FIELD_FROM:
      r = mailimf_string_write(f, col, "From: ", 6);
      if (r != MAILIMF_NO_ERROR)
        goto err;
      
      r = etpan_mailbox_list_write(f, col,
          field->fld_data.fld_from->frm_mb_list);
      if (r != NO_ERROR)
        goto err;

      r = mailimf_string_write(f, col, "\r\n", 2);
      if (r != MAILIMF_NO_ERROR)
        goto err;
      * col = 0;

      break;

    case MAILIMF_FIELD_REPLY_TO:
      r = mailimf_string_write(f, col, "Reply-To: ", 10);
      if (r != MAILIMF_NO_ERROR)
        goto err;
      
      r = etpan_address_list_write(f, col,
          field->fld_data.fld_reply_to->rt_addr_list);
      if (r != NO_ERROR)
        goto err;

      r = mailimf_string_write(f, col, "\r\n", 2);
      if (r != MAILIMF_NO_ERROR)
        goto err;
      * col = 0;

      break;

    case MAILIMF_FIELD_TO:
      r = mailimf_string_write(f, col, "To: ", 4);
      if (r != MAILIMF_NO_ERROR)
        goto err;
      
      r = etpan_address_list_write(f, col,
          field->fld_data.fld_to->to_addr_list);
      if (r != NO_ERROR)
        goto err;

      r = mailimf_string_write(f, col, "\r\n", 2);
      if (r != MAILIMF_NO_ERROR)
        goto err;
      * col = 0;

      break;

    case MAILIMF_FIELD_CC:
      r = mailimf_string_write(f, col, "Cc: ", 4);
      if (r != MAILIMF_NO_ERROR)
        goto err;
      
      r = etpan_address_list_write(f, col,
          field->fld_data.fld_cc->cc_addr_list);
      if (r != NO_ERROR)
        goto err;

      r = mailimf_string_write(f, col, "\r\n", 2);
      if (r != MAILIMF_NO_ERROR)
        goto err;
      * col = 0;

      break;

    case MAILIMF_FIELD_BCC:
      r = mailimf_string_write(f, col, "Bcc: ", 10);
      if (r != MAILIMF_NO_ERROR)
        goto err;
      
      if (field->fld_data.fld_bcc->bcc_addr_list != NULL) {
        r = etpan_address_list_write(f, col,
            field->fld_data.fld_bcc->bcc_addr_list);
        if (r != NO_ERROR)
          goto err;
      }

      r = mailimf_string_write(f, col, "\r\n", 2);
      if (r != MAILIMF_NO_ERROR)
        goto err;
      * col = 0;

      break;

    case MAILIMF_FIELD_SUBJECT:
      r = etpan_subject_write(f, col, field->fld_data.fld_subject->sbj_value);
      if (r != MAILIMF_NO_ERROR)
        goto err;
      break;

    case MAILIMF_FIELD_RESENT_FROM:
      r = mailimf_string_write(f, col, "Resent-From: ", 13);
      if (r != MAILIMF_NO_ERROR)
        goto err;
      
      r = etpan_mailbox_list_write(f, col,
          field->fld_data.fld_resent_from->frm_mb_list);
      if (r != NO_ERROR)
        goto err;

      r = mailimf_string_write(f, col, "\r\n", 2);
      if (r != MAILIMF_NO_ERROR)
        goto err;
      * col = 0;
      break;

    case MAILIMF_FIELD_RESENT_TO:
      r = mailimf_string_write(f, col, "Resent-To: ", 11);
      if (r != MAILIMF_NO_ERROR)
        goto err;
      
      r = etpan_address_list_write(f, col,
          field->fld_data.fld_resent_to->to_addr_list);
      if (r != NO_ERROR)
        goto err;

      r = mailimf_string_write(f, col, "\r\n", 2);
      if (r != MAILIMF_NO_ERROR)
        goto err;
      * col = 0;

      break;
    case MAILIMF_FIELD_RESENT_CC:
      r = mailimf_string_write(f, col, "Resent-Cc: ", 11);
      if (r != MAILIMF_NO_ERROR)
        goto err;
      
      r = etpan_address_list_write(f, col,
          field->fld_data.fld_resent_cc->cc_addr_list);
      if (r != NO_ERROR)
        goto err;

      r = mailimf_string_write(f, col, "\r\n", 2);
      if (r != MAILIMF_NO_ERROR)
        goto err;
      * col = 0;

      break;
    case MAILIMF_FIELD_RESENT_BCC:
      r = mailimf_string_write(f, col, "Resent-Bcc: ", 12);
      if (r != MAILIMF_NO_ERROR)
        goto err;
      
      if (field->fld_data.fld_resent_bcc->bcc_addr_list != NULL) {
        r = etpan_address_list_write(f, col,
            field->fld_data.fld_resent_bcc->bcc_addr_list);
        if (r != NO_ERROR)
          goto err;
      }

      r = mailimf_string_write(f, col, "\r\n", 2);
      if (r != MAILIMF_NO_ERROR)
        goto err;
      * col = 0;

      break;

    case MAILIMF_FIELD_ORIG_DATE:
    case MAILIMF_FIELD_RESENT_DATE:
      r = mailimf_field_write(f, col, field);
      if (r != MAILIMF_NO_ERROR)
        goto err;
      break;

    case MAILIMF_FIELD_OPTIONAL_FIELD:
      if ((strcasecmp(field->fld_data.fld_optional_field->fld_name,
               "X-Mailer") == 0)
          || (strncasecmp(field->fld_data.fld_optional_field->fld_name,
                  "Resent-", 7) == 0)
          || (strcasecmp(field->fld_data.fld_optional_field->fld_name,
                  "Newsgroups") == 0)
          || (strcasecmp(field->fld_data.fld_optional_field->fld_name,
                  "Followup-To") == 0)
          || (strcasecmp(field->fld_data.fld_optional_field->fld_name,
                  "User-Agent") == 0)) {
        r = mailimf_field_write(f, col, field);
        if (r != MAILIMF_NO_ERROR)
          goto err;
      }
      break;

    case MAILIMF_FIELD_MESSAGE_ID:
    case MAILIMF_FIELD_SENDER:
    case MAILIMF_FIELD_IN_REPLY_TO:
    case MAILIMF_FIELD_REFERENCES:
    default:
      break;
    }
  }

  return NO_ERROR;

 err:
  return ERROR_FILE;
}
