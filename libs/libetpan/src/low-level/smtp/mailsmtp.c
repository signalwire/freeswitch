/*
 * libEtPan! -- a mail stuff library
 *
 * Copyright (C) 2001, 2005 - DINH Viet Hoa,
 * All rights reserved.
 *
 * SMTP AUTH support by Juergen Graf
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
 * $Id: mailsmtp.c,v 1.27 2006/08/29 23:14:02 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailsmtp.h"
#include "connect.h"
#include "base64.h"
#include "mail.h"

#ifdef HAVE_NETINET_IN_H
#	include <netinet/in.h>
#endif
#include <string.h>
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#	include <unistd.h>
#endif
#include <stdio.h>

#ifdef USE_SASL
#include <sasl/sasl.h>
#include <sasl/saslutil.h>
#endif

#include "mailsasl.h"

/*
  RFC 2821 : SMTP
  RFC 1891 : SMTP Service Extension for Delivery Status Notifications

  RFC 1428 : Transition of Internet Mail from Just-Send-8 to 8bit-SMTP/MIME
  RFC 1652 : SMTP Service Extension for 8bit-MIMEtransport
  RFC 1845 : SMTP Service Extension for Checkpoint/Restart
  RFC 1846 : SMTP 521 Reply Code
  RFC 1870 : SMTP Service Extension for Message Size Declaration
  RFC 1985 : SMTP Service Extension for Remote Message Queue Starting
  RFC 2034 : SMTP Service Extension for Returning Enhanced Error Codes
  RFC 2442 : The Batch SMTP Media Type
  RFC 2487 : SMTP Service Extension for Secure SMTP over TLS
  RFC 2505 : Anti-Spam Recommendations for SMTP MTAs
  RFC 2554 : SMTP Service Extension for Authentication
  RFC 2645 : ON-DEMAND MAIL RELAY (ODMR) SMTP with Dynamic IP Addresses
  RFC 2852 : Deliver By SMTP Service Extension
  RFC 2920 : SMTP Service Extension for Command Pipelining
  RFC 3030 : SMTP Service Extensions for Transmission of Large and Binary MIME
     Messages
*/

#define SMTP_STATUS_CONTINUE 0x1000

mailsmtp * mailsmtp_new(size_t progr_rate,
			progress_function * progr_fun)
{
  mailsmtp * session;

  session = malloc(sizeof(* session));
  if (session == NULL)
    goto err;

  session->stream = NULL;

  session->progr_rate = progr_rate;
  session->progr_fun = progr_fun;

  session->response = NULL;

  session->line_buffer = mmap_string_new("");
  if (session->line_buffer == NULL)
    goto free_session;

  session->response_buffer = mmap_string_new("");
  if (session->response_buffer == NULL)
    goto free_line_buffer;

  session->esmtp = 0;
  session->auth = MAILSMTP_AUTH_NOT_CHECKED;
  
#ifdef USE_SASL
  session->smtp_sasl.sasl_conn = NULL;
#endif
  
  return session;

 free_line_buffer:
  mmap_string_free(session->line_buffer);
 free_session:
  free(session);
 err:
  return NULL;
}

void mailsmtp_free(mailsmtp * session)
{
#ifdef USE_SASL
  if (session->smtp_sasl.sasl_conn != NULL) {
    sasl_dispose((sasl_conn_t **) &session->smtp_sasl.sasl_conn);
    mailsasl_unref();
  }
#endif
  
  if (session->stream)
    mailsmtp_quit(session);

  mmap_string_free(session->line_buffer);
  mmap_string_free(session->response_buffer);
  free(session);
}

static int send_command(mailsmtp * f, char * command);

static int read_response(mailsmtp * session);

/* smtp operations */

int mailsmtp_connect(mailsmtp * session, mailstream * s)
{
  int code;

  session->stream = s;

  code = read_response(session);

  switch (code) {
  case 220:
    return MAILSMTP_NO_ERROR;
      
  case 554:
    session->stream = NULL;
    mailstream_close(s);
    return MAILSMTP_ERROR_SERVICE_NOT_AVAILABLE;
      
  default:
    session->stream = NULL;
    mailstream_close(s);
    return MAILSMTP_ERROR_UNEXPECTED_CODE;
  }
}


#define SMTP_STRING_SIZE 513

int mailsmtp_quit(mailsmtp * session)
{
  char command[SMTP_STRING_SIZE];
  int r;
  int res;
  
  snprintf(command, SMTP_STRING_SIZE, "QUIT\r\n");
  r = send_command(session, command);
  if (r == -1) {
    res = MAILSMTP_ERROR_STREAM;
    goto close;
  }
  r = read_response(session);
  if (r == 0) {
    res = MAILSMTP_ERROR_STREAM;
    goto close;
  }
  
  res = MAILSMTP_NO_ERROR;  
  
 close:
  mailstream_close(session->stream);
  session->stream = NULL;
  
  return res;
}



#define HOSTNAME_SIZE 256

