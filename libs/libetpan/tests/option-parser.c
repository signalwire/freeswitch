#define _GNU_SOURCE
#ifdef _MSC_VER
#	include "../src/bsd/getopt.h"
#else
#	include <getopt.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include <libetpan/libetpan.h>

#include "option-parser.h"

/*
  options
  
  --driver (pop3|imap|nntp|mbox|mh|maildir)  -d

  default driver is mbox

  --server {server-name} -s
  --port {port-number}   -p
  --tls                  -t
  --starttls             -x
  --user {login}         -u
  --password {password}  -v
  --path {mailbox}       -l
  --apop                 -a
  --cache {directory}    -c
  --flags {directory}    -f
*/

struct storage_name {
  int id;
  char * name;
};

static struct storage_name storage_tab[] = {
  {POP3_STORAGE, "pop3"},
  {IMAP_STORAGE, "imap"},
  {NNTP_STORAGE, "nntp"},
  {MBOX_STORAGE, "mbox"},
  {MH_STORAGE, "mh"},
  {MAILDIR_STORAGE, "maildir"},
};

static int get_driver(char * name)
{
  int driver_type;
  unsigned int i;

  driver_type = -1;
  for(i = 0 ; i < sizeof(storage_tab) / sizeof(struct storage_name) ; i++) {
    if (strcasecmp(name, storage_tab[i].name) == 0) {
      driver_type = i;
      break;
    }
  }

  return driver_type;
}

int parse_options(int argc, char ** argv,
    int * driver,
    char ** server, int * port, int * connection_type,
    char ** user, char ** password, int * auth_type,
    char ** path, char ** cache_directory,
    char ** flags_directory)
{
  int index;
  static struct option long_options[] = {
    {"driver",   1, 0, 'd'},
    {"server",   1, 0, 's'},
    {"port",     1, 0, 'p'},
    {"tls",      0, 0, 't'},
    {"starttls", 0, 0, 'x'},
    {"user",     1, 0, 'u'},
    {"password", 1, 0, 'v'},
    {"path",     1, 0, 'l'},
    {"apop",     0, 0, 'a'},
    {"cache",    1, 0, 'c'},
    {"flags",    1, 0, 'f'},
	{"debug-stream", 0, 0, 'D'},
  };
  int r;
  char location[PATH_MAX];
  char * env_user;

  index = 0;

  * driver = MBOX_STORAGE;
  * server = NULL;
  * port = 0;
  * connection_type = CONNECTION_TYPE_PLAIN;
  * user = NULL;
  * password = NULL;
  * auth_type = POP3_AUTH_TYPE_PLAIN;
  env_user = getenv("USER");
  if (env_user != NULL) {
    snprintf(location, PATH_MAX, "/var/mail/%s", env_user);
    * path = strdup(location);
  }
  else
    * path = NULL;
  * cache_directory = NULL;
  * flags_directory = NULL;

  while (1) {
    r = getopt_long(argc, argv, "d:s:p:txu:v:l:ac:f:D", long_options, &index);
    
    if (r == -1)
      break;

    switch (r) {
    case 'd':
      * driver = get_driver(optarg);
      break;
    case 's':
      if (* server != NULL)
        free(* server);
      * server = strdup(optarg);
      break;
    case 'p':
      * port = strtoul(optarg, NULL, 10);
      break;
    case 't':
      * connection_type = CONNECTION_TYPE_TLS;
      break;
    case 'x':
      * connection_type = CONNECTION_TYPE_STARTTLS;
        break;
    case 'u':
      if (* user != NULL)
        free(* user);
      * user = strdup(optarg);
      break;
    case 'v':
      if (* password != NULL)
        free(* password);
      * password = strdup(optarg);
      break;
    case 'l':
      if (* path != NULL)
        free(* path);
      * path = strdup(optarg);
      break;
    case 'a':
      * auth_type = POP3_AUTH_TYPE_APOP;
      break;
    case 'c':
      if (* cache_directory != NULL)
        free(* cache_directory);
      * cache_directory = strdup(optarg);
      break;
    case 'f':
      if (* flags_directory != NULL)
        free(* flags_directory);
      * flags_directory = strdup(optarg);
      break;
	case 'D':
		mailstream_debug = 1;
		break;
    }
  }

  return 0;
}

int init_storage(struct mailstorage * storage,
    int driver, const char * server, int port,
    int connection_type, const char * user, const char * password, int auth_type,
    const char * path, const char * cache_directory, const char * flags_directory)
{
  int r;
  int cached;

  cached = (cache_directory != NULL);
  
  switch (driver) {
  case POP3_STORAGE:
    r = pop3_mailstorage_init(storage, server, port, NULL, connection_type,
        auth_type, user, password, cached, cache_directory,
        flags_directory);
    if (r != MAIL_NO_ERROR) {
      printf("error initializing POP3 storage\n");
      goto err;
    }
    break;

  case IMAP_STORAGE:
    r = imap_mailstorage_init(storage, server, port, NULL, connection_type,
        IMAP_AUTH_TYPE_PLAIN, user, password, cached, cache_directory);
    if (r != MAIL_NO_ERROR) {
      printf("error initializing IMAP storage\n");
      goto err;
    }
    break;

  case NNTP_STORAGE:
    r = nntp_mailstorage_init(storage, server, port, NULL, connection_type,
        NNTP_AUTH_TYPE_PLAIN, user, password, cached, cache_directory,
        flags_directory);
    if (r != MAIL_NO_ERROR) {
      printf("error initializing NNTP storage\n");
      goto err;
    }
    break;

  case MBOX_STORAGE:
    r = mbox_mailstorage_init(storage, path, cached, cache_directory,
        flags_directory);
    if (r != MAIL_NO_ERROR) {
      printf("error initializing mbox storage\n");
      goto err;
    }
    break;

  case MH_STORAGE:
    r = mh_mailstorage_init(storage, path, cached, cache_directory,
        flags_directory);
    if (r != MAIL_NO_ERROR) {
      printf("error initializing MH storage\n");
      goto err;
    }
    break;
  case MAILDIR_STORAGE:
    r = maildir_mailstorage_init(storage, path, cached, cache_directory,
        flags_directory);
    if (r != MAIL_NO_ERROR) {
      printf("error initializing maildir storage\n");
      goto err;
    }
    break;
  }
  
  return MAIL_NO_ERROR;
  
 err:
  return r;
}
