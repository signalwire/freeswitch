#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <string.h>
#include <stdlib.h>

#include <libetpan/charconv.h>
#include <libetpan/libetpan.h>

#include "option-parser.h"
#include "readmsg-common.h"
#ifdef _MSC_VER
#	include "../src/bsd/getopt.h"
#endif

/* render message */

static int etpan_render_mime(FILE * f, mailmessage * msg_info,
    struct mailmime * mime)
{
  int r;
  clistiter * cur;
  int col;
  int text;
  int show;
  struct mailmime_single_fields fields;
  int res;

  mailmime_single_fields_init(&fields, mime->mm_mime_fields,
      mime->mm_content_type);
  
  text = etpan_mime_is_text(mime);
  
  r = show_part_info(f, &fields, mime->mm_content_type);
  if (r != NO_ERROR) {
    res = r;
    goto err;
  }

  switch(mime->mm_type) {
  case MAILMIME_SINGLE:
    show = 0;
    if (text)
      show = 1;
    
    if (show) {
      char * data;
      size_t len;
      char * converted;
      size_t converted_len;
      char * source_charset;
      size_t write_len;

      /* viewable part */
          
      r = etpan_fetch_message(msg_info, mime,
          &fields, &data, &len);
      if (r != NO_ERROR) {
        res = r;
        goto err;
      }
          
      source_charset = fields.fld_content_charset;
      if (source_charset == NULL)
        source_charset = DEST_CHARSET;
      
      r = charconv_buffer(source_charset, DEST_CHARSET,
          data, len, &converted, &converted_len);
      if (r != MAIL_CHARCONV_NO_ERROR) {
        
        r = fprintf(f, "[ error converting charset from %s to %s ]\n",
            source_charset, DEST_CHARSET);
          if (r < 0) {
            res = ERROR_FILE;
            goto err;
          }
          
          write_len = fwrite(data, 1, len, f);
          if (write_len != len) {
            mailmime_decoded_part_free(data);
            res = r;
            goto err;
          }
        }
        else {
          write_len = fwrite(converted, 1, converted_len, f);
          if (write_len != len) {
            charconv_buffer_free(converted);
            mailmime_decoded_part_free(data);
            res = r;
            goto err;
          }
              
          charconv_buffer_free(converted);
        }
            
        write_len = fwrite("\r\n\r\n", 1, 4, f);
        if (write_len < 4) {
          mailmime_decoded_part_free(data);
          res = ERROR_FILE;
          goto err;
        }
          
      mailmime_decoded_part_free(data);
    }
    else {
      /* not viewable part */

      r = fprintf(f, "   (not shown)\n\n");
      if (r < 0) {
        res = ERROR_FILE;
        goto err;
      }
    }

    break;
    
  case MAILMIME_MULTIPLE:

    if (strcasecmp(mime->mm_content_type->ct_subtype, "alternative") == 0) {
      struct mailmime * prefered_body;
      int prefered_score;

      /* case of multiple/alternative */

      /*
        we choose the better part,
        alternative preference :

	text/plain => score 3
	text/xxx   => score 2
	other      => score 1
      */

      prefered_body = NULL;
      prefered_score = 0;

      for(cur = clist_begin(mime->mm_data.mm_multipart.mm_mp_list) ;
          cur != NULL ; cur = clist_next(cur)) {
	struct mailmime * submime;
	int score;

	score = 1;
	submime = clist_content(cur);
        if (etpan_mime_is_text(submime))
          score = 2;

	if (submime->mm_content_type != NULL) {
          if (strcasecmp(submime->mm_content_type->ct_subtype, "plain") == 0)
            score = 3;
	}

	if (score > prefered_score) {
	  prefered_score = score;
	  prefered_body = submime;
	}
      }

      if (prefered_body != NULL) {
	r = etpan_render_mime(f, msg_info, prefered_body);
	if (r != NO_ERROR) {
	  res = r;
          goto err;
        }
      }
    }
    else {
      for(cur = clist_begin(mime->mm_data.mm_multipart.mm_mp_list) ;
          cur != NULL ; cur = clist_next(cur)) {
        
        r = etpan_render_mime(f, msg_info, clist_content(cur));
        if (r != NO_ERROR) {
          res = r;
          goto err;
        }
      }
    }

    break;
      
  case MAILMIME_MESSAGE:

    if (mime->mm_data.mm_message.mm_fields != NULL) {
      struct mailimf_fields * fields;
      
      if (msg_info != NULL) {
        fields = fetch_fields(msg_info, mime);
        if (fields == NULL) {
          res = ERROR_FETCH;
          goto err;
        }
        
        col = 0;
        r = fields_write(f, &col, fields);
        if (r != NO_ERROR) {
          mailimf_fields_free(fields);
          res = r;
          goto err;
        }
        
        mailimf_fields_free(fields);
      }
      else {
        col = 0;
        r = fields_write(f, &col, mime->mm_data.mm_message.mm_fields);
        if (r != NO_ERROR) {
          res = r;
          goto err;
        }
      }
      
      r = fprintf(f, "\r\n");
      if (r < 0) {
        res = ERROR_FILE;
        goto err;
      }
    }
    
    if (mime->mm_data.mm_message.mm_msg_mime != NULL) {
      r = etpan_render_mime(f, msg_info, mime->mm_data.mm_message.mm_msg_mime);
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
    
    r = etpan_render_mime(stdout, msg, mime);

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