int mailsmtp_helo(mailsmtp * session)
{
  int r;
  char hostname[HOSTNAME_SIZE];
  char command[SMTP_STRING_SIZE];

  r = gethostname(hostname, HOSTNAME_SIZE);
  if (r < 0)
    return MAILSMTP_ERROR_HOSTNAME;

  snprintf(command, SMTP_STRING_SIZE, "HELO %s\r\n", hostname);
  r = send_command(session, command);
  if (r == -1)
    return MAILSMTP_ERROR_STREAM;
  r = read_response(session);
  
  switch (r) {
  case 250:
    session->esmtp = 0;
    session->auth = MAILSMTP_AUTH_NOT_CHECKED;
    return MAILSMTP_NO_ERROR;
    
  case 504:
    return MAILSMTP_ERROR_NOT_IMPLEMENTED;

  case 550:
    return MAILSMTP_ERROR_ACTION_NOT_TAKEN;

  case 0:
    return MAILSMTP_ERROR_STREAM;

  default:
    return MAILSMTP_ERROR_UNEXPECTED_CODE;
  }
}

int mailsmtp_mail(mailsmtp * session, const char * from)
{
  int r;
  char command[SMTP_STRING_SIZE];

  snprintf(command, SMTP_STRING_SIZE, "MAIL FROM:<%s>\r\n", from);
  r = send_command(session, command);
  if (r == -1)
    return MAILSMTP_ERROR_STREAM;
  r = read_response(session);

  switch (r) {
  case 250:
    return MAILSMTP_NO_ERROR;

  case 552:
    return MAILSMTP_ERROR_EXCEED_STORAGE_ALLOCATION;

  case 451:
    return MAILSMTP_ERROR_IN_PROCESSING;

  case 452:
    return MAILSMTP_ERROR_INSUFFICIENT_SYSTEM_STORAGE;

  case 550:
    return MAILSMTP_ERROR_MAILBOX_UNAVAILABLE;

  case 553:
    return MAILSMTP_ERROR_MAILBOX_NAME_NOT_ALLOWED;

  case 503:
    return MAILSMTP_ERROR_BAD_SEQUENCE_OF_COMMAND;

  case 0:
    return MAILSMTP_ERROR_STREAM;

  default:
    return MAILSMTP_ERROR_UNEXPECTED_CODE;
  }
}

int mailsmtp_rcpt(mailsmtp * session, const char * to)
{
  return mailesmtp_rcpt(session, to, 0, NULL);
}

int mailsmtp_data(mailsmtp * session)
{
  int r;
  char command[SMTP_STRING_SIZE];

  snprintf(command, SMTP_STRING_SIZE, "DATA\r\n");
  r = send_command(session, command);
  if (r == -1)
    return MAILSMTP_ERROR_STREAM;
  r = read_response(session);

  switch (r) {
  case 354:
    return MAILSMTP_NO_ERROR;

  case 451:
    return MAILSMTP_ERROR_IN_PROCESSING;

  case 554:
    return MAILSMTP_ERROR_TRANSACTION_FAILED;

  case 503:
    return MAILSMTP_ERROR_BAD_SEQUENCE_OF_COMMAND;

  default:
    return MAILSMTP_ERROR_UNEXPECTED_CODE;
  }
}

static int send_data(mailsmtp * session, const char * message, size_t size);

int mailsmtp_data_message(mailsmtp * session,
			   const char * message,
			   size_t size)
{
  int r;

  r = send_data(session, message, size);
  if (r == -1)
    return MAILSMTP_ERROR_STREAM;

  r = read_response(session);

  switch(r) {
  case 250:
    return MAILSMTP_NO_ERROR;
      
  case 552:
    return MAILSMTP_ERROR_EXCEED_STORAGE_ALLOCATION;

  case 554:
    return MAILSMTP_ERROR_TRANSACTION_FAILED;
      
  case 451:
    return MAILSMTP_ERROR_IN_PROCESSING;
      
  case 452:
    return MAILSMTP_ERROR_INSUFFICIENT_SYSTEM_STORAGE;

  case 0:
    return MAILSMTP_ERROR_STREAM;

  default:
    return MAILSMTP_ERROR_UNEXPECTED_CODE;
  }
}

/* esmtp operations */


/**
 * called during mailesmtp_ehlo
 * checks EHLO answer for server extensions and sets flags
 * in session->esmtp
 * checks AUTH methods in session->response and sets flags
 * in session->auth
 */
#define isdelim(x) ((x) == ' ' || (x) == '\r' || (x) == '\n' || (x) == '\0')

