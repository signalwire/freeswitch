#include "option-parser.h"
#include "frm-common.h"

#include <libetpan/libetpan.h>

#include <stdlib.h>
#include <string.h>

#define DEST_CHARSET "iso-8859-1"

/* display tree */

static void
display_sub_tree(MMAPString * prefix,
    struct mailmessage_tree * msg_tree,
    int level, int has_next, unsigned int * pcount)
{
  carray * list;
  uint32_t cur;
  
  if (msg_tree->node_msg != NULL) {
    print_mail_info(prefix->str, msg_tree->node_msg);
    (* pcount) ++;
  }

  list = msg_tree->node_children;
  
  if (carray_count(list) != 0) {
    char old_prefix[2];
	    
    if (level > 1) {
      memcpy(old_prefix, prefix->str + prefix->len - 2, 2);
      if (has_next)
        memcpy(prefix->str + prefix->len - 2, "| ", 2);
      else
        memcpy(prefix->str + prefix->len - 2, "  ", 2);
    }
    for(cur = 0 ; cur < carray_count(list) ; cur ++) {
      int sub_has_next;
      
      if (cur != carray_count(list) - 1) {
	if (level > 0) {
	  if (mmap_string_append(prefix, "+-") == NULL)
            return;
        }
	sub_has_next = 1;
      }
      else {
	if (level > 0) {
	  if (mmap_string_append(prefix, "\\-") == NULL)
            return;
        }
	sub_has_next = 0;
      }

      display_sub_tree(prefix, carray_get(list, cur),
          level + 1, sub_has_next, pcount);

      if (mmap_string_truncate(prefix, prefix->len - 2) == NULL) {
        return;
      }
    }
    if (level > 1) {
      memcpy(prefix->str + prefix->len - 2, old_prefix, 2);
    }
  }
}

static void display_tree(struct mailmessage_tree * env_tree,
    unsigned int * pcount)
{
  MMAPString * prefix;

  prefix = mmap_string_new("");
  if (prefix == NULL)
    return;

  display_sub_tree(prefix, env_tree, 0, 0, pcount);

  mmap_string_free(prefix);
}

/* get the message list and display it */

static void print_message_list(mailsession * session)
{
  int r;
  struct mailmessage_list * env_list;
  struct mailmessage_tree * env_tree;
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

  /* build threads */

  r = mail_build_thread(MAIL_THREAD_REFERENCES_NO_SUBJECT,
      DEST_CHARSET,
      env_list, &env_tree,
      mailthread_tree_timecomp);
  if (r != MAIL_NO_ERROR) {
    printf("can't build tree\n");
    goto free_msg_list;
  }

  /* display message tree */
  
  count = 0;
  display_tree(env_tree, &count);

  printf("  %i messages\n", count);

  /* free structure */

  mailmessage_tree_free_recursive(env_tree);
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

