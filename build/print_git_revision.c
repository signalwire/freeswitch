/* -*- mode:c; indent-tabs-mode:nil; c-basic-offset:2 -*-
 * Author: Travis Cross <tc@traviscross.com>
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>

static int show_unclean = 0;

static int sys(char *buf, int buflen, char *cmd) {
  int i, p[2];
  if (pipe(p)) return 255;
  if (!(i=fork())) {
    close(p[0]);
    dup2(p[1],1);
    close(p[1]);
    execlp("sh","sh","-c",cmd,NULL);
  } else {
    int s, x=0;
    close(p[1]);
    waitpid(i,&s,0);
    if (!(WIFEXITED(s))) return 255;
    if (WEXITSTATUS(s)) return WEXITSTATUS(s);
    if (buf) {
      while (buflen>1 && (x=read(p[0],buf,buflen-1))>0) buf+=x,buflen-=x;
      close(p[0]);
      if (x<0) return 255;
      *buf=0;
    } else close(p[0]);
  }
  return 0;
}

static int sys1(char *buf, int buflen, char *cmd) {
  int r; char *c;
  if ((r=sys(buf,buflen,cmd))) return r;
  if ((c=strstr(buf,"\n"))) *c=0;
  return 0;
}

static int print_version(void) {
  char xver[256], xdate[256], xfdate[256], xcommit[256];
  time_t xdate_t; struct tm *xdate_tm;
  if ((sys1(xdate,sizeof(xdate),"git log -n1 --format='%ct' HEAD"))) return 1;
  xdate_t=(time_t)atoi(xdate);
  if (!(xdate_tm=gmtime(&xdate_t))) return 1;
  strftime(xfdate,sizeof(xfdate),"%Y%m%dT%H%M%SZ",xdate_tm);
  if ((sys1(xcommit,sizeof(xcommit),"git rev-list -n1 --abbrev=10 --abbrev-commit HEAD")))
    return 1;
  snprintf(xver,sizeof(xver),"+git~%s~%s",xfdate,xcommit);
  if (show_unclean && (sys(NULL,0,"git diff-index --quiet HEAD"))) {
    char buf[256], now[256]; time_t now_t=time(NULL); struct tm *now_tm;
    if (!(now_tm=gmtime(&now_t))) return 1;
    strftime(now,sizeof(now),"%Y%m%dT%H%M%SZ",now_tm);
    snprintf(buf,sizeof(buf),"%s+unclean~%s",xver,now);
    strncpy(xver,buf,sizeof(xver));
  }
  printf("%s\n",xver);
  return 0;
}

static int print_human_version(void) {
  char xver[256], xdate[256], xfdate[256], xcommit[256];
  time_t xdate_t; struct tm *xdate_tm;
  if ((sys1(xdate,sizeof(xdate),"git log -n1 --format='%ct' HEAD"))) return 1;
  xdate_t=(time_t)atoi(xdate);
  if (!(xdate_tm=gmtime(&xdate_t))) return 1;
  strftime(xfdate,sizeof(xfdate),"%a, %d %b %Y %H:%M:%S Z",xdate_tm);
  if ((sys1(xcommit,sizeof(xcommit),"git rev-list -n1 --abbrev=10 --abbrev-commit HEAD")))
    return 1;
  snprintf(xver,sizeof(xver),"; git at commit %s on %s",xcommit,xfdate);
  if (show_unclean && (sys(NULL,0,"git diff-index --quiet HEAD"))) {
    char buf[256], now[256]; time_t now_t=time(NULL); struct tm *now_tm;
    if (!(now_tm=gmtime(&now_t))) return 1;
    strftime(now,sizeof(now),"%a, %d %b %Y %H:%M:%S Z",now_tm);
    snprintf(buf,sizeof(buf),"%s; unclean git build on %s",xver,now);
    strncpy(xver,buf,sizeof(xver));
  }
  printf("%s\n",xver);
  return 0;
}

int main(int argc, char **argv) {
  if (argc > 1 && !strcasecmp(argv[1],"-h"))
    return print_human_version();
  else
    return print_version();
}