int mailesmtp_parse_ehlo(mailsmtp * session)
{
  char * response;
  
  /* restore data */
  session->esmtp = MAILSMTP_ESMTP;
  session->auth = MAILSMTP_AUTH_CHECKED;

  response = session->response;
  
  /* ESMTP supported extensions :
     DSN
     EXPN
     8BITMIME
     SIZE [<n>]
     ETRN
     STARTTLS
     AUTH <mechanisms...>
  */
  while (response != NULL) {
    if (!strncasecmp(response, "EXPN", 4) && isdelim(response[4])) 
      session->esmtp |= MAILSMTP_ESMTP_EXPN;
    else if (!strncasecmp(response, "ETRN", 4) && isdelim(response[4]))
      session->esmtp |= MAILSMTP_ESMTP_ETRN;
    else if (!strncasecmp(response, "DSN", 3) && isdelim(response[3]))
      session->esmtp |= MAILSMTP_ESMTP_DSN;
    else if (!strncasecmp(response, "8BITMIME", 8) && isdelim(response[8]))
      session->esmtp |= MAILSMTP_ESMTP_8BITMIME;
    else if (!strncasecmp(response, "STARTTLS", 8) && isdelim(response[8]))
      session->esmtp |= MAILSMTP_ESMTP_STARTTLS;
    else if (!strncasecmp(response, "SIZE", 4) && isdelim(response[4])) {
      session->esmtp |= MAILSMTP_ESMTP_SIZE;
      /* TODO: grab optionnal max size */
    } else if (!strncasecmp(response, "AUTH ", 5)) {
      response += 5;       /* remove "AUTH " */
      while (response[0] != '\n' && response[0] != '\0') {
	while (response[0] == ' ') response++;
	if (strncasecmp(response, "LOGIN", 5) == 0) {
	  session->auth |= MAILSMTP_AUTH_LOGIN;
	  response += 5;
	} else if (strncasecmp(response, "CRAM-MD5", 8) == 0) {
	  session->auth |= MAILSMTP_AUTH_CRAM_MD5;
	  response += 8;
	} else if (strncasecmp(response, "PLAIN", 5) == 0) {
	  session->auth |= MAILSMTP_AUTH_PLAIN;
	  response += 5;
	} else if (strncasecmp(response, "DIGEST-MD5", 10) == 0) {
	  session->auth |= MAILSMTP_AUTH_DIGEST_MD5;
	  response += 10;
	} else {
	  /* unknown auth method - jump to next word or eol */
	  while (!isdelim(response[0]) || response[0] == '\r')
	    response++;
	}
      }
    }
    response = strpbrk(response, "\n");
    if (response != NULL)
      response++;
  }
  
  return MAILSMTP_NO_ERROR;
}


int mailesmtp_ehlo(mailsmtp * session)
{
  int r;
  char hostname[HOSTNAME_SIZE];
  char command[SMTP_STRING_SIZE];

  r = gethostname(hostname, HOSTNAME_SIZE);
  if (r != 0)
    return MAILSMTP_ERROR_HOSTNAME;

  snprintf(command, SMTP_STRING_SIZE, "EHLO %s\r\n", hostname);
  r = send_command(session, command);
  if (r == -1)
    return MAILSMTP_ERROR_STREAM;
  r = read_response(session);
  
  switch (r) {
  case 250:
    return mailesmtp_parse_ehlo(session);
    
  case 504:
    return MAILSMTP_ERROR_NOT_IMPLEMENTED;

  case 550:
    return MAILSMTP_ERROR_ACTION_NOT_TAKEN;

  case 0:
    return MAILSMTP_ERROR_STREAM;

  default:
    return MAILSMTP_ERROR_UNEXPECTED_CODE;
  }
}

/*
  if return_full is TRUE, the entire message is returned on error
  envid can be NULL
*/


int mailesmtp_mail(mailsmtp * session,
		    const char * from,
		    int return_full,
		    const char * envid)
{
  int r;
  char command[SMTP_STRING_SIZE];
  char *body = "";

#ifdef notyet
  /* TODO: figure out a way for the user to explicity enable this or not */
  if (session->esmtp & MAILSMTP_ESMTP_8BITMIME)
    body = " BODY=8BITMIME";
#endif
  
  if (session->esmtp & MAILSMTP_ESMTP_DSN) {
    if (envid)
      snprintf(command, SMTP_STRING_SIZE, "MAIL FROM:<%s> RET=%s ENVID=%s%s\r\n",
	       from, return_full ? "FULL" : "HDRS", envid, body);
    else
      snprintf(command, SMTP_STRING_SIZE, "MAIL FROM:<%s> RET=%s%s\r\n",
	       from, return_full ? "FULL" : "HDRS", body);
  } else
    snprintf(command, SMTP_STRING_SIZE, "MAIL FROM:<%s>%s\r\n",
	     from, body);

  r = send_command(session, command);
  if (r == -1)
    return MAILSMTP_ERROR_STREAM;
  r = read_response(session);

  switch (r) {
  case 250:
    return MAILSMTP_NO_ERROR;

  case 552:
    return MAILSMTP_ERROR_EXCEED_STORAGE_ALLOCATION;

  case 451:
    return MAILSMTP_ERROR_IN_PROCESSING;

  case 452:
    return MAILSMTP_ERROR_INSUFFICIENT_SYSTEM_STORAGE;

  case 550:
    return MAILSMTP_ERROR_MAILBOX_UNAVAILABLE;

  case 553:
    return MAILSMTP_ERROR_MAILBOX_NAME_NOT_ALLOWED;

  case 503:
    return MAILSMTP_ERROR_BAD_SEQUENCE_OF_COMMAND;

  case 0:
    return MAILSMTP_ERROR_STREAM;

  default:
    return MAILSMTP_ERROR_UNEXPECTED_CODE;
  }
}

