#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <libetpan/libetpan.h>

#include "option-parser.h"
#include "readmsg-common.h"

/* write content to the given filename */

static int etpan_write_data(char * filename, char * data, size_t len)
{
  size_t write_len;
  FILE * f;
  int res;
  mode_t old_umask;
  
  old_umask = umask(0077);
  f = fopen(filename, "w");
  umask(old_umask);
  if (f == NULL) {
    res = ERROR_FILE;
    goto err;
  }

  write_len = fwrite(data, 1, len, f);
  if (write_len < len) {
    res = ERROR_FILE;
    goto close;
  }

  fclose(f);

  return NO_ERROR;

 close:
  fclose(f);
 err:
  return res;
}


/* save attachment */

static int save_mime_content(mailmessage * msg_info,
    struct mailmime * mime_part)
{
  char * body;
  size_t body_len;
  int r;
  char * filename;
  struct mailmime_single_fields fields;
  int res;

  memset(&fields, 0, sizeof(struct mailmime_single_fields));
  if (mime_part->mm_mime_fields != NULL)
    mailmime_single_fields_init(&fields, mime_part->mm_mime_fields,
        mime_part->mm_content_type);

  filename = fields.fld_disposition_filename;

  if (filename == NULL)
    filename = fields.fld_content_name;

  if (filename == NULL)
    return ERROR_INVAL;

  r = etpan_fetch_message(msg_info, mime_part, &fields, &body, &body_len);
  if (r != NO_ERROR) {
    res = r;
    goto err;
  }

  printf("writing %s, %i bytes\n", filename, body_len);

  r = etpan_write_data(filename, body, body_len);
  if (r != NO_ERROR) {
    res = r;
    goto free;
  }

  mailmime_decoded_part_free(body);

  return NO_ERROR;

 free:
  mailmime_decoded_part_free(body);
 err:
  return res;
}



/* fetch attachments */

static int etpan_fetch_mime(FILE * f, mailmessage * msg_info,
    struct mailmime * mime)
{
  int r;
  clistiter * cur;
  struct mailmime_single_fields fields;
  int res;

  memset(&fields, 0, sizeof(struct mailmime_single_fields));
  if (mime->mm_mime_fields != NULL)
    mailmime_single_fields_init(&fields, mime->mm_mime_fields,
        mime->mm_content_type);

  switch(mime->mm_type) {
  case MAILMIME_SINGLE:
    save_mime_content(msg_info, mime);

    break;
    
  case MAILMIME_MULTIPLE:

    for(cur = clist_begin(mime->mm_data.mm_multipart.mm_mp_list) ;
        cur != NULL ; cur = clist_next(cur)) {
      
      r = etpan_fetch_mime(f, msg_info, clist_content(cur));
      if (r != NO_ERROR) {
        res = r;
        goto err;
      }
    }

    break;
      
  case MAILMIME_MESSAGE:

    if (mime->mm_data.mm_message.mm_msg_mime != NULL) {
      r = etpan_fetch_mime(f, msg_info, mime->mm_data.mm_message.mm_msg_mime);
      if (r != NO_ERROR) {
        res = r;
        goto err;
      }
    }

    break;
  }

  return NO_ERROR;

 err:
  return res;
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
  
  while (optind < argc) {
    mailmessage * msg;
    uint32_t msg_num;
    struct mailmime * mime;

    msg_num = strtoul(argv[optind], NULL, 10);
    
    r = mailsession_get_message(folder->fld_session, msg_num, &msg);
    if (r != MAIL_NO_ERROR) {
      printf("** message %i not found ** - %s\n", msg_num,
          maildriver_strerror(r));
      optind ++;
      continue;
    }

    r = mailmessage_get_bodystructure(msg, &mime);
    if (r != MAIL_NO_ERROR) {
      printf("** message %i not found - %s **\n", msg_num,
          maildriver_strerror(r));
      mailmessage_free(msg);
      optind ++;
      continue;
    }
    
    r = etpan_fetch_mime(stdout, msg, mime);

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
