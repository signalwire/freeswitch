#include "frm-common.h"

#include <string.h>
#include <stdlib.h>

#define DEST_CHARSET "iso-8859-1"

/* get part of the from field to display */

void get_from_value(struct mailimf_single_fields * fields,
    char ** from, int * is_addr)
{
  struct mailimf_mailbox * mb;
  
  if (fields->fld_from == NULL) {
    * from = NULL;
    * is_addr = 0;
    return;
  }

  if (clist_isempty(fields->fld_from->frm_mb_list->mb_list)) {
    * from = NULL;
    * is_addr = 0;
    return;
  }

  mb = clist_begin(fields->fld_from->frm_mb_list->mb_list)->data;

  if (mb->mb_display_name != NULL) {
    * from = mb->mb_display_name;
    * is_addr = 0;
  }
  else {
    * from = mb->mb_addr_spec;
    * is_addr = 1;
  }
}

/* remove all CR and LF of a string and replace them with SP */

void strip_crlf(char * str)
{
  char * p;
  
  for(p = str ; * p != '\0' ; p ++) {
    if ((* p == '\n') || (* p == '\r'))
      * p = ' ';
  }
}

#define MAX_OUTPUT 81

/* display information for one message */

void print_mail_info(char * prefix, mailmessage * msg)
{
  char * from;
  char * subject;
  char * decoded_from;
  char * decoded_subject;
  size_t cur_token;
  int r;
  int is_addr;
  char * dsp_from;
  char * dsp_subject;
  char output[MAX_OUTPUT];
  struct mailimf_single_fields single_fields;
  
  is_addr = 0;
  from = NULL;
  subject = NULL;

  decoded_subject = NULL;
  decoded_from = NULL;

  /* from field */
  
  if (msg->msg_fields != NULL)
    mailimf_single_fields_init(&single_fields, msg->msg_fields);
  else
    memset(&single_fields, 0, sizeof(single_fields));
  
  get_from_value(&single_fields, &from, &is_addr);
  
  if (from == NULL)
    decoded_from = NULL;
  else {
    if (!is_addr) {
      cur_token = 0;
      r = mailmime_encoded_phrase_parse(DEST_CHARSET,
          from, strlen(from),
          &cur_token, DEST_CHARSET,
          &decoded_from);
      if (r != MAILIMF_NO_ERROR) {
        decoded_from = strdup(from);
        if (decoded_from == NULL)
          goto err;
      }
    }
    else {
      decoded_from = strdup(from);
      if (decoded_from == NULL) {
        goto err;
      }
    }
  }

  if (decoded_from == NULL)
    dsp_from = "";
  else {
    dsp_from = decoded_from;
    strip_crlf(dsp_from);
  }

  /* subject */

  if (single_fields.fld_subject != NULL)
    subject = single_fields.fld_subject->sbj_value;
    
  if (subject == NULL)
    decoded_subject = NULL;
  else {
    cur_token = 0;
    r = mailmime_encoded_phrase_parse(DEST_CHARSET,
        subject, strlen(subject),
        &cur_token, DEST_CHARSET,
        &decoded_subject);
    if (r != MAILIMF_NO_ERROR) {
      decoded_subject = strdup(subject);
      if (decoded_subject == NULL)
        goto free_from;
    }
  }

  if (decoded_subject == NULL)
    dsp_subject = "";
  else {
    dsp_subject = decoded_subject;
    strip_crlf(dsp_subject);
  }

  snprintf(output, MAX_OUTPUT, "%3i: %-21.21s %s%-53.53s",
      msg->msg_index, dsp_from, prefix, dsp_subject);
  
  printf("%s\n", output);

  if (decoded_subject != NULL)
    free(decoded_subject);
  if (decoded_from != NULL)
    free(decoded_from);

  return;

 free_from:
  if (decoded_from)
    free(decoded_from);
 err:
  {}
}