int mailesmtp_rcpt(mailsmtp * session,
		    const char * to,
		    int notify,
		    const char * orcpt)
{
  int r;
  char command[SMTP_STRING_SIZE];
  char notify_str[30] = "";
  char notify_info_str[30] = "";

  if (notify != 0 && session->esmtp & MAILSMTP_ESMTP_DSN) {
    if (notify & MAILSMTP_DSN_NOTIFY_SUCCESS)
      strcat(notify_info_str, ",SUCCESS");
    if (notify & MAILSMTP_DSN_NOTIFY_FAILURE)
      strcat(notify_info_str, ",FAILURE");
    if (notify & MAILSMTP_DSN_NOTIFY_DELAY)
      strcat(notify_info_str, ",DELAY");

    if (notify & MAILSMTP_DSN_NOTIFY_NEVER)
      strcpy(notify_info_str, ",NEVER");

    notify_info_str[0] = '=';

    strcpy(notify_str, " NOTIFY");
    strcat(notify_str, notify_info_str);
  }

  if (orcpt && session->esmtp & MAILSMTP_ESMTP_DSN)
    snprintf(command, SMTP_STRING_SIZE, "RCPT TO:<%s>%s ORCPT=%s\r\n",
	     to, notify_str, orcpt);
  else
    snprintf(command, SMTP_STRING_SIZE, "RCPT TO:<%s>%s\r\n", to, notify_str);

  r = send_command(session, command);
  if (r == -1)
    return MAILSMTP_ERROR_STREAM;
  r = read_response(session);

  switch (r) {
  case 250:
    return MAILSMTP_NO_ERROR;

  case 251: /* not local user, will be forwarded */
    return MAILSMTP_NO_ERROR;

  case 550:
  case 450:
    return MAILSMTP_ERROR_MAILBOX_UNAVAILABLE;

  case 551:
    return MAILSMTP_ERROR_USER_NOT_LOCAL;

  case 552:
    return MAILSMTP_ERROR_EXCEED_STORAGE_ALLOCATION;

  case 553:
    return MAILSMTP_ERROR_MAILBOX_NAME_NOT_ALLOWED;

  case 451:
    return MAILSMTP_ERROR_IN_PROCESSING;

  case 452:
    return MAILSMTP_ERROR_INSUFFICIENT_SYSTEM_STORAGE;

  case 503:
    return MAILSMTP_ERROR_BAD_SEQUENCE_OF_COMMAND;

  case 0:
    return MAILSMTP_ERROR_STREAM;

  default:
    return MAILSMTP_ERROR_UNEXPECTED_CODE;
  }
}

int auth_map_errors(int err)
{
  switch (err) {
  case 235:
    return MAILSMTP_NO_ERROR;     /* AUTH successfull */
  case 334:
    return MAILSMTP_NO_ERROR;     /* AUTH in progress */
  case 432:
    return MAILSMTP_ERROR_AUTH_TRANSITION_NEEDED;
  case 454:
    return MAILSMTP_ERROR_AUTH_TEMPORARY_FAILTURE;
  case 504:
    return MAILSMTP_ERROR_AUTH_NOT_SUPPORTED;
  case 530:
    return MAILSMTP_ERROR_AUTH_REQUIRED;
  case 534:
    return MAILSMTP_ERROR_AUTH_TOO_WEAK;
  case 535:
    return MAILSMTP_ERROR_AUTH_AUTHENTICATION_FAILED;
  case 538:
    return MAILSMTP_ERROR_AUTH_ENCRYPTION_REQUIRED;
  default:
    /* opportunistic approach ;) */
    return MAILSMTP_NO_ERROR;
  }
}

static int mailsmtp_auth_login(mailsmtp * session,
    const char * user, const char * pass)
{
  int err;
  char command[SMTP_STRING_SIZE];
  char * user64, * pass64;
  
  user64 = NULL;
  pass64 = NULL;
  
  user64 = encode_base64(user, strlen(user));
  if (user64 == NULL) {
    err = MAILSMTP_ERROR_MEMORY;
    goto err_free;
  }
  
  pass64 = encode_base64(pass, strlen(pass));
  if (pass64 == NULL) {
    err = MAILSMTP_ERROR_MEMORY;
    goto err_free;
  }
  
  snprintf(command, SMTP_STRING_SIZE, "%s\r\n", user64);
  err = send_command(session, command);
  if (err == -1) {
    err = MAILSMTP_ERROR_STREAM;
    goto err_free;
  }
  err = read_response(session);
  err = auth_map_errors(err);
  if (err != MAILSMTP_NO_ERROR)
    goto err_free;
  
  snprintf(command, SMTP_STRING_SIZE, "%s\r\n", pass64);
  err = send_command(session, command);
  if (err == -1) {
    err = MAILSMTP_ERROR_STREAM;
    goto err_free;
  }
  err = read_response(session);
  err = auth_map_errors(err);
  
 err_free:
  free(user64);
  free(pass64);
  
  return err;
}

#if 0
static int mailsmtp_auth_plain(mailsmtp * session,
    const char * user, const char * pass)
{
  int err, len;
  char command[SMTP_STRING_SIZE];
  char * plain, * plain64;

  len = strlen(user) + strlen(pass) + 3;
  plain = (char *) malloc(len);
  if (plain == NULL) {
    err = MAILSMTP_ERROR_MEMORY;
    goto err;
  }
  
  snprintf(plain, len, "%c%s%c%s", '\0', user, '\0', pass);
  plain64 = encode_base64(plain, len - 1);

  snprintf(command, SMTP_STRING_SIZE, "%s\r\n", plain64);
  err = send_command(session, command);
  if (err == -1) {
    err = MAILSMTP_ERROR_STREAM;
    goto err_free;
  }

  err = read_response(session);
  err = auth_map_errors(err);
  
err_free:
  free(plain64);
  free(plain);

 err:
  return err;
}

