#ifndef OPTION_PARSER

#define OPTION_PARSER

#include <libetpan/libetpan.h>

enum {
  POP3_STORAGE = 0,
  IMAP_STORAGE,
  NNTP_STORAGE,
  MBOX_STORAGE,
  MH_STORAGE,
  MAILDIR_STORAGE,
};

int parse_options(int argc, char ** argv,
    int * driver,
    char ** server, int * port, int * connection_type,
    char ** user, char ** password, int * auth_type,
    char ** path, char ** cache_directory,
    char ** flags_directory);

int init_storage(struct mailstorage * storage,
    int driver, const char * server, int port,
    int connection_type, const char * user, const char * password, int auth_type,
    const char * path, const char * cache_directory, const char * flags_directory);

#endif
