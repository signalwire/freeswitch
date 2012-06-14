/* -*- mode:c; indent-tabs-mode:nil; c-basic-offset:2 -*-
 * Author: Travis Cross <tc@traviscross.com>
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>

int sys(char *buf, int buflen, char *cmd) {
  int i, p[2];
  pipe(p);
  if (!(i=fork())) {
    close(p[0]);
    dup2(p[1],1);
    close(p[1]);
    execlp("sh","sh","-c",cmd,NULL);
  } else {
    int s, x, c=0;
    close(p[1]);
    waitpid(i,&s,0);
    if (!(WIFEXITED(s))) return 255;
    if (WEXITSTATUS(s)) return WEXITSTATUS(s);
    while ((x=read(p[0],buf,buflen-1))>0) c+=x;
    if (x<0) return 1;
    buf[c] = 0;
  }
  return 0;
}

int sys1(char *buf, int buflen, char *cmd) {
  int r = sys(buf,buflen,cmd);
  char *c;
  if (r!=0) return r;
  if ((c=strstr(buf,"\n"))) *c=0;
  return 0;
}

int main(int argc, char **argv) {
  char buf[256], xdate[256], xfdate[256], xcommit[256], xver[256];
  time_t xdate_t;
  struct tm *xdate_tm;

  sys1(xdate,sizeof(xdate),"git log -n1 --format='%ct' HEAD");
  xdate_t = (time_t) atoi(xdate);
  if (!(xdate_tm = gmtime(&xdate_t))) return 1;
  strftime(xfdate,sizeof(xfdate),"%Y%m%dT%H%M%SZ",xdate_tm);
  sys1(xcommit,sizeof(xcommit),"git rev-list -n1 --abbrev=10 --abbrev-commit HEAD");
  snprintf(xver,sizeof(xver),"+git~%s~%s",xfdate,xcommit);
  if ((sys(buf,sizeof(buf),"git diff-index --quiet HEAD"))) {
    time_t now_t = time(NULL);
    struct tm *now_tm = gmtime(&now_t);
    char now[256];
    if (!now_tm) return 1;
    strftime(now,sizeof(now),"%Y%m%dT%H%M%SZ",now_tm);
    snprintf(buf,sizeof(buf),"%s+unclean~%s",xver,now);
    strncpy(xver,buf,sizeof(xver));
  }
  printf("%s\n",xver);
  return 0;
}

