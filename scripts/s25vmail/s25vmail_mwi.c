/*
 *  File:    s25vmail_mwi.c
 *  Purpose: Send AT&T System 25 PBX MWI DTMF based on MWI events
 *  Machine:                      OS:
 *  Author:  John Wehle           Date: July 24, 2008
 *
 *  Copyright (c)  2008  Feith Systems and Software, Inc.
 *                      All Rights Reserved
 *
 *  Tested using a Zyxel U90e configured using:
 *
 *    at OK at&f OK at&d3&y2q2 OK ats0=0s2=255s15.7=0s18=4s35.1=0 OK
 *    ats38.3=1s42.3=1s42.6=1 OK atl0 OK at&w OK at&v
 *
 *  though just about any modem should work.  Preferred settings are
 *
 *    DTR OFF causes hangup and reset from profile 0
 *    RTS / CTS flow control
 *    allow abort during modem handshake
 *    auto answer off
 *    ring message off
 */


#include <ctype.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <memory.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <termios.h>

#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <unistd.h>


#define LOGFILE                    "/var/log/s25vmail_mwi.log"


static const char *MyName = "s25vmail_mwi";

static int daimon = 0;
static int error_msg_throttle = 0;
static volatile int shutdown_server = 0;


static void
debugmsg (const char *fmt, ...)
  {
  char message[256];
  va_list args;

  if (daimon)
    return;

  va_start (args, fmt);
  vsprintf (message, fmt, args);
  va_end (args);

  fprintf (stderr, "%s: %s", MyName, message);
  if ( !strchr (message, '\n'))
    fprintf (stderr, "\n");
  fflush (stderr);
  }


static void
errmsg (const char *fmt, ...)
  {
  char time_stamp[256];
  struct tm *tmp;
  time_t now;
  va_list args;

  if (! daimon) {
    fprintf (stderr, "%s: ", MyName);

    va_start (args, fmt);
    vfprintf (stderr, fmt, args);
    va_end (args);

    if (! strchr (fmt, '\n'))
      fputc ('\n', stderr);

    fflush (stderr);
    return;
    }

  if (error_msg_throttle)
    return;

  time (&now);

  if ( !(tmp = localtime (&now)) ) {
    fprintf (stderr, "%s: errmsg -- localtime failed.\n", MyName);
    perror (MyName);
    fflush (stderr);
    return;
    }

  strftime (time_stamp, sizeof (time_stamp), "%b %d %H:%M:%S", tmp);
  fprintf (stderr, "%s %s[%d]: ", time_stamp, MyName, (int)getpid ());

  va_start (args, fmt);
  vfprintf (stderr, fmt, args);
  va_end (args);

  if (! strchr (fmt, '\n'))
    fputc ('\n', stderr);

  fflush (stderr);
  }


static void
catch_signal ()
  {

  shutdown_server = 1;
  }


static void
daemonize()
  {

#ifdef SIGTSTP
  (void)signal(SIGTSTP, SIG_IGN);
#endif
#ifdef SIGTTIN
  (void)signal(SIGTTIN, SIG_IGN);
#endif
#ifdef SIGTTOU
  (void)signal(SIGTTOU, SIG_IGN);
#endif

  switch (fork ()) {
    case 0:
      break;

    case -1:
      fprintf (stderr, "%s: daemonize -- fork failed.", MyName);
      perror (MyName);
      exit (1);
    /* NOTREACHED */
      break;

    default:
      exit (0);
    /* NOTREACHED */
      break;
    }

  setsid();

  close (0);
  close (1);
  close (2);

  (void)open ("/dev/null", O_RDWR);
  (void)open ("/dev/null", O_RDWR);
  (void)open (LOGFILE, O_WRONLY | O_APPEND | O_CREAT, 0644);

  daimon = 1;
  }


static void
install_signal_handlers ()
  {
  struct sigaction act;

  memset (&act, '\0', sizeof (act));

  act.sa_handler = catch_signal;
  sigemptyset (&act.sa_mask);
  act.sa_flags = 0;

  if (signal (SIGHUP, SIG_IGN) != SIG_IGN)
    sigaction (SIGHUP, &act, NULL);
  if (signal (SIGINT, SIG_IGN) != SIG_IGN)
    sigaction (SIGINT, &act, NULL);
  (void)sigaction (SIGTERM, &act, NULL);
  }