static char * convert_hex(unsigned char *in, int len)
{
  static char hex[] = "0123456789abcdef";
  char * out;
  int i;

  out = (char *) malloc(len * 2 + 1);
  if (out == NULL)
    return NULL;

  for (i = 0; i < len; i++) {
    out[i * 2] = hex[in[i] >> 4];
    out[i * 2 + 1] = hex[in[i] & 15];
  }

  out[i*2] = 0;
  
  return out;
}

static char * hash_md5(const char * sec_key, const char * data, int len)
{
  char key[65], digest[24];
  char * hash_hex;
  
  int sec_len, i;

  sec_len = strlen(sec_key);
  
  if (sec_len < 64) {
    memcpy(key, sec_key, sec_len);      
    for (i = sec_len; i < 64; i++) {
      key[i] = 0;
    }
  } else {
    memcpy(key, sec_key, 64);      
  }

  hmac_md5(data, len, key, 64, digest);
  hash_hex = convert_hex((unsigned char *) digest, 16);

  return hash_hex;
}

static int mailsmtp_auth_cram_md5(mailsmtp * session,
    const char * user, const char * pass)
{
  int err;
  char command[SMTP_STRING_SIZE];
  char *response, *auth_hex, *auth;
  
  response = decode_base64(session->response, strlen(session->response));
  if (response == NULL) return MAILSMTP_ERROR_MEMORY;
  
  auth_hex = hash_md5(pass, response, strlen(response));
  if (auth_hex == NULL) {
    err = MAILSMTP_ERROR_MEMORY;
    goto err_free_response;
  }
  
  snprintf(command, SMTP_STRING_SIZE, "%s %s", user, auth_hex);
  
  auth = encode_base64(command, strlen(command));
  if (auth == NULL) {
    err = MAILSMTP_ERROR_MEMORY;
    goto err_free_auth_hex;
  }

  snprintf(command, SMTP_STRING_SIZE, "%s\r\n", auth);
  err = send_command(session, command);
  if (err == -1) {
    err = MAILSMTP_ERROR_STREAM;
    goto err_free;
  }

  err = read_response(session);
  err = auth_map_errors(err);
  
err_free:  
  free(auth);
err_free_auth_hex:
  free(auth_hex);
err_free_response:
  free(response);
  return err;
}
#endif

int mailsmtp_auth_type(mailsmtp * session,
    const char * user, const char * pass, int type)
{
#if 0 /* deprecated */
  int err;
  char command[SMTP_STRING_SIZE];
  
  if (session->auth == MAILSMTP_AUTH_NOT_CHECKED) 
    return MAILSMTP_ERROR_BAD_SEQUENCE_OF_COMMAND;
    
  if ( !(session->auth & type) ) return MAILSMTP_ERROR_AUTH_NOT_SUPPORTED;
  
  switch (type) {
  case MAILSMTP_AUTH_LOGIN:
    snprintf(command, SMTP_STRING_SIZE, "AUTH LOGIN\r\n");
    break;
  case MAILSMTP_AUTH_PLAIN:
    snprintf(command, SMTP_STRING_SIZE, "AUTH PLAIN\r\n");
    break;
  case MAILSMTP_AUTH_CRAM_MD5:
    snprintf(command, SMTP_STRING_SIZE, "AUTH CRAM-MD5\r\n");
    break;
  default:
    return MAILSMTP_ERROR_NOT_IMPLEMENTED;
  }
  
  err = send_command(session, command);
  if (err == -1) return MAILSMTP_ERROR_STREAM;

  err = read_response(session);
  err = auth_map_errors(err);
  if (err != MAILSMTP_NO_ERROR) return err;
  
  switch (type) {
  case MAILSMTP_AUTH_LOGIN:
    return mailsmtp_auth_login(session, user, pass);
  case MAILSMTP_AUTH_PLAIN:
    return mailsmtp_auth_plain(session, user, pass);
  case MAILSMTP_AUTH_CRAM_MD5:
    return mailsmtp_auth_cram_md5(session, user, pass);
  default:
    return MAILSMTP_ERROR_NOT_IMPLEMENTED;
  }
#endif
  int err;
  char command[SMTP_STRING_SIZE];
  char hostname[SMTP_STRING_SIZE];
  
  err = gethostname(hostname, sizeof(hostname));
  if (err < 0) {
    return MAILSMTP_ERROR_MEMORY;
  }
  
  if (session->auth == MAILSMTP_AUTH_NOT_CHECKED) 
    return MAILSMTP_ERROR_BAD_SEQUENCE_OF_COMMAND;
    
  if ( !(session->auth & type) ) return MAILSMTP_ERROR_AUTH_NOT_SUPPORTED;
  
  switch (type) {
  case MAILSMTP_AUTH_LOGIN:
    {
      snprintf(command, SMTP_STRING_SIZE, "AUTH LOGIN\r\n");
      err = send_command(session, command);
      if (err == -1) return MAILSMTP_ERROR_STREAM;
      
      err = read_response(session);
      err = auth_map_errors(err);
      if (err != MAILSMTP_NO_ERROR) return err;
      
      return mailsmtp_auth_login(session, user, pass);
    }
  case MAILSMTP_AUTH_PLAIN:
    return mailesmtp_auth_sasl(session, "PLAIN",
        hostname, NULL, NULL, user, user, pass, NULL);
    
  case MAILSMTP_AUTH_CRAM_MD5:
    return mailesmtp_auth_sasl(session, "CRAM-MD5",
        hostname, NULL, NULL, user, user, pass, NULL);
    
  case MAILSMTP_AUTH_DIGEST_MD5:
    return mailesmtp_auth_sasl(session, "DIGEST-MD5",
        hostname, NULL, NULL, user, user, pass, NULL);
    
  default:
    return MAILSMTP_ERROR_NOT_IMPLEMENTED;
  }

}


