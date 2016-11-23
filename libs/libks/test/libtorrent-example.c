/* This example code was written by Juliusz Chroboczek.
   You are free to cut'n'paste from it to your heart's content. */

/* For crypt */
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/signal.h>

#include "ks.h"
#include "histedit.h"
#include "libtorrent.h"

static char * prompt(EditLine *e) {
  return "dht> ";
}

typedef struct dht_globals_s {
  int exiting;
} dht_globals_t;

int
main(int argc, char **argv)
{
  dht_globals_t globals = {0};
  int opt;
  EditLine *el;
  History *myhistory;
  int count;
  const char *line;
  HistEvent ev;
  ks_status_t status = KS_STATUS_SUCCESS;
  //  ks_pool_t *pool;
  void *session = session_create(SES_LISTENPORT, 8090,
				 SES_LISTENPORT_END, 8098,
				 TAG_END);

  session_start_dht(session);

  
  globals.exiting = 0;
    
  el = el_init("test", stdin, stdout, stderr);
  el_set(el, EL_PROMPT, &prompt);
  el_set(el, EL_EDITOR, "emacs");
  myhistory = history_init();
  history(myhistory, &ev, H_SETSIZE, 800);
  el_set(el, EL_HIST, history, myhistory);
    
  ks_global_set_default_logger(7);

  while(1) {
    opt = getopt(argc, argv, "hb:");
    if(opt < 0)
      break;

    switch(opt) {
    case 'b': {
      printf("Not yet implemented\n");
      goto usage;
    }
      break;
    default:
      goto usage;
    }
  }

  /*
    ks_pool_open(&pool);
    status = ks_thread_create_ex(&threads[0], dht_event_thread, &globals, KS_THREAD_FLAG_DETATCHED, KS_THREAD_DEFAULT_STACK, KS_PRI_NORMAL, pool);
  */
    
  if ( status != KS_STATUS_SUCCESS) {
    printf("Failed to start DHT event thread\n");
    exit(1);
  }

  while ( !globals.exiting ) {
    line = el_gets(el, &count);
      
    if (count > 1) {
      int line_len = (int)strlen(line) - 1;
      history(myhistory, &ev, H_ENTER, line);

      if (!strncmp(line, "quit", 4)) {
	globals.exiting = 1;
      } else if (!strncmp(line, "loglevel", 8)) {
	ks_global_set_default_logger(atoi(line + 9));
      } else if (!strncmp(line, "peer_dump", 9)) {
	printf("Not yet implemented\n");
      } else if (!strncmp(line, "search", 6)) {
	printf("Not yet implemented\n");
      } else if (!strncmp(line, "announce", 8)) {
	printf("Not yet implemented\n");
      } else {
	printf("Unknown command entered[%.*s]\n", line_len, line);
      }
    }
  }

  history_end(myhistory);
  el_end(el);
  session_close(session);
  return 0;
    
 usage:
  printf("Usage: dht-example [-4] [-6] [-i filename] [-b address]...\n"
	 "                   port [address port]...\n");
  exit(1);
}