static int
connect_to_service (const char *hostname, const char *port)
  {
  int sock;
  struct hostent *hp;
  struct in_addr address;
  struct servent *servp;
  struct sockaddr_in sin;
  
  if ((sock = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
    char *errstr = strerror (errno);

    errmsg ("socket failed\n");
    errmsg (errstr);
    return -1;
    }

  memset (&sin, 0, sizeof (sin));

  if (isalpha (hostname[0])) {
    if ( !(hp = gethostbyname (hostname))) {
      char *errstr = strerror (errno);

      errmsg ("gethostbyname failed\n");
      errmsg (errstr);
      close (sock);
      return -1;
      }
    if (hp->h_addrtype != AF_INET) {
      errmsg ("gethostbyname returned unsupported family\n");
      close (sock);
      return -1;
      }
    memcpy (&sin.sin_addr, hp->h_addr, sizeof(sin.sin_addr));
    }
  else {
    address.s_addr = inet_addr (hostname);

    if ((long)address.s_addr == -1) {
      char *errstr = strerror (errno);

      errmsg ("inet_addr failed\n");
      errmsg (errstr);
      close (sock);
      return -1;
      }
    sin.sin_addr.s_addr = address.s_addr;
    }

  if (isalpha (*port)) {
    if ( !(servp = getservbyname(port, "tcp"))) {
      char *errstr = strerror (errno);

      errmsg ("getservbyname failed\n");
      errmsg (errstr);
      close (sock);
      return -1;
      }
    sin.sin_port = servp->s_port;
    }
  else
    sin.sin_port = htons ((unsigned short)atoi (port));

  sin.sin_family = AF_INET;

  if (connect (sock, (struct sockaddr *)&sin, sizeof(sin)) == -1) {
    char *errstr = strerror (errno);

    errmsg ("connect failed\n");
    errmsg (errstr);
    close (sock);
    return -1;
    }

  debugmsg ("Connected to service\n");

  return sock;
  }


static ssize_t
read_line (int fd, char *buf, size_t buf_len)
  {
  size_t l;
  ssize_t nbytes_read;

  l = 0;

  for ( ; ; ) {
    nbytes_read = read (fd, &buf[l], 1);

    if (nbytes_read < 0) {
      char *errstr = strerror (errno);

      errmsg ("read failed in middle of line\n");
      errmsg (errstr);
      return -1;
      }

    if (nbytes_read == 0) {
      if (l)
        errmsg ("EOF in middle of line\n");
      return l ? -1 : 0;
      }

    if (buf[l] == '\n') {
      while (l && buf[l - 1] == '\r')
        l--;
      buf[l++] = '\0';
      break;
      }

    l++;

    if (l == buf_len) {
      errmsg ("line too long\n");
      return -1;
      }
    }

  return l;
  }


static int
read_trailing_newline(int fd)
  {
  char c;
  ssize_t nbytes_read;

  nbytes_read = read (fd, &c, 1);

  if (nbytes_read < 0) {
    char *errstr = strerror (errno);

    errmsg ("read failed in trailing newline\n");
    errmsg (errstr);
    return -1;
    }

  if (nbytes_read == 0) {
    errmsg ("EOF in trailing newline\n");
    return -1;
    }

  if (c != '\n') {
    errmsg ("missing trailing newline\n");
    return -1;
    }

  return 0;
  }


static char *
retrieve_message (int fd)
  {
  char cl_buf[64];
  char ct_buf[64];
  char *h;
  char *m;
  ssize_t cl;
  ssize_t nbytes_read;
  size_t l;
  size_t nbytes_to_read;

  if (shutdown_server)
    return NULL;

  /*
   * Read / parse Content-Length and Content-Type.
   */

  nbytes_read = read_line (fd, cl_buf, sizeof (cl_buf));

  if (nbytes_read < 0) {
    errmsg ("read_line failed\n");
    return NULL;
    }

  if (nbytes_read == 0) {

    /*
     * EOF
     */

    return NULL;
    }

  nbytes_read = read_line (fd, ct_buf, sizeof (ct_buf));

  if (nbytes_read < 0) {
    errmsg ("read_line failed\n");
    return NULL;
    }

  if (nbytes_read == 0) {
    errmsg ("EOF in middle of headers\n");
    return NULL;
    }

  h = "Content-Length: ";
  l = strlen (h);

  if (strncmp (cl_buf, h, l) != 0) {

    /*
     * If the message header doesn't being with Content-Length,
     * then it needs to be a Content-Type we understand.
     */

    h = "Content-Type: ";
    l = strlen (h);

    if (strncmp (cl_buf, h, l) != 0) {
      errmsg ("missing Content-Type\n");
      return NULL;
      }

    if (strcmp (&cl_buf[l], "auth/request") != 0
        && strcmp (&cl_buf[l], "command/reply") != 0) {
      errmsg ("Unsupported Content-Type\n");
      return NULL;
      }

    if (ct_buf[0])
      if (read_trailing_newline (fd) < 0) {
        return NULL;
        }

    m = malloc (strlen (cl_buf) + 1 + strlen (ct_buf) + 1 + 1);

    if (! m) {
      char *errstr = strerror (errno);

      errmsg ("malloc failed\n");
      errmsg (errstr);
      return NULL;
      }

    sprintf (m, "%s\n%s\n", cl_buf, ct_buf);

    return m;
    }

  cl = atoi (&cl_buf[l]);

  if (cl <= 0) {
    errmsg ("Content-Length must be greater than zero\n");
    return NULL;
    }

  h = "Content-Type: ";
  l = strlen (h);

  if (strncmp (ct_buf, h, l) != 0) {
    errmsg ("missing Content-Type\n");
    return NULL;
    }

  if (strcmp (&ct_buf[l], "text/event-plain") != 0) {
    errmsg ("Unsupported Content-Type\n");
    return NULL;
    }

  if (read_trailing_newline (fd) < 0) {
    return NULL;
    }

  /*
   * Read the event.
   */

  m = malloc (cl);

  if (! m) {
    char *errstr = strerror (errno);

    errmsg ("malloc failed\n");
    errmsg (errstr);
    return NULL;
    }

  for (nbytes_to_read = cl; nbytes_to_read; nbytes_to_read -= nbytes_read) {
    nbytes_read = read (fd, m + (cl - nbytes_to_read), nbytes_to_read);

    if (nbytes_read < 0) {
      char *errstr = strerror (errno);

      errmsg ("read failed in middle of message\n");
      errmsg (errstr);
      free (m);
      return NULL;
      }

    if (nbytes_read == 0) {
      errmsg ("EOF in middle of message\n");
      free (m);
      return NULL;
      }
    }

  if (m[cl - 2] != '\n' || m[cl - 1] != '\n') {
    errmsg ("Message is missing trailing newlines\n");
    free (m);
    return NULL;
    }

  return m;
  }


static int
send_password (int fd, const char *passwd)
  {
  char *h;
  char *last;
  char *m;
  char *p;
  int l;
  size_t ml;

  m = retrieve_message (fd);
  if (! m)
    return -1;

  p = strtok_r (m, "\n", &last);

  h = "Content-Type: auth/request";

  if (strcmp (p, h) != 0) {
    errmsg ("Content-Type wasn't auth/request\n");
    free (m);
    return -1;
    }

  free (m);

  l = snprintf (NULL, 0, "auth %s\n\n", passwd);
  if (l <= 0) {
    errmsg ("snprintf failed\n");
    return -1;
    }
  l++;

  m = malloc (l);
  if (! m) {
    char *errstr = strerror (errno);

    errmsg ("malloc failed\n");
    errmsg (errstr);
    return -1;
    }

  ml = snprintf (m, l, "auth %s\n\n", passwd);
  if ((ml + 1) != l) {
    errmsg ("snprintf failed\n");
    free (m);
    return -1;
    }

  if (write (fd, m, ml) != ml) {
    char *errstr = strerror (errno);

    errmsg ("write failed\n");
    errmsg (errstr);
    free (m);
    return -1;
    }

  m = retrieve_message (fd);
  if (! m )
    return -1;

  p = strtok_r (m, "\n", &last);

  h = "Content-Type: command/reply";

  if (! p || strcmp (p, h) != 0) {
    errmsg ("Content-Type wasn't command/reply\n");
    free (m);
    return -1;
    }

  p = strtok_r (NULL, "\n", &last);

  h = "Reply-Text: +OK accepted";

  if (! p || strcmp (p, h) != 0) {
    errmsg ("auth wasn't accepted\n");
    free (m);
    return -1;
    }

  free (m);

  debugmsg ("Logged into service\n");

  return 0;
  }


static int
enable_mwi_event (int fd)
  {
  char *h;
  char *last;
  char *m;
  char *p;
  size_t ml;

  m = "event plain MESSAGE_WAITING\n\n";
  ml = strlen (m);

  if (write (fd, m, ml) != ml) {
    char *errstr = strerror (errno);

    errmsg ("write failed\n");
    errmsg (errstr);
    return -1;
    }

  m = retrieve_message (fd);
  if (! m )
    return -1;

  p = strtok_r (m, "\n", &last);

  h = "Content-Type: command/reply";

  if (! p || strcmp (p, h) != 0) {
    errmsg ("Content-Type wasn't command/reply\n");
    free (m);
    return -1;
    }

  p = strtok_r (NULL, "\n", &last);

  h = "Reply-Text: +OK event listener enabled plain";

  if (! p || strcmp (p, h) != 0) {
    errmsg ("event wasn't enabled\n");
    free (m);
    return -1;
    }

  free (m);

  debugmsg ("Enabled message waiting event\n");

  return 0;
  }


static int
process_mwi_event (char *m, const char *device)
  {
  char cbuf[64];
  char rbuf[64];
  char *h;
  char *last;
  char *ma;
  char *mw;
  char *p;
  int fd;
  int mwi_off;
  int mwi_on;
  int r;
  int w;
  size_t l;
  ssize_t ml;
  ssize_t nbytes_read;
  struct termios tio;

  debugmsg ("Processing MWI event\n");

  ma = NULL;
  mw = NULL;

  p = m;

  while ( (p = strtok_r (p, "\n", &last)) ) {
    h = "MWI-Messages-Waiting: ";
    l = strlen (h);

    if (strncmp (p, h, l) == 0)
      mw = p + l;

    h = "MWI-Message-Account: ";
    l = strlen (h);

    if (strncmp (p, h, l) == 0)
      ma = p + l;

    p = NULL;
    }

  if (! (ma && mw) ) {
    errmsg ("message account or message waiting missing\n");
    return -1;
    }

  p = strchr (ma, '\n');
  if (p)
    *p = '\n';

  p = strchr (mw, '\n');
  if (p)
    *p = '\n';

  /*
   * The account is considered to be a System 25 extension if
   * it's of the form:
   *
   *   numeric_string@host
   */

  p = strchr (ma, '%');
  if (! p)
    p = strchr (ma, '@');

  if (! p || (strncmp (p, "%40", 3) != 0 && strncmp (p, "@", 1) != 0)) {
    debugmsg ("  %s is not a System 25 extension\n", ma);
    return 0;
    }

  *p = '\0';

  for (p = ma; *p; p++)
    if (! isdigit (*p)) {
      debugmsg ("  %s is not a System 25 extension\n", ma);
      return 0;
      }

  mwi_off = strcasecmp (mw, "no") == 0;
  mwi_on = strcasecmp (mw, "yes") == 0;

  if (mwi_off == mwi_on) {
    errmsg ("Unsupported Messages-Waiting\n");
    return 0;
    }

  for (r = 0; r < 3; r++) {
    if ((fd = open (device, O_RDWR)) < 0) {
      char *errstr = strerror (errno);

      errmsg ("open failed for device node <%s>.\n", device);
      errmsg (errstr);
      return -1;
      }

    cfmakeraw (&tio);

    tio.c_cflag = CS8 | CREAD | HUPCL | CCTS_OFLOW | CRTS_IFLOW;

    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 50;

    cfsetispeed (&tio, B9600);
    cfsetospeed (&tio, B9600);

    if (tcsetattr (fd, TCSAFLUSH, &tio) < 0) {
      char *errstr = strerror (errno);

      errmsg ("tcsetattr failed\n");
      errmsg (errstr);
      close (fd);
      return -1;
      }

    m = "AT";
    ml = strlen (m);

    if (write (fd, m, ml) != ml
        || write (fd, "\r\n", 2) != 2) {
      char *errstr = strerror (errno);

      errmsg ("write failed\n");
      errmsg (errstr);
      close (fd);
      return -1;
      }

    for (w = 0; w < 2; w++) {
      nbytes_read = read_line (fd, rbuf, sizeof (rbuf));
      if (nbytes_read > 0 && (rbuf[0] == '\0' || strcmp (rbuf, m) == 0))
        continue;
      break;
      }

    if (nbytes_read < 0) {
      errmsg ("read_line failed\n");
      close (fd);
      return -1;
      }

    if (nbytes_read == 0
        || strcmp (rbuf, "OK") != 0) {
      errmsg ("modem failed to wake up\n");
      close (fd);
      continue;
      }

    m = cbuf;
    ml  = snprintf (cbuf, sizeof (cbuf),
                    "ATDT%s%s", (mwi_on ? "#90" : "#91"),  ma);
    if (ml <= 0 || ml >= sizeof (cbuf)) {
      errmsg ("snprintf failed.\n");
      close (fd);
      return -1;
      }

    if (write (fd, m, ml) != ml
        || write (fd, "\r\n", 2) != 2) {
      char *errstr = strerror (errno);

      errmsg ("write failed\n");
      errmsg (errstr);
      close (fd);
      return -1;
      }

    sleep (5);

    if (write (fd, "\r\n", 2) != 2) {
      char *errstr = strerror (errno);

      errmsg ("write failed\n");
      errmsg (errstr);
      close (fd);
      return -1;
      }

    for (w = 0; w < 2; w++) {
      nbytes_read = read_line (fd, rbuf, sizeof (rbuf));
      if (nbytes_read > 0 && (rbuf[0] == '\0' || strcmp (rbuf, m) == 0))
        continue;
      break;
      }

    if (nbytes_read < 0) {
      errmsg ("read_line failed\n");
      close (fd);
      return -1;
      }

    if (nbytes_read > 0 && strcmp (rbuf, "NO DIALTONE") == 0) {
      errmsg ("modem failed to detect dialtone\n");
      close (fd);
      return -1;
      }

    if (nbytes_read == 0
        || strcmp (rbuf, "NO CARRIER") != 0) {
      errmsg ("modem failed to update MWI\n");
      close (fd);
      continue;
      }

    close (fd);

    debugmsg ("  message waiting indicator updated for %s\n", ma);
    return 0;
    }

  errmsg (" failed to update message waiting indicator for %s\n", ma);

  return -1;
  }


int
main (int argc, char **argv)
  {
  const char *device = "/dev/cuad0";
  const char *machine = "localhost";
  const char *port = "8021";
  const char *passwd = "ClueCon";
  char *m;
  int c;
  int debug;
  int fd;
  struct stat statbuf;

  debug = 0;

  while ((c = getopt (argc, argv, "dm:p:w:")) != -1)
    switch (c) {
      case 'd':
        debug = 1;
        break;

      case 'm':
        machine = optarg;
        break;

      case 'p':
        port = optarg;
        break;

      case 'w':
        passwd = optarg;
        break;

      case 'l':
        device = optarg;
        break;

      default:
        fprintf (stderr,
          "Usage: %s [-d] [-m machine] [-p port] [-w passwd] [-l device]\n",
                 MyName);
        exit(1);
      /* NOTREACHED */
        break;
      }

  if (stat (device, &statbuf) < 0 || ! S_ISCHR (statbuf.st_mode)) {
    fprintf (stderr, "%s: stat failed for path <%s>\n", MyName, device);
    fprintf (stderr, "%s: or the path isn't a character special file.\n",
             MyName);
    perror (MyName);
    exit (1);
    }

  install_signal_handlers ();

  if (! debug)
    daemonize ();

  while (! shutdown_server) {
    sleep (5);

    fd = connect_to_service (machine, port);
    if (fd < 0) {
      error_msg_throttle = 1;
      continue;
      }

    if (send_password (fd, passwd) < 0
        || enable_mwi_event (fd) < 0) {
      error_msg_throttle = 1;
      close (fd);
      continue;
      }

    error_msg_throttle = 0;

    while (m = retrieve_message (fd)) {
      process_mwi_event (m, device);
      free (m);
      }

    close (fd);
    }

  exit (0);
  }
