#include <libetpan/libetpan.h>
#include <string.h>
#include <stdlib.h>

#define DEST_CHARSET "iso-8859-1"

/* build a mime parameter */

static struct mailmime_parameter *
mailmime_param_new_with_data(char * name, char * value)
{
  char * param_name;
  char * param_value;
  struct mailmime_parameter * param;

  param_name = strdup(name);
  if (param_name == NULL)
    goto err;
  
  param_value = strdup(value);
  if (param_value == NULL)
    goto free_name;
  
  param = mailmime_parameter_new(param_name, param_value);
  if (param == NULL)
    goto free_value;
  
  return param;
  
 free_value:
  free(param_value);
 free_name:
  free(param_name);
 err:
  return NULL;
}


/* build sample fields */

static struct mailimf_fields * build_fields(void)
{
  struct mailimf_mailbox_list * from;
  struct mailimf_address_list * to;
  char * subject;
  int r;
  struct mailimf_fields * new_fields;

  /* subject field */

  subject = strdup("this is a sample");
  if (subject == NULL) {
    goto err;
  }

  /* from field */

  from = mailimf_mailbox_list_new_empty();
  if (from == NULL) {
    goto free_subject;
  }

  r = mailimf_mailbox_list_add_parse(from,
      "DINH Viet Hoa <hoa@sourceforge.net>");
  if (r != MAILIMF_NO_ERROR) {
    goto free_from;
  }

  /* to field */

  to = mailimf_address_list_new_empty();
  if (to == NULL) {
    goto free_from;
  }

  r = mailimf_address_list_add_parse(to,
      "Paul <claws@thewildbeast.co.uk>");
  if (r != MAILIMF_NO_ERROR) {
    goto free_to;
  }

  new_fields = mailimf_fields_new_with_data(from /* from */,
      NULL /* sender */, NULL /* reply-to */, 
      to, NULL /* cc */, NULL /* bcc */, NULL /* in-reply-to */,
      NULL /* references */,
      subject);
  if (new_fields == NULL)
    goto free_to;

  return new_fields;

 free_to:
  mailimf_address_list_free(to);
 free_from:
  mailimf_mailbox_list_free(from);
 free_subject:
  free(subject);
 err:
  return NULL;
}



/* text is a string, build a mime part containing this string */

static struct mailmime * build_body_text(char * text)
{
  struct mailmime_fields * mime_fields;
  struct mailmime * mime_sub;
  struct mailmime_content * content;
  struct mailmime_parameter * param;
  int r;

  /* text/plain part */

  mime_fields = mailmime_fields_new_encoding(MAILMIME_MECHANISM_8BIT);
  if (mime_fields == NULL) {
    goto err;
  }

  content = mailmime_content_new_with_str("text/plain");
  if (content == NULL) {
    goto free_fields;
  }

  param = mailmime_param_new_with_data("charset", DEST_CHARSET);
  if (param == NULL) {
    goto free_content;
  }

  r = clist_append(content->ct_parameters, param);
  if (r < 0) {
    mailmime_parameter_free(param);
    goto free_content;
  }

  mime_sub = mailmime_new_empty(content, mime_fields);
  if (mime_sub == NULL) {
    goto free_content;
  }

  r = mailmime_set_body_text(mime_sub, text, strlen(text));
  if (r != MAILIMF_NO_ERROR) {
    goto free_mime;
  }

  return mime_sub;

 free_mime:
  mailmime_free(mime_sub);
  goto err;
 free_content:
  mailmime_content_free(content);
 free_fields:
  mailmime_fields_free(mime_fields);
 err:
  return NULL;
}


/* build a mime part containing the given file */

static struct mailmime * build_body_file(char * filename)
{
  struct mailmime_fields * mime_fields;
  struct mailmime * mime_sub;
  struct mailmime_content * content;
  struct mailmime_parameter * param;
  char * dup_filename;
  int r;

  /* text/plain part */

  dup_filename = strdup(filename);
  if (dup_filename == NULL)
    goto err;

  mime_fields =
    mailmime_fields_new_filename(MAILMIME_DISPOSITION_TYPE_ATTACHMENT,
        dup_filename, MAILMIME_MECHANISM_BASE64);
  if (mime_fields == NULL)
    goto free_dup_filename;

  content = mailmime_content_new_with_str("text/plain");
  if (content == NULL) {
    goto free_fields;
  }

  param = mailmime_param_new_with_data("charset", DEST_CHARSET);
  if (param == NULL) {
    goto free_content;
  }

  r = clist_append(content->ct_parameters, param);
  if (r < 0) {
    mailmime_parameter_free(param);
    goto free_content;
  }

  mime_sub = mailmime_new_empty(content, mime_fields);
  if (mime_sub == NULL) {
    goto free_content;
  }

  dup_filename = strdup(filename);
  if (dup_filename == NULL)
    goto free_mime;

  r = mailmime_set_body_file(mime_sub, dup_filename);
  if (r != MAILIMF_NO_ERROR) {
    goto free_mime;
  }

  return mime_sub;

 free_mime:
  mailmime_free(mime_sub);
  goto err;
 free_content:
  mailmime_content_free(content);
 free_fields:
  mailmime_fields_free(mime_fields);
  goto err;
 free_dup_filename:
  free(dup_filename);
 err:
  return NULL;
}


/* build an empty message */

static struct mailmime * build_message(struct mailimf_fields * fields)
{
  struct mailmime * mime;
  
  /* message */
  
  mime = mailmime_new_message_data(NULL);
  if (mime == NULL) {
    goto err;
  }

  mailmime_set_imf_fields(mime, fields);

  return mime;

 err:
  return NULL;
}


int main(int argc, char ** argv)
{
  struct mailimf_fields * fields;
  char * text;
  char * filename;
  struct mailmime * message;
  struct mailmime * text_part;
  struct mailmime * file_part;
  int r;
  int col;

  if (argc < 3) {
    printf("syntax: compose-msg \"text\" filename\n");
    return 1;
  }

  fields = build_fields();
  if (fields == NULL)
    goto err;

  message = build_message(fields);
  if (message == NULL)
    goto free_fields;

  text = argv[1];
  text_part = build_body_text(text);
  if (text_part == NULL)
    goto free_message;

  filename = argv[2];
  file_part = build_body_file(filename);
  if (file_part == NULL)
    goto free_text;

  r = mailmime_smart_add_part(message, text_part);
  if (r != MAILIMF_NO_ERROR)
    goto free_file;

  r = mailmime_smart_add_part(message, file_part);
  if (r != MAILIMF_NO_ERROR)
    goto free_file_alone;
  
  col = 0;
  mailmime_write(stdout, &col, message);

  mailmime_free(message);

  return 0;

 free_file_alone:
  mailmime_free(file_part);
  goto free_text;
 free_file:
  mailmime_free(file_part);
 free_text:
  mailmime_free(text_part);
 free_message:
  mailmime_free(message);
  goto err;
 free_fields:
  mailimf_fields_free(fields);
 err:
  printf("error memory\n");
  return 1;
}
