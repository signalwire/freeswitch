/*
 * libEtPan! -- a mail stuff library
 *
 * Copyright (C) 2001, 2005 - DINH Viet Hoa
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the libEtPan! project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * $Id: mailstorage_tools.c,v 1.20 2006/07/15 12:24:26 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailstorage_tools.h"

#include "libetpan-config.h"

#include <sys/types.h>
#include <stdlib.h>
#ifndef _MSC_VER
#	include <netdb.h>
#	include <netinet/in.h>
#	include <sys/socket.h>
#	include <unistd.h>
#	include <sys/wait.h>
#	include <sys/ioctl.h>
#endif
#include <fcntl.h>
#include <string.h>
#include <stdio.h>

#include "mail.h"
#include "mailmessage.h"
#include "maildriver.h"

/* tools */

/* connection to TCP/IP server */

static int tcp_connect(char * server, uint16_t port)
{
  return mail_tcp_connect(server, port);
}

/* connection through a shell command */
/* SEB unsupported on Windows */
#ifndef WIN32

#define ENV_BUFFER_SIZE 512

static void do_exec_command(int fd, const char *command,
    char *servername, uint16_t port)
{
  int i, maxopen;
#ifdef SOLARIS
  char env_buffer[ENV_BUFFER_SIZE];
#endif
  
  if (fork() > 0) {
    /* Fork again to become a child of init rather than
       the etpan client. */
    exit(0);
  }
  
#ifdef SOLARIS
  if (servername)
    snprintf(env_buffer, ENV_BUFFER_SIZE, "ETPANSERVER=%s", servername);
  else
    snprintf(env_buffer, ENV_BUFFER_SIZE, "ETPANSERVER=");
  putenv(env_buffer);
#else
  if (servername)
    setenv("ETPANSERVER", servername, 1);
  else
    unsetenv("ETPANSERVER");
#endif
  
#ifdef SOLARIS
  if (port)
    snprintf(env_buffer, ENV_BUFFER_SIZE, "ETPANPORT=%d", port);
  else
    snprintf(env_buffer, ENV_BUFFER_SIZE, "ETPANPORT=");
  putenv(env_buffer);
#else
  if (port) {
    char porttext[20];
    
    snprintf(porttext, sizeof(porttext), "%d", port);
    setenv("ETPANPORT", porttext, 1);
  }
  else {
    unsetenv("ETPANPORT");
  }
#endif
  
  /* Not a lot we can do if there's an error other than bail. */
  if (dup2(fd, 0) == -1)
    exit(1);
  if (dup2(fd, 1) == -1)
    exit(1);
  
  /* Should we close stderr and reopen /dev/null? */
  
  maxopen = sysconf(_SC_OPEN_MAX);
  for (i=3; i < maxopen; i++)
    close(i);
  
#ifdef TIOCNOTTY
  /* Detach from the controlling tty if we have one. Otherwise,
     SSH might do something stupid like trying to use it instead
     of running $SSH_ASKPASS. Doh. */
  fd = open("/dev/tty", O_RDONLY);
  if (fd != -1) {
    ioctl(fd, TIOCNOTTY, NULL);
    close(fd);
  }
#endif /* TIOCNOTTY */

  execl("/bin/sh", "/bin/sh", "-c", command, NULL);
  
  /* Eep. Shouldn't reach this */
  exit(1);
}
#endif /* WIN32 */

static int subcommand_connect(char *command, char *servername, uint16_t port)
{
/* SEB unsupported on Windows */
#ifdef WIN32
	return -1;
#else

  int sockfds[2];
  pid_t childpid;
  
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockfds))
    return -1;
  
  childpid = fork();
  if (!childpid) {
    do_exec_command(sockfds[1], command, servername, port);
  }
  else if (childpid == -1) {
    close(sockfds[0]);
    close(sockfds[1]);
    return -1;
  }
  
  close(sockfds[1]);
  
  /* Reap child, leaving grandchild process to run */
  waitpid(childpid, NULL, 0);
  
  return sockfds[0];
#endif /* WIN32 */
}