int mailsmtp_auth(mailsmtp * session, const char * user, const char * pass)
{
  if (session->auth == MAILSMTP_AUTH_NOT_CHECKED) 
    return MAILSMTP_ERROR_BAD_SEQUENCE_OF_COMMAND;
    
  if (session->auth & MAILSMTP_AUTH_DIGEST_MD5) {
    return mailsmtp_auth_type(session, user, pass, MAILSMTP_AUTH_DIGEST_MD5);
  } else if (session->auth & MAILSMTP_AUTH_CRAM_MD5) {
    return mailsmtp_auth_type(session, user, pass, MAILSMTP_AUTH_CRAM_MD5);
  } else if (session->auth & MAILSMTP_AUTH_PLAIN) {
    return mailsmtp_auth_type(session, user, pass, MAILSMTP_AUTH_PLAIN);
  } else if (session->auth & MAILSMTP_AUTH_LOGIN) {
    return mailsmtp_auth_type(session, user, pass, MAILSMTP_AUTH_LOGIN);
  } else {
    return MAILSMTP_ERROR_AUTH_NOT_SUPPORTED;
  }
}

/* TODO: add mailesmtp_etrn, mailssmtp_expn */

int mailesmtp_starttls(mailsmtp * session) {
  int r;

  if (!(session->esmtp & MAILSMTP_ESMTP_STARTTLS))
    return MAILSMTP_ERROR_STARTTLS_NOT_SUPPORTED;

  r = send_command(session, "STARTTLS\r\n");
  if (r == -1)
    return MAILSMTP_ERROR_STREAM;
  r = read_response(session);

  switch (r) {
  case 220:
    return MAILSMTP_NO_ERROR;

  case 454:
    return MAILSMTP_ERROR_STARTTLS_TEMPORARY_FAILURE;

  default:
    return MAILSMTP_ERROR_UNEXPECTED_CODE;
  }
}

static int parse_response(mailsmtp * session,
			   char * response)
{
  char * message;
  int code;
  int cont = 0;

  code = strtol(response, &message, 10);
  if (* message == ' ')
    mmap_string_append(session->response_buffer, message + 1);
  else if (* message == '-') {
    cont = SMTP_STATUS_CONTINUE;
    mmap_string_append(session->response_buffer, message + 1);
  }
  else
    mmap_string_append(session->response_buffer, message);
      
  return code | cont;
}

static char * read_line(mailsmtp * session)
{
  return mailstream_read_line_remove_eol(session->stream,
					 session->line_buffer);
}

static int read_response(mailsmtp * session)
{
  char * line;
  int code;

  mmap_string_assign(session->response_buffer, "");

  do {
    line = read_line(session);

    if (line != NULL) {
	code = parse_response(session, line);
        mmap_string_append_c(session->response_buffer, '\n');
      }
    else
      code = 0;
  }
  while ((code & SMTP_STATUS_CONTINUE) != 0);

  session->response = session->response_buffer->str;

  return code;
}





static int send_command(mailsmtp * f, char * command)
{
  ssize_t r;

  r = mailstream_write(f->stream, command, strlen(command));
  if (r == -1)
    return -1;

  r = mailstream_flush(f->stream);
  if (r == -1)
    return -1;

  return 0;
}

static int send_data(mailsmtp * session, const char * message, size_t size)
{
  if (mailstream_send_data(session->stream, message, size,
			   session->progr_rate, session->progr_fun) == -1)
    return -1;

  if (mailstream_flush(session->stream) == -1)
    return -1;

  return 0;
}


