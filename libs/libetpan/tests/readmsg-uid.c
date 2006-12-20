#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include <libetpan/libetpan.h>

#include "option-parser.h"

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
  
  while (optind < argc) {
    mailmessage * msg;
    char * data;
    size_t size;
    char * uid;
    
    uid = argv[optind];
    
    r = mailsession_get_message_by_uid(folder->fld_session, uid, &msg);
    if (r != MAIL_NO_ERROR) {
      printf("** message %s not found **\n", uid);
      optind ++;
      continue;
    }

    r = mailmessage_fetch(msg, &data, &size);
    if (r != MAIL_NO_ERROR) {
      printf("** message %s not found - %s **\n", uid,
          maildriver_strerror(r));
      mailmessage_free(msg);
      optind ++;
      continue;
    }

    fwrite(data, 1, size, stdout);
    
    mailmessage_fetch_result_free(msg, data);

    mailmessage_free(msg);

    optind ++;
  }

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
