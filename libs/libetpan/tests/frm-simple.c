#include "option-parser.h"
#include "frm-common.h"

#include <libetpan/libetpan.h>

#include <stdlib.h>
#include <string.h>

#define DEST_CHARSET "iso-8859-1"

#define MAX_OUTPUT 81

/* display information for one message */

static void simple_print_mail_info(mailmessage * msg)
{
  char * from;
  char * subject;
  int is_addr;
  char * dsp_from;
  char * dsp_subject;
  char output[MAX_OUTPUT];
  struct mailimf_single_fields single_fields;
  
  is_addr = 0;
  from = NULL;
  subject = NULL;
  
  if (msg->msg_fields != NULL)
    mailimf_single_fields_init(&single_fields, msg->msg_fields);
  else
    memset(&single_fields, 0, sizeof(single_fields));
  
  /* from field */
 
  get_from_value(&single_fields, &from, &is_addr);

  if (from == NULL)
    dsp_from = strdup("");
  else
    dsp_from = strdup(from);
  if (dsp_from == NULL)
    goto err;

  strip_crlf(dsp_from);

  /* subject */
  
  if (single_fields.fld_subject != NULL)
    subject = single_fields.fld_subject->sbj_value;
    
  if (subject == NULL)
    dsp_subject = strdup("");
  else
    dsp_subject = strdup(subject);
  
  if (dsp_subject == NULL)
    goto free_from;

  strip_crlf(dsp_subject);

  snprintf(output, MAX_OUTPUT, "%3i: %-21.21s %-53.53s\n",
      msg->msg_index % 1000, dsp_from, dsp_subject);
  
  printf("%s\n", output);

  free(dsp_subject);
  free(dsp_from);

  return;

 free_from:
  free(dsp_from);
 err:
  {}
}

/* get the message list and display it */

static void print_message_list(mailsession * session)
{
  int r;
  uint32_t i;
  struct mailmessage_list * env_list;
  unsigned int count;
  
  /* get the list of messages numbers of the folder */

  r = mailsession_get_messages_list(session, &env_list);
  if (r != MAIL_NO_ERROR) {
    printf("error message list\n");
    goto err;
  }

  /* get fields content of these messages */

  r = mailsession_get_envelopes_list(session, env_list);
  if (r != MAIL_NO_ERROR) {
    printf("error envelopes list\n");
    goto free_msg_list;
  }

  /* display all the messages */
  
  count = 0;
  for(i = 0 ; i < carray_count(env_list->msg_tab) ; i ++) {
    mailmessage * msg;
    
    msg = carray_get(env_list->msg_tab, i);

    if (msg->msg_fields == NULL) {
      printf("could not fetch envelope of message %i\n", i);
    }
    else {
      simple_print_mail_info(msg);
      count ++;
    }
  }
  printf("  %i messages\n", count);

  /* free structure */

  mailmessage_list_free(env_list);

  return;

 free_msg_list:
  mailmessage_list_free(env_list);
 err:
  {}
}

int main(int argc, char ** argv)
{
  int r;
  int driver;
  char * server;
  int port;
  int connection_type;
  char * user;
  char * password;
  int auth_type;
  char * path;
  char * cache_directory;
  char * flags_directory;
  struct mailstorage * storage;
  int cached;
  struct mailfolder * folder;

  /* get options */

  r = parse_options(argc, argv,
      &driver, &server, &port, &connection_type,
      &user, &password, &auth_type,
      &path, &cache_directory, &flags_directory);

  cached = (cache_directory != NULL);

  /* build the storage structure */

  storage = mailstorage_new(NULL);
  if (storage == NULL) {
    printf("error initializing storage\n");
    goto free_opt;
  }
  
  r = init_storage(storage, driver, server, port, connection_type,
      user, password, auth_type, path, cache_directory, flags_directory);
  if (r != MAIL_NO_ERROR) {
    printf("error initializing storage\n");
    goto free_opt;
  }
  
  /* get the folder structure */

  folder = mailfolder_new(storage, path, NULL);
  if (folder == NULL) {
    printf("error initializing folder\n");
    goto free_storage;
  }

  r = mailfolder_connect(folder);
  if (r != MAIL_NO_ERROR) {
    printf("error initializing folder\n");
    goto free_folder;
  }

  /* get and display the list of messages */

  print_message_list(folder->fld_session);

  mailfolder_free(folder);
  mailstorage_free(storage);

  if (server != NULL)
    free(server);
  if (user != NULL)
    free(user);
  if (password != NULL)
    free(password);
  if (path != NULL)
    free(path);
  if (cache_directory != NULL)
    free(cache_directory);
  if (flags_directory != NULL)
    free(flags_directory);

  return 0;

 free_folder:
  mailfolder_free(folder);
 free_storage:
  mailstorage_free(storage);
 free_opt:
  if (server != NULL)
    free(server);
  if (user != NULL)
    free(user);
  if (password != NULL)
    free(password);
  if (path != NULL)
    free(path);
  if (cache_directory != NULL)
    free(cache_directory);
  if (flags_directory != NULL)
    free(flags_directory);
  return -1;
}