const char * mailsmtp_strerror(int errnum)
{
  switch (errnum)    {
  case MAILSMTP_NO_ERROR:
    return "No error";
  case MAILSMTP_ERROR_UNEXPECTED_CODE:
    return "Unexpected error code";
  case MAILSMTP_ERROR_SERVICE_NOT_AVAILABLE:
    return "Service not available";
  case MAILSMTP_ERROR_STREAM:
    return "Stream error";
  case MAILSMTP_ERROR_HOSTNAME:
    return "gethostname() failed";
  case MAILSMTP_ERROR_NOT_IMPLEMENTED:
    return "Not implemented";
  case MAILSMTP_ERROR_ACTION_NOT_TAKEN:
    return "Error, action not taken";
  case MAILSMTP_ERROR_EXCEED_STORAGE_ALLOCATION:
    return "Data exceeds storage allocation";
  case MAILSMTP_ERROR_IN_PROCESSING:
    return "Error in processing";
  case MAILSMTP_ERROR_INSUFFICIENT_SYSTEM_STORAGE:
    return "Insufficient system storage";
  case MAILSMTP_ERROR_MAILBOX_UNAVAILABLE:
    return "Mailbox unavailable";
  case MAILSMTP_ERROR_MAILBOX_NAME_NOT_ALLOWED:
    return "Mailbox name not allowed";
  case MAILSMTP_ERROR_BAD_SEQUENCE_OF_COMMAND:
    return "Bad command sequence";
  case MAILSMTP_ERROR_USER_NOT_LOCAL:
    return "User not local";
  case MAILSMTP_ERROR_TRANSACTION_FAILED:
    return "Transaction failed";
  case MAILSMTP_ERROR_MEMORY:
    return "Memory error";
  case MAILSMTP_ERROR_CONNECTION_REFUSED:
    return "Connection refused";
  case MAILSMTP_ERROR_STARTTLS_TEMPORARY_FAILURE:
    return "TLS not available on server for temporary reason";
  case MAILSMTP_ERROR_STARTTLS_NOT_SUPPORTED:
    return "TLS not supported by server";
  default:
    return "Unknown error code";
  }
}


#ifdef USE_SASL
static int sasl_getsimple(void * context, int id,
    const char ** result, unsigned * len)
{
  mailsmtp * session;
  
  session = context;
  
  switch (id) {
  case SASL_CB_USER:
    if (result != NULL)
      * result = session->smtp_sasl.sasl_login;
    if (len != NULL)
      * len = strlen(session->smtp_sasl.sasl_login);
    return SASL_OK;
    
  case SASL_CB_AUTHNAME:
    if (result != NULL)
      * result = session->smtp_sasl.sasl_auth_name;
    if (len != NULL)
      * len = strlen(session->smtp_sasl.sasl_auth_name);
    return SASL_OK;
  }
  
  return SASL_FAIL;
}

static int sasl_getsecret(sasl_conn_t * conn, void * context, int id,
    sasl_secret_t ** psecret)
{
  mailsmtp * session;
  
  session = context;
  
  switch (id) {
  case SASL_CB_PASS:
    if (psecret != NULL)
      * psecret = session->smtp_sasl.sasl_secret;
    return SASL_OK;
  }
  
  return SASL_FAIL;
}

static int sasl_getrealm(void * context, int id,
    const char ** availrealms,
    const char ** result)
{
  mailsmtp * session;
  
  session = context;
  
  switch (id) {
  case SASL_CB_GETREALM:
    if (result != NULL)
      * result = session->smtp_sasl.sasl_realm;
    return SASL_OK;
  }
  
  return SASL_FAIL;
}
#endif