int mailstorage_generic_connect(mailsession_driver * driver,
    char * servername,
    uint16_t port,
    char * command,
    int connection_type,
    int cache_function_id,
    char * cache_directory,
    int flags_function_id,
    char * flags_directory,
    mailsession ** result)
{
  int r;
  int res;
  mailstream * stream;
  int fd;
  mailsession * session;
  int connect_result;
  
  switch (connection_type) {
  case CONNECTION_TYPE_PLAIN:
  case CONNECTION_TYPE_TRY_STARTTLS:
  case CONNECTION_TYPE_STARTTLS:
  case CONNECTION_TYPE_TLS:
    fd = tcp_connect(servername, port);
    if (fd == -1) {
      res = MAIL_ERROR_CONNECT;
      goto err;
    }
    break;

  case CONNECTION_TYPE_COMMAND:
  case CONNECTION_TYPE_COMMAND_TRY_STARTTLS:
  case CONNECTION_TYPE_COMMAND_STARTTLS:
  case CONNECTION_TYPE_COMMAND_TLS:
    fd = subcommand_connect(command, servername, port);
    break;
  default:
    fd = -1;
    break;
  }
  
  if (fd == -1) {
    res = MAIL_ERROR_INVAL;
    goto err;
  }
  
  switch (connection_type) {
  case CONNECTION_TYPE_PLAIN:
  case CONNECTION_TYPE_TRY_STARTTLS:
  case CONNECTION_TYPE_STARTTLS:
  case CONNECTION_TYPE_COMMAND:
  case CONNECTION_TYPE_COMMAND_TRY_STARTTLS:
  case CONNECTION_TYPE_COMMAND_STARTTLS:
    stream = mailstream_socket_open(fd);
    break;
    
  case CONNECTION_TYPE_TLS:
  case CONNECTION_TYPE_COMMAND_TLS:
    stream = mailstream_ssl_open(fd);
    break;
    
  default:
    stream = NULL;
    break;
  }

  if (stream == NULL) {
    res = MAIL_ERROR_STREAM;
    close(fd);
    goto err;
  }

  session = mailsession_new(driver);
  if (session == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto close_stream;
  }

  if (cache_directory != NULL) {
    char cache_directory_server[PATH_MAX];
    
    snprintf(cache_directory_server, PATH_MAX, "%s/%s",
        cache_directory, servername);
    
    r = mailsession_parameters(session,
			       cache_function_id,
			       cache_directory_server);
    if (r != MAIL_NO_ERROR) {
      res = r;
      goto close_stream;
    }
  }

  if (flags_directory != NULL) {
    char flags_directory_server[PATH_MAX];
    
    snprintf(flags_directory_server, PATH_MAX, "%s/%s",
        flags_directory, servername);
    
    r = mailsession_parameters(session,
        flags_function_id,
        flags_directory_server);
    if (r != MAIL_NO_ERROR) {
      res = r;
      goto close_stream;
    }
  }

  r = mailsession_connect_stream(session, stream);
  switch (r) {
  case MAIL_NO_ERROR_NON_AUTHENTICATED:
  case MAIL_NO_ERROR_AUTHENTICATED:
  case MAIL_NO_ERROR:
    break;
  default:
    res = r;
    goto free;
  }

  connect_result = r;

  switch (connection_type) {
  case CONNECTION_TYPE_TRY_STARTTLS:
  case CONNECTION_TYPE_COMMAND_TRY_STARTTLS:
    r = mailsession_starttls(session);
    if ((r != MAIL_NO_ERROR) && (r != MAIL_ERROR_NO_TLS)) {
      res = r;
      goto free;
    }
    break;

  case CONNECTION_TYPE_STARTTLS:
  case CONNECTION_TYPE_COMMAND_STARTTLS:
    r = mailsession_starttls(session);
    if (r != MAIL_NO_ERROR) {
      res = r;
      goto free;
    }
  }

  * result = session;

  return connect_result;

 close_stream:
  mailstream_close(stream);
 free:
  mailsession_free(session);
 err:
  return res;
}





int mailstorage_generic_auth(mailsession * session,
    int connect_result,
    int auth_type,
    char * login,
    char * password)
{
  int must_auth;
  int r;
  int res;

  r = connect_result;

  must_auth = FALSE;
  switch (r) {
  case MAIL_NO_ERROR_NON_AUTHENTICATED:
    must_auth = TRUE;
    break;
  case MAIL_NO_ERROR_AUTHENTICATED:
  case MAIL_NO_ERROR:
    break;
  default:
    res = r;
    goto err;
  }

  if ((login == NULL) || (password == NULL))
    must_auth = FALSE;

  if (must_auth) {
    r = mailsession_login(session, login, password);
    if (r != MAIL_NO_ERROR) {
      mailsession_logout(session);
      res = r;
      goto err;
    }
  }

  return MAIL_NO_ERROR;

 err:
  return res;
}

int mailstorage_generic_auth_sasl(mailsession * session,
    int connect_result,
    const char * auth_type,
    const char * server_fqdn,
    const char * local_ip_port,
    const char * remote_ip_port,
    const char * login, const char * auth_name,
    const char * password, const char * realm)
{
  int must_auth;
  int r;
  int res;

  r = connect_result;

  must_auth = FALSE;
  switch (r) {
  case MAIL_NO_ERROR_NON_AUTHENTICATED:
    must_auth = TRUE;
    break;
  case MAIL_NO_ERROR_AUTHENTICATED:
  case MAIL_NO_ERROR:
    break;
  default:
    res = r;
    goto err;
  }

  if (must_auth) {
    r = mailsession_login_sasl(session, auth_type,
        server_fqdn,
        local_ip_port,
        remote_ip_port,
        login, auth_name,
        password, realm);
    if (r != MAIL_NO_ERROR) {
      mailsession_logout(session);
      res = r;
      goto err;
    }
  }

  return MAIL_NO_ERROR;

 err:
  return res;
}