int mailesmtp_auth_sasl(mailsmtp * session, const char * auth_type,
    const char * server_fqdn,
    const char * local_ip_port,
    const char * remote_ip_port,
    const char * login, const char * auth_name,
    const char * password, const char * realm)
{
#ifdef USE_SASL
  int r;
  char command[SMTP_STRING_SIZE];
  sasl_callback_t sasl_callback[5];
  const char * sasl_out;
  unsigned sasl_out_len;
  const char * mechusing;
  sasl_secret_t * secret;
  int res;
  size_t len;
  char * encoded;
  unsigned int encoded_len;
  unsigned int max_encoded;
  
  sasl_callback[0].id = SASL_CB_GETREALM;
  sasl_callback[0].proc =  sasl_getrealm;
  sasl_callback[0].context = session;
  sasl_callback[1].id = SASL_CB_USER;
  sasl_callback[1].proc =  sasl_getsimple;
  sasl_callback[1].context = session;
  sasl_callback[2].id = SASL_CB_AUTHNAME;
  sasl_callback[2].proc =  sasl_getsimple;
  sasl_callback[2].context = session; 
  sasl_callback[3].id = SASL_CB_PASS;
  sasl_callback[3].proc =  sasl_getsecret;
  sasl_callback[3].context = session;
  sasl_callback[4].id = SASL_CB_LIST_END;
  sasl_callback[4].proc =  NULL;
  sasl_callback[4].context = NULL;
  
  len = strlen(password);
  secret = malloc(sizeof(* secret) + len);
  if (secret == NULL) {
    res = MAILSMTP_ERROR_MEMORY;
    goto err;
  }
  secret->len = len;
  memcpy(secret->data, password, len + 1);
  
  session->smtp_sasl.sasl_server_fqdn = server_fqdn;
  session->smtp_sasl.sasl_login = login;
  session->smtp_sasl.sasl_auth_name = auth_name;
  session->smtp_sasl.sasl_password = password;
  session->smtp_sasl.sasl_realm = realm;
  session->smtp_sasl.sasl_secret = secret;
  
  /* init SASL */
  if (session->smtp_sasl.sasl_conn != NULL) {
    sasl_dispose((sasl_conn_t **) &session->smtp_sasl.sasl_conn);
    session->smtp_sasl.sasl_conn = NULL;
  }
  else {
    mailsasl_ref();
  }
  
  r = sasl_client_new("smtp", server_fqdn,
      local_ip_port, remote_ip_port, sasl_callback, 0,
      (sasl_conn_t **) &session->smtp_sasl.sasl_conn);
  if (r != SASL_OK) {
    res = MAILSMTP_ERROR_AUTH_LOGIN;
    goto free_secret;
  }
  
  r = sasl_client_start(session->smtp_sasl.sasl_conn,
      auth_type, NULL, &sasl_out, &sasl_out_len, &mechusing);
  if ((r != SASL_CONTINUE) && (r != SASL_OK)) {
    res = MAILSMTP_ERROR_AUTH_LOGIN;
    goto free_sasl_conn;
  }
  
  if (sasl_out_len != 0) {
    max_encoded = ((sasl_out_len + 2) / 3) * 4;
    encoded = malloc(max_encoded + 1);
    if (encoded == NULL) {
      res = MAILSMTP_ERROR_MEMORY;
      goto free_sasl_conn;
    }
    
    r = sasl_encode64(sasl_out, sasl_out_len,
        encoded, max_encoded + 1, &encoded_len);
    if (r != SASL_OK) {
      free(encoded);
      res = MAILSMTP_ERROR_MEMORY;
      goto free_sasl_conn;
    }
    
    snprintf(command, SMTP_STRING_SIZE, "AUTH %s %s\r\n", auth_type, encoded);
    
    free(encoded);
  }
  else {
    snprintf(command, SMTP_STRING_SIZE, "AUTH %s\r\n", auth_type);
  }
  
  r = send_command(session, command);
  if (r == -1) {
    res = MAILSMTP_ERROR_STREAM;
    goto free_sasl_conn;
  }
  
  while (1) {
    r = read_response(session);
    switch (r) {
    case 220:
    case 235:
      res = MAILSMTP_NO_ERROR;
      goto free_sasl_conn;
      
    case 334:
      {
        size_t response_len;
        char * decoded;
        unsigned int decoded_len;
        unsigned int max_decoded;
        
        response_len = strlen(session->response);
        max_decoded = response_len * 3 / 4;
        decoded = malloc(max_decoded + 1);
        if (decoded == NULL) {
          res = MAILSMTP_ERROR_MEMORY;
          goto free_sasl_conn;
        }
        
        r = sasl_decode64(session->response, response_len,
            decoded, max_decoded + 1, &decoded_len);
        
        if (r != SASL_OK) {
          free(decoded);
          res = MAILSMTP_ERROR_MEMORY;
          goto free_sasl_conn;
        }
        
        r = sasl_client_step(session->smtp_sasl.sasl_conn,
            decoded, decoded_len, NULL, &sasl_out, &sasl_out_len);
        
        free(decoded);
        
        if ((r != SASL_CONTINUE) && (r != SASL_OK)) {
          res = MAILSMTP_ERROR_AUTH_LOGIN;
          goto free_sasl_conn;
        }
        
        max_encoded = ((sasl_out_len + 2) / 3) * 4;
        encoded = malloc(max_encoded + 1);
        if (encoded == NULL) {
          res = MAILSMTP_ERROR_MEMORY;
          goto free_sasl_conn;
        }
        
        r = sasl_encode64(sasl_out, sasl_out_len,
            encoded, max_encoded + 1, &encoded_len);
        if (r != SASL_OK) {
          free(encoded);
          res = MAILSMTP_ERROR_MEMORY;
          goto free_sasl_conn;
        }
        
        snprintf(command, SMTP_STRING_SIZE, "%s\r\n", encoded);
        r = send_command(session, command);
        
        free(encoded);
        
        if (r == -1) {
          res = MAILSMTP_ERROR_STREAM;
          goto free_sasl_conn;
        }
      }
      break;
      
    default:
      res = auth_map_errors(r);
      goto free_sasl_conn;
    }
  }
  
  res = MAILSMTP_NO_ERROR;
  
 free_sasl_conn:
  sasl_dispose((sasl_conn_t **) &session->smtp_sasl.sasl_conn);
  session->smtp_sasl.sasl_conn = NULL;
  mailsasl_unref();
 free_secret:
  free(session->smtp_sasl.sasl_secret);
  session->smtp_sasl.sasl_secret = NULL;
 err:
  return res;
#else
  return MAILSMTP_ERROR_NOT_IMPLEMENTED;
#endif
}


int mailsmtp_noop(mailsmtp * session)
{
  char command[SMTP_STRING_SIZE];
  int r;

  snprintf(command, SMTP_STRING_SIZE, "NOOP\r\n");
  r = send_command(session, command);
  if (r == -1)
    return MAILSMTP_ERROR_STREAM;
  r = read_response(session);
  if (r == 0)
    return MAILSMTP_ERROR_STREAM;
  
  return MAILSMTP_NO_ERROR;
}

int mailsmtp_reset(mailsmtp * session)
{
  char command[SMTP_STRING_SIZE];
  int r;

  snprintf(command, SMTP_STRING_SIZE, "RSET\r\n");
  r = send_command(session, command);
  if (r == -1)
    return MAILSMTP_ERROR_STREAM;
  r = read_response(session);
  if (r == 0)
    return MAILSMTP_ERROR_STREAM;
  
  return MAILSMTP_NO_ERROR;
}
