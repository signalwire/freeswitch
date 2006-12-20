/*
 * libEtPan! -- a mail library
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
 * $Id: mailprivacy_gnupg.c,v 1.10 2006/06/26 11:50:27 hoa Exp $
 */

/* passphrase is needed when private key is needed
   private key is needed :
   - to sign a message
   - and to decrypt a message
*/

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailprivacy_gnupg.h"

#include "mailprivacy.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <sys/wait.h>
#include <stdlib.h>
#include "mailprivacy_tools.h"
#include <libetpan/mailmime.h>
#include <libetpan/libetpan-config.h>
#ifdef LIBETPAN_REENTRANT
#include <pthread.h>
#endif
#include <ctype.h>


enum {
  NO_ERROR_PGP = 0,
  ERROR_PGP_CHECK,
  ERROR_PGP_COMMAND,
  ERROR_PGP_FILE,
  ERROR_PGP_NOPASSPHRASE
};

static int mailprivacy_gnupg_add_encryption_id(struct mailprivacy * privacy,
    mailmessage * msg, char * encryption_id);
static char * get_passphrase(struct mailprivacy * privacy,
    char * user_id);
static int get_userid(char * filename, char * username, size_t length);

static int gpg_command_passphrase(struct mailprivacy * privacy,
    struct mailmessage * msg,
    char * command, char * userid,
    char * stdoutfile, char * stderrfile)
{
  pid_t pid;
  int passphrase_input[2];
  int fd_err;
  int fd_out;
  int r;
  char * passphrase;
  int bad_passphrase;
  int res;
  
  bad_passphrase = 0;
  
  passphrase = NULL;
  if (userid != NULL)
    passphrase = get_passphrase(privacy, userid);
  
  fd_out = open(stdoutfile, O_RDWR | O_CREAT | O_TRUNC, 0600);
  if (fd_out < 0) {
    res = ERROR_PGP_FILE;
    goto err;
  }
  
  fd_err = open(stderrfile, O_RDWR | O_CREAT | O_TRUNC, 0600);
  if (fd_err < 0) {
    res = ERROR_PGP_FILE;
    goto close_out;
  }
  
  r = pipe(passphrase_input);
  if (r < 0) {
    res = ERROR_PGP_FILE;
    goto close_err;
  }
  
  pid = fork();
  switch (pid) {
  case -1:
    return ERROR_PGP_COMMAND;
    
  case 0:
    /* child */
    {
      int status;
      
      /* close unneeded fd */
      close(passphrase_input[1]);
      
      dup2(passphrase_input[0], 0);
      close(passphrase_input[0]);
      dup2(fd_out, 1);
      close(fd_out);
      dup2(fd_err, 2);
      close(fd_err);
      
      status = system(command);
      
      exit(WEXITSTATUS(status));
    }
    break;
    
  default:
    /* parent */
    {
      int status;
      
      /* close unneeded fd */
      close(fd_err);
      close(fd_out);
      close(passphrase_input[0]);
      
      if (passphrase != NULL) {
        write(passphrase_input[1], passphrase, strlen(passphrase));
      }
      close(passphrase_input[1]);
      
      waitpid(pid, &status, 0);
      
      if (WEXITSTATUS(status) != 0)
        bad_passphrase = 1;
    }
    break;
  }
  
  if (bad_passphrase && (userid == NULL)) {
    char encryption_id[4096];
    
    encryption_id[0] = '\0';
    r = get_userid(stderrfile, encryption_id, sizeof(encryption_id));
    if (r == 0) {
      passphrase = get_passphrase(privacy, encryption_id);
      if (passphrase == NULL) {
        mailprivacy_gnupg_add_encryption_id(privacy, msg, encryption_id);
        return ERROR_PGP_NOPASSPHRASE;
      }
      else {
        return gpg_command_passphrase(privacy, msg, command, encryption_id,
            stdoutfile, stderrfile);
      }
    }
    else {
      return ERROR_PGP_CHECK;
    }
  }
  
  if (bad_passphrase && (passphrase != NULL)) {
    return ERROR_PGP_CHECK;
  }
  
  if (bad_passphrase) {
    mailprivacy_gnupg_add_encryption_id(privacy, msg, userid);
    return ERROR_PGP_NOPASSPHRASE;
  }
  
  return NO_ERROR_PGP;
  
 close_err:
  close(fd_err);
 close_out:
  close(fd_out);
 err:
  return res;
}

static int pgp_is_encrypted(struct mailmime * mime)
{
  if (mime->mm_content_type != NULL) {
    clistiter * cur;
    
    if (strcasecmp(mime->mm_content_type->ct_subtype, "encrypted") != 0)
      return 0;
    
    for(cur = clist_begin(mime->mm_content_type->ct_parameters) ; cur != NULL ;
        cur = clist_next(cur)) {
      struct mailmime_parameter * param;
      
      param = clist_content(cur);
      
      if ((strcasecmp(param->pa_name, "protocol") == 0) &&
          (strcasecmp(param->pa_value, "application/pgp-encrypted") == 0))
        return 1;
    }
  }
  
  return 0;
}

static int pgp_is_signed(struct mailmime * mime)
{
  if (mime->mm_content_type != NULL) {
    clistiter * cur;
    
    if (strcasecmp(mime->mm_content_type->ct_subtype, "signed") != 0)
      return 0;
    
    for(cur = clist_begin(mime->mm_content_type->ct_parameters) ;
        cur != NULL ; cur = clist_next(cur)) {
      struct mailmime_parameter * param;
      
      param = clist_content(cur);
      
      if ((strcasecmp(param->pa_name, "protocol") == 0) &&
          (strcasecmp(param->pa_value, "application/pgp-signature") == 0))
        return 1;
    }
  }
  
  return 0;
}

#define PGP_SIGNED "-----BEGIN PGP SIGNED MESSAGE-----"

int pgp_is_clearsigned(char * data, size_t len)
{
  if (len >= strlen(PGP_SIGNED))
    if (strncmp(data, PGP_SIGNED, sizeof(PGP_SIGNED) - 1) == 0)
      return 1;

  return 0;
}

#define PGP_CRYPTED "-----BEGIN PGP MESSAGE-----"

int pgp_is_crypted_armor(char * data, size_t len)
{
  if (len >= strlen(PGP_CRYPTED))
    if (strncmp(data, PGP_CRYPTED, sizeof(PGP_CRYPTED) - 1) == 0)
      return 1;

  return 0;
}


#if 0
#define BUF_SIZE 1024

/* write output to a file */

static int get_pgp_output(FILE * dest_f, char * command)
{
  FILE * p;
  char buf[BUF_SIZE];
  size_t size;
  int res;
  int status;
  char command_redirected[PATH_MAX];
  
  snprintf(command_redirected, sizeof(command_redirected), "%s 2>&1", command);
  
  /*
    flush buffer so that it is not flushed more than once when forking
  */
  fflush(dest_f);
  
  p = popen(command_redirected, "r");
  if (p == NULL) {
    res = ERROR_PGP_COMMAND;
    goto err;
  }
  
  while ((size = fread(buf, 1, sizeof(buf), p)) != 0) {
    size_t written;
    
    written = fwrite(buf, 1, size, dest_f);
    if (written != size) {
      res = ERROR_PGP_FILE;
      goto close;
    }
  }
  status = pclose(p);
  
  if (WEXITSTATUS(status) != 0)
    return ERROR_PGP_CHECK;
  else
    return NO_ERROR_PGP;
  
 close:
  pclose(p);
 err:
  return res;
}
#endif

/* parse output */

enum {
  STATE_USERID,
  STATE_NORMAL
};

static int get_userid(char * filename, char * username, size_t length)
{
  FILE * f;
  int state;
  char buffer[4096];
  int exit_code;
  
  exit_code = -1;
  
  f = fopen(filename, "r");
  if (f == NULL)
    goto exit;
  
  state = STATE_NORMAL;
  while (fgets(buffer, sizeof(buffer), f) != NULL) {
    
    switch (state) {
    case STATE_NORMAL:
      if (strncmp(buffer, "gpg: encrypted", 14) == 0)
        state = STATE_USERID;
      break;
      
    case STATE_USERID:
      {
        struct mailimf_mailbox * mb;
        size_t current_index;
        int r;
        size_t buflen;
        size_t i;
        char * beginning;
        
        /* find double-quotes and remove beginning and ending */
        
        buflen = strlen(buffer);
        for(i = buflen - 1 ; 1 ; i --) {
          if (buffer[i] == '\"') {
            buffer[i] = '\0';
            break;
          }
          
          if (i == 0)
            break;
        }
        
        beginning = buffer;
        for(i = 0 ; i < buflen ; i ++) {
          if (buffer[i] == '\"') {
            beginning = buffer + i + 1;
            break;
          }
        }
        
        r = mailimf_mailbox_parse(beginning, strlen(beginning),
            &current_index, &mb);
        if (r == MAILIMF_NO_ERROR) {
          strncpy(username, mb->mb_addr_spec, length);
          username[length - 1] = '\0';
          mailimf_mailbox_free(mb);
          exit_code = 0;
        }
        
        state = STATE_NORMAL;
      }
      break;
    }
  }
  
  fclose(f);
  
 exit:
  return exit_code;
}

#define PGP_DECRYPT_DESCRIPTION "PGP encrypted part\r\n"
#define PGP_DECRYPT_FAILED "PGP decryption FAILED\r\n"
#define PGP_DECRYPT_SUCCESS "PGP decryption success\r\n"

/* extracted from mailprivacy_smime.c -- begin */

static char * get_first_from_addr(struct mailmime * mime)
{
  clistiter * cur;
  struct mailimf_single_fields single_fields;
  struct mailimf_fields * fields;
  struct mailimf_mailbox * mb;
  
  if (mime->mm_type != MAILMIME_MESSAGE)
    return NULL;
  
  fields = mime->mm_data.mm_message.mm_fields;
  if (fields == NULL)
    return NULL;
  
  mailimf_single_fields_init(&single_fields, fields);
  
  if (single_fields.fld_from == NULL)
    return NULL;
  
  cur = clist_begin(single_fields.fld_from->frm_mb_list->mb_list);
  if (cur == NULL)
    return NULL;
  
  mb = clist_content(cur);
  
  return mb->mb_addr_spec;
}

/* extracted from mailprivacy_smime.c -- end */

static int pgp_decrypt(struct mailprivacy * privacy,
    mailmessage * msg,
    struct mailmime * mime, struct mailmime ** result)
{
  struct mailmime * version_mime;
  struct mailmime * encrypted_mime;
  clistiter * cur;
  char encrypted_filename[PATH_MAX];
  char description_filename[PATH_MAX];
  char decrypted_filename[PATH_MAX];
  char command[PATH_MAX];
  struct mailmime * description_mime;
  struct mailmime * decrypted_mime;
  int r;
  int res;
  int decrypt_ok;
  char quoted_encrypted_filename[PATH_MAX];
  struct mailmime * multipart;
  
  /* get the two parts of the PGP message */
  
  cur = clist_begin(mime->mm_data.mm_multipart.mm_mp_list);
  if (cur == NULL) {
    res = MAIL_ERROR_INVAL;
    goto err;
  }
  
  version_mime = clist_content(cur);
  cur = clist_next(cur);
  if (cur == NULL) {
    res = MAIL_ERROR_INVAL;
    goto err;
  }
  
  encrypted_mime = clist_content(cur);
  
  /* fetch the second section, that's the useful one */
  
  r = mailprivacy_fetch_decoded_to_file(privacy,
      encrypted_filename, sizeof(encrypted_filename),
      msg, encrypted_mime);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto err;
  }
  
  /* we are in a safe directory */
  
  r = mailprivacy_get_tmp_filename(privacy,
      decrypted_filename, sizeof(decrypted_filename));
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto unlink_encrypted;
  }
  
  /* description */
  
  r = mailprivacy_get_tmp_filename(privacy, description_filename,
      sizeof(description_filename));
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto unlink_decrypted;
  }
  
  /* run the command */
  
  r = mail_quote_filename(quoted_encrypted_filename,
       sizeof(quoted_encrypted_filename), encrypted_filename);
  if (r < 0) {
    res = MAIL_ERROR_MEMORY;
    goto unlink_description;
  }
  
  snprintf(command, sizeof(command),
      "gpg --passphrase-fd=0 --batch --yes --decrypt '%s'",
      quoted_encrypted_filename);
  
  decrypt_ok = 0;
  r = gpg_command_passphrase(privacy, msg, command, NULL,
      decrypted_filename, description_filename);
  switch (r) {
  case NO_ERROR_PGP:
    decrypt_ok = 1;
    break;
  case ERROR_PGP_NOPASSPHRASE:
  case ERROR_PGP_CHECK:
    decrypt_ok = 0;
    break;
  case ERROR_PGP_COMMAND:
    res = MAIL_ERROR_COMMAND;
    goto unlink_description;
  case ERROR_PGP_FILE:
    res = MAIL_ERROR_FILE;
    goto unlink_description;
  }
  
  if (!decrypt_ok) {
    char encryption_id[4096];
    
    encryption_id[0] = '\0';
    r = get_userid(description_filename, encryption_id, sizeof(encryption_id));
    if (r == 0) {
      mailprivacy_gnupg_add_encryption_id(privacy, msg, encryption_id);
    }
  }
  
  /* building multipart */

  r = mailmime_new_with_content("multipart/x-decrypted", NULL, &multipart);
  if (r != MAILIMF_NO_ERROR) {
    res = MAIL_ERROR_MEMORY;
    goto unlink_description;
  }
  
  /* building the description part */
  
  description_mime = mailprivacy_new_file_part(privacy,
      description_filename,
      "text/plain", MAILMIME_MECHANISM_8BIT);
  if (description_mime == NULL) {
    mailprivacy_mime_clear(multipart);
    mailmime_free(multipart);
    res = MAIL_ERROR_MEMORY;
    goto unlink_description;
  }
  
  /* adds the description part */
  
  r = mailmime_smart_add_part(multipart, description_mime);
  if (r != MAIL_NO_ERROR) {
    mailprivacy_mime_clear(description_mime);
    mailmime_free(description_mime);
    mailprivacy_mime_clear(multipart);
    mailmime_free(multipart);
    res = MAIL_ERROR_MEMORY;
    goto unlink_description;
  }
  
  /* building the decrypted part */
  
  r = mailprivacy_get_part_from_file(privacy, 1, 0,
      decrypted_filename, &decrypted_mime);
  if (r == MAIL_NO_ERROR) {
    /* adds the decrypted part */
    
    r = mailmime_smart_add_part(multipart, decrypted_mime);
    if (r != MAIL_NO_ERROR) {
      mailprivacy_mime_clear(decrypted_mime);
      mailmime_free(decrypted_mime);
      mailprivacy_mime_clear(multipart);
      mailmime_free(multipart);
      res = MAIL_ERROR_MEMORY;
      goto unlink_description;
    }
  }
  
  unlink(description_filename);
  unlink(decrypted_filename);
  unlink(encrypted_filename);
  
  * result = multipart;
  
  return MAIL_NO_ERROR;
  
 unlink_description:
  unlink(description_filename);
 unlink_decrypted:
  unlink(decrypted_filename);
 unlink_encrypted:
  unlink(encrypted_filename);
 err:
  return res;
}

#define PGP_VERIFY_DESCRIPTION "PGP verify signed message\r\n"
#define PGP_VERIFY_FAILED "PGP verification FAILED\r\n"
#define PGP_VERIFY_SUCCESS "PGP verification success\r\n"

static int
pgp_verify(struct mailprivacy * privacy,
    mailmessage * msg,
    struct mailmime * mime, struct mailmime ** result)
{
  struct mailmime * signed_mime;
  struct mailmime * signature_mime;
  char signed_filename[PATH_MAX];
  char signature_filename[PATH_MAX];
  int res;
  int r;
  clistiter * cur;
  char command[PATH_MAX];
  int sign_ok;
  struct mailmime * description_mime;
  char decrypted_filename[PATH_MAX];
  char description_filename[PATH_MAX];
  char quoted_signed_filename[PATH_MAX];
  char quoted_signature_filename[PATH_MAX];
  struct mailmime * multipart;
  struct mailmime * signed_msg_mime;
  
  /* get the two parts of the PGP message */
  
  cur = clist_begin(mime->mm_data.mm_multipart.mm_mp_list);
  if (cur == NULL) {
    res = MAIL_ERROR_INVAL;
    goto err;
  }
  
  signed_mime = clist_content(cur);
  cur = clist_next(cur);
  if (cur == NULL) {
    res = MAIL_ERROR_INVAL;
    goto err;
  }
  
  signature_mime = clist_content(cur);
  
  /* fetch signed part and write it to a file */
  
  r = mailprivacy_fetch_mime_body_to_file(privacy,
      signed_filename, sizeof(signed_filename),
      msg, signed_mime);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto err;
  }
  
  /* fetch signed part and write it to a file */

  r = mailprivacy_fetch_decoded_to_file(privacy,
      signature_filename, sizeof(signature_filename),
      msg, signature_mime);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto unlink_signed;
  }
  
  /* description */
  
  r = mailprivacy_get_tmp_filename(privacy, description_filename,
      sizeof(description_filename));
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto unlink_signature;
  }
  
  /* decrypted (dummy) */
  
  r = mailprivacy_get_tmp_filename(privacy, decrypted_filename,
      sizeof(decrypted_filename));
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto unlink_description;
  }
  
  /* run the command */
  
  r = mail_quote_filename(quoted_signature_filename,
      sizeof(quoted_signature_filename), signature_filename);
  if (r < 0) {
    res = MAIL_ERROR_MEMORY;
    goto unlink_decrypted;
  }
  
  r = mail_quote_filename(quoted_signed_filename,
      sizeof(quoted_signed_filename), signed_filename);
  if (r < 0) {
    res = MAIL_ERROR_MEMORY;
    goto unlink_decrypted;
  }
  
  snprintf(command, sizeof(command), "gpg --batch --yes --verify '%s' '%s'",
      quoted_signature_filename, quoted_signed_filename);

  sign_ok = 0;
  r = gpg_command_passphrase(privacy, msg, command, NULL,
      decrypted_filename, description_filename);
  switch (r) {
  case NO_ERROR_PGP:
    sign_ok = 1;
    break;
  case ERROR_PGP_NOPASSPHRASE:
  case ERROR_PGP_CHECK:
    sign_ok = 0;
    break;
  case ERROR_PGP_COMMAND:
    res = MAIL_ERROR_COMMAND;
    goto unlink_decrypted;
  case ERROR_PGP_FILE:
    res = MAIL_ERROR_FILE;
    goto unlink_decrypted;
  }
  
  /* building multipart */

  r = mailmime_new_with_content("multipart/x-verified", NULL, &multipart);
  if (r != MAILIMF_NO_ERROR) {
    res = MAIL_ERROR_MEMORY;
    goto unlink_decrypted;
  }

  /* building the description part */
  
  description_mime = mailprivacy_new_file_part(privacy,
      description_filename,
      "text/plain", MAILMIME_MECHANISM_8BIT);
  if (description_mime == NULL) {
    mailprivacy_mime_clear(multipart);
    mailmime_free(multipart);
    res = MAIL_ERROR_MEMORY;
    goto unlink_decrypted;
  }

  /* adds the description part */
  
  r = mailmime_smart_add_part(multipart, description_mime);
  if (r != MAIL_NO_ERROR) {
    mailprivacy_mime_clear(description_mime);
    mailmime_free(description_mime);
    mailprivacy_mime_clear(multipart);
    mailmime_free(multipart);
    res = MAIL_ERROR_MEMORY;
    goto unlink_decrypted;
  }

  r = mailprivacy_get_part_from_file(privacy, 1, 0,
      signed_filename, &signed_msg_mime);
  if (r != MAIL_NO_ERROR) {
    mailprivacy_mime_clear(multipart);
    mailmime_free(multipart);
    res = MAIL_ERROR_MEMORY;
    goto unlink_decrypted;
  }
  
  r = mailmime_smart_add_part(multipart, signed_msg_mime);
  if (r != MAIL_NO_ERROR) {
    mailprivacy_mime_clear(signed_msg_mime);
    mailmime_free(signed_msg_mime);
    mailprivacy_mime_clear(multipart);
    mailmime_free(multipart);
    res = MAIL_ERROR_MEMORY;
    goto unlink_decrypted;
  }
  
  unlink(decrypted_filename);
  unlink(description_filename);
  unlink(signature_filename);
  unlink(signed_filename);

  * result = multipart;
  
  return MAIL_NO_ERROR;
  
 unlink_decrypted:
  unlink(decrypted_filename);
 unlink_description:
  unlink(description_filename);
 unlink_signature:
  unlink(signature_filename);
 unlink_signed:
  unlink(signed_filename);
 err:
  return res;
}


#define PGP_CLEAR_VERIFY_DESCRIPTION "PGP verify clear signed message\r\n"
#define PGP_CLEAR_VERIFY_FAILED "PGP verification FAILED\r\n"
#define PGP_CLEAR_VERIFY_SUCCESS "PGP verification success\r\n"

static int pgp_verify_clearsigned(struct mailprivacy * privacy,
    mailmessage * msg,
    struct mailmime * mime,
    char * content, size_t content_len, struct mailmime ** result)
{
  int r;
  char command[PATH_MAX];
  int res;
  int sign_ok;
  size_t written;
  char signed_filename[PATH_MAX];
  FILE * signed_f;
  char stripped_filename[PATH_MAX];
  char description_filename[PATH_MAX];
  char quoted_signed_filename[PATH_MAX];
  struct mailmime * stripped_mime;
  struct mailmime * description_mime;
  struct mailmime * multipart;
  struct mailmime_content * content_type;
  
  if (mime->mm_parent == NULL) {
    res = MAIL_ERROR_INVAL;
    goto err;
  }

  if (mime->mm_parent->mm_type == MAILMIME_SINGLE) {
    res = MAIL_ERROR_INVAL;
    goto err;
  }
  
  signed_f = mailprivacy_get_tmp_file(privacy,
      signed_filename, sizeof(signed_filename));
  if (signed_f == NULL) {
    res = MAIL_ERROR_FILE;
    goto err;
  }
  
  written = fwrite(content, 1, content_len, signed_f);
  if (written != content_len) {
    fclose(signed_f);
    unlink(signed_filename);
    res = MAIL_ERROR_FILE;
    goto err;
  }
  fclose(signed_f);

  /* XXX - prepare file for PGP, remove trailing WS */
  
  r = mailprivacy_get_tmp_filename(privacy, stripped_filename,
      sizeof(stripped_filename));
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto unlink_signed;
  }
  
  /* description */
  
  r = mailprivacy_get_tmp_filename(privacy, description_filename,
      sizeof(description_filename));
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto unlink_stripped;
  }
  
  r = mail_quote_filename(quoted_signed_filename,
      sizeof(quoted_signed_filename), signed_filename);
  if (r < 0) {
    res = MAIL_ERROR_MEMORY;
    goto unlink_description;
  }
  
  snprintf(command, sizeof(command),
      "gpg --batch --yes --decrypt '%s'", quoted_signed_filename);
  
  sign_ok = 0;
  r = gpg_command_passphrase(privacy, msg, command, NULL,
      stripped_filename, description_filename);
  switch (r) {
  case NO_ERROR_PGP:
    sign_ok = 1;
    break;
  case ERROR_PGP_NOPASSPHRASE:
  case ERROR_PGP_CHECK:
    sign_ok = 0;
    break;
  case ERROR_PGP_COMMAND:
    res = MAIL_ERROR_COMMAND;
    goto unlink_description;
  case ERROR_PGP_FILE:
    res = MAIL_ERROR_FILE;
    goto unlink_description;
  }
  
  /* building multipart */

  r = mailmime_new_with_content("multipart/x-verified", NULL, &multipart);
  if (r != MAILIMF_NO_ERROR) {
    res = MAIL_ERROR_MEMORY;
    goto unlink_description;
  }
  
  /* building the description part */
  
  description_mime = mailprivacy_new_file_part(privacy,
      description_filename,
      "text/plain", MAILMIME_MECHANISM_8BIT);
  if (description_mime == NULL) {
    mailprivacy_mime_clear(multipart);
    mailmime_free(multipart);
    res = MAIL_ERROR_MEMORY;
    goto unlink_description;
  }
  
  /* adds the description part */
  
  r = mailmime_smart_add_part(multipart, description_mime);
  if (r != MAIL_NO_ERROR) {
    mailprivacy_mime_clear(description_mime);
    mailmime_free(description_mime);
    mailprivacy_mime_clear(multipart);
    mailmime_free(multipart);
    res = MAIL_ERROR_MEMORY;
    goto unlink_description;
  }
  
  /* building the signature stripped part */
  
  stripped_mime = mailprivacy_new_file_part(privacy,
      stripped_filename,
      "application/octet-stream",
      MAILMIME_MECHANISM_8BIT);
  if (stripped_mime == NULL) {
    mailprivacy_mime_clear(multipart);
    mailmime_free(multipart);
    res = MAIL_ERROR_MEMORY;
    goto unlink_description;
  }
  
  /* place original content type */
  
  content_type = mailmime_content_dup(mime->mm_content_type);
  if (content_type == NULL) {
    mailprivacy_mime_clear(stripped_mime);
    mailmime_free(stripped_mime);
    mailprivacy_mime_clear(multipart);
    mailmime_free(multipart);
    res = MAIL_ERROR_MEMORY;
    goto unlink_description;
  }
  
  mailmime_content_free(stripped_mime->mm_content_type);
  stripped_mime->mm_content_type = content_type;

  /* place original MIME fields */
  
  if (mime->mm_mime_fields != NULL) {
    struct mailmime_fields * mime_fields;
    clistiter * cur;

    mime_fields = mailprivacy_mime_fields_dup(privacy, mime->mm_mime_fields);
    if (mime_fields == NULL) {
      mailprivacy_mime_clear(stripped_mime);
      mailmime_free(stripped_mime);
      mailprivacy_mime_clear(multipart);
      mailmime_free(multipart);
      res = MAIL_ERROR_MEMORY;
      goto unlink_description;
    }
    for(cur = clist_begin(mime_fields->fld_list) ;
        cur != NULL ; cur = clist_next(cur)) {
      struct mailmime_field * field;
      
      field = clist_content(cur);
      if (field->fld_type == MAILMIME_FIELD_TRANSFER_ENCODING) {
        mailmime_field_free(field);
        clist_delete(mime_fields->fld_list, cur);
        break;
      }
    }
    clist_concat(stripped_mime->mm_mime_fields->fld_list,
        mime_fields->fld_list);
    mailmime_fields_free(mime_fields);
  }
  
  /* adds the stripped part */
  
  r = mailmime_smart_add_part(multipart, stripped_mime);
  if (r != MAIL_NO_ERROR) {
    mailprivacy_mime_clear(stripped_mime);
    mailmime_free(stripped_mime);
    mailprivacy_mime_clear(multipart);
    mailmime_free(multipart);
    res = MAIL_ERROR_MEMORY;
    goto unlink_description;
  }
  
  unlink(description_filename);
  unlink(stripped_filename);
  unlink(signed_filename);
  
  * result = multipart;
  
  return MAIL_NO_ERROR;
  
 unlink_description:
  unlink(description_filename);
 unlink_stripped:
  unlink(stripped_filename);
 unlink_signed:
  unlink(signed_filename);
 err:
  return res;
}


#define PGP_DECRYPT_ARMOR_DESCRIPTION "PGP ASCII armor encrypted part\r\n"
#define PGP_DECRYPT_ARMOR_FAILED "PGP ASCII armor decryption FAILED\r\n"
#define PGP_DECRYPT_ARMOR_SUCCESS "PGP ASCII armor decryption success\r\n"

static int pgp_decrypt_armor(struct mailprivacy * privacy,
    mailmessage * msg,
    struct mailmime * mime,
    char * content, size_t content_len, struct mailmime ** result)
{
  FILE * encrypted_f;
  char encrypted_filename[PATH_MAX];
  char description_filename[PATH_MAX];
  char decrypted_filename[PATH_MAX];
  size_t written;
  char command[PATH_MAX];
  struct mailmime * description_mime;
  struct mailmime * decrypted_mime;
  struct mailmime * multipart;
  int r;
  int res;
  int sign_ok;
  char quoted_encrypted_filename[PATH_MAX];

  if (mime->mm_parent == NULL) {
    res = MAIL_ERROR_INVAL;
    goto err;
  }

  if (mime->mm_parent->mm_type == MAILMIME_SINGLE) {
    res = MAIL_ERROR_INVAL;
    goto err;
  }
  
  encrypted_f = mailprivacy_get_tmp_file(privacy,
      encrypted_filename,
      sizeof(encrypted_filename));
  if (encrypted_f == NULL) {
    res = MAIL_ERROR_FILE;
    goto err;
  }
  
  written = fwrite(content, 1, content_len, encrypted_f);
  if (written != content_len) {
    fclose(encrypted_f);
    unlink(encrypted_filename);
    res = MAIL_ERROR_FILE;
    goto err;
  }
  
  fclose(encrypted_f);
  
  /* we are in a safe directory */
  
  r = mailprivacy_get_tmp_filename(privacy, decrypted_filename,
      sizeof(decrypted_filename));
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto unlink_encrypted;
  }

  /* description */
  
  r = mailprivacy_get_tmp_filename(privacy, description_filename,
      sizeof(description_filename));
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto unlink_decrypted;
  }
  
  /* run the command */
  
  r = mail_quote_filename(quoted_encrypted_filename,
       sizeof(quoted_encrypted_filename), encrypted_filename);
  if (r < 0) {
    res = MAIL_ERROR_MEMORY;
    goto unlink_description;
  }
  
  snprintf(command, sizeof(command),
      "gpg --passphrase-fd=0 --batch --yes --decrypt '%s'",
      quoted_encrypted_filename);
  
  sign_ok = 0;
  r = gpg_command_passphrase(privacy, msg, command, NULL,
      decrypted_filename, description_filename);
  switch (r) {
  case NO_ERROR_PGP:
    sign_ok = 1;
    break;
  case ERROR_PGP_NOPASSPHRASE:
  case ERROR_PGP_CHECK:
    sign_ok = 0;
    break;
  case ERROR_PGP_COMMAND:
    res = MAIL_ERROR_COMMAND;
    goto unlink_description;
  case ERROR_PGP_FILE:
    res = MAIL_ERROR_FILE;
    goto unlink_description;
  }
  
  /* building multipart */
  
  r = mailmime_new_with_content("multipart/x-decrypted", NULL, &multipart);
  if (r != MAILIMF_NO_ERROR) {
    res = MAIL_ERROR_MEMORY;
    goto unlink_description;
  }
  
  /* building the description part */
  
  description_mime = mailprivacy_new_file_part(privacy,
      description_filename,
      "text/plain", MAILMIME_MECHANISM_8BIT);
  if (description_mime == NULL) {
    mailprivacy_mime_clear(multipart);
    mailmime_free(multipart);
    res = MAIL_ERROR_MEMORY;
    goto unlink_description;
  }
  
  /* adds the description part */
  
  r = mailmime_smart_add_part(multipart, description_mime);
  if (r != MAIL_NO_ERROR) {
    mailprivacy_mime_clear(description_mime);
    mailmime_free(description_mime);
    mailprivacy_mime_clear(multipart);
    mailmime_free(multipart);
    res = MAIL_ERROR_MEMORY;
    goto unlink_description;
  }
  
  /* building the decrypted part */
  
  r = mailprivacy_get_part_from_file(privacy, 1, 0,
      decrypted_filename, &decrypted_mime);
  if (r != MAIL_NO_ERROR) {
    mailprivacy_mime_clear(multipart);
    mailmime_free(multipart);
    res = r;
    goto unlink_description;
  }
  
  /* adds the decrypted part */
  
  r = mailmime_smart_add_part(multipart, decrypted_mime);
  if (r != MAIL_NO_ERROR) {
    mailprivacy_mime_clear(decrypted_mime);
    mailmime_free(decrypted_mime);
    mailprivacy_mime_clear(multipart);
    mailmime_free(multipart);
    res = MAIL_ERROR_MEMORY;
    goto unlink_description;
  }
  
  unlink(description_filename);
  unlink(decrypted_filename);
  unlink(encrypted_filename);
  
  * result = multipart;
  
  return MAIL_NO_ERROR;
  
 unlink_description:
  unlink(description_filename);
 unlink_decrypted:
  unlink(decrypted_filename);
 unlink_encrypted:
  unlink(encrypted_filename);
 err:
  return res;
}


static int mime_is_text(struct mailmime * build_info)
{
  if (build_info->mm_type == MAILMIME_SINGLE) {
    if (build_info->mm_content_type != NULL) {
      if (build_info->mm_content_type->ct_type->tp_type ==
          MAILMIME_TYPE_DISCRETE_TYPE) {
        if (build_info->mm_content_type->ct_type->tp_data.tp_discrete_type->dt_type ==
            MAILMIME_DISCRETE_TYPE_TEXT)
          return 1;
      }
    }
    else
      return 1;
  }

  return 0;
}


static int pgp_test_encrypted(struct mailprivacy * privacy,
    mailmessage * msg, struct mailmime * mime)
{
  int r;
  int res;

  switch (mime->mm_type) {
  case MAILMIME_MULTIPLE:
    return (pgp_is_encrypted(mime) || pgp_is_signed(mime));
    
  case MAILMIME_SINGLE:
    /* clear sign or ASCII armor encryption */
    if (mime_is_text(mime)) {
      char * content;
      size_t content_len;
      char * parsed_content;
      size_t parsed_content_len;
      size_t cur_token;
      int encoding;
      struct mailmime_single_fields single_fields;
      
      r = mailprivacy_msg_fetch_section(privacy, msg, mime,
          &content, &content_len);
      if (r != MAIL_NO_ERROR)
        return 0;
      
      mailmime_single_fields_init(&single_fields, mime->mm_mime_fields,
          mime->mm_content_type);
      if (single_fields.fld_encoding != NULL)
        encoding = single_fields.fld_encoding->enc_type;
      else
        encoding = MAILMIME_MECHANISM_8BIT;
      
      cur_token = 0;
      r = mailmime_part_parse(content, content_len, &cur_token,
          encoding, &parsed_content, &parsed_content_len);
      mailprivacy_msg_fetch_result_free(privacy, msg, content);
      
      if (r != MAILIMF_NO_ERROR)
        return 0;
      
      res = 0;
      
      if (pgp_is_clearsigned(parsed_content, parsed_content_len))
        res = 1;
      else if (pgp_is_crypted_armor(parsed_content, parsed_content_len))
        res = 1;
      
      mmap_string_unref(parsed_content);
      
      return res;
    }
    break;
  }
  
  return 0;
}

static int pgp_handler(struct mailprivacy * privacy,
    mailmessage * msg,
    struct mailmime * mime, struct mailmime ** result)
{
  int r;
  struct mailmime * alternative_mime;
  
  alternative_mime = NULL;
  switch (mime->mm_type) {
  case MAILMIME_MULTIPLE:
    r = MAIL_ERROR_INVAL;
    if (pgp_is_encrypted(mime)) {
      r = pgp_decrypt(privacy, msg, mime, &alternative_mime);
    }
    else if (pgp_is_signed(mime)) {
      r = pgp_verify(privacy, msg, mime, &alternative_mime);
    }
    
    if (r != MAIL_NO_ERROR)
      return r;

    * result = alternative_mime;
    
    return MAIL_NO_ERROR;
    
  case MAILMIME_SINGLE:
    /* clear sign or ASCII armor encryption */
    if (mime_is_text(mime)) {
      char * content;
      size_t content_len;
      char * parsed_content;
      size_t parsed_content_len;
      size_t cur_token;
      int encoding;
      struct mailmime_single_fields single_fields;
      
      r = mailprivacy_msg_fetch_section(privacy, msg, mime,
          &content, &content_len);
      if (r != MAIL_NO_ERROR)
        return MAIL_ERROR_FETCH;
      
      mailmime_single_fields_init(&single_fields, mime->mm_mime_fields,
          mime->mm_content_type);
      if (single_fields.fld_encoding != NULL)
        encoding = single_fields.fld_encoding->enc_type;
      else
        encoding = MAILMIME_MECHANISM_8BIT;
      
      cur_token = 0;
      r = mailmime_part_parse(content, content_len, &cur_token,
          encoding, &parsed_content, &parsed_content_len);
      mailprivacy_msg_fetch_result_free(privacy, msg, content);
      
      if (r != MAILIMF_NO_ERROR)
        return MAIL_ERROR_PARSE;
      
      r = MAIL_ERROR_INVAL;
      if (pgp_is_clearsigned(parsed_content,
              parsed_content_len)) {
        r = pgp_verify_clearsigned(privacy,
            msg, mime, parsed_content, parsed_content_len, &alternative_mime);
      }
      else if (pgp_is_crypted_armor(parsed_content,
                   parsed_content_len)) {
        r = pgp_decrypt_armor(privacy,
            msg, mime, parsed_content, parsed_content_len, &alternative_mime);
      }
      
      mmap_string_unref(parsed_content);
      
      if (r != MAIL_NO_ERROR)
        return r;

      * result = alternative_mime;
      
      return MAIL_NO_ERROR;
    }
    break;
  }
  
  return MAIL_ERROR_INVAL;
}


#if 0
static void prepare_mime_single(struct mailmime * mime)
{
  struct mailmime_single_fields single_fields;
  int encoding;
  int r;
  
  if (mime->mime_fields != NULL) {
    mailmime_single_fields_init(&single_fields, mime->mime_fields,
        mime->content_type);
    if (single_fields.encoding != NULL) {
      encoding = single_fields.encoding->type;
      switch (encoding) {
      case MAILMIME_MECHANISM_8BIT:
      case MAILMIME_MECHANISM_7BIT:
      case MAILMIME_MECHANISM_BINARY:
        single_fields.encoding->type = MAILMIME_MECHANISM_QUOTED_PRINTABLE;
        break;
      }
    }
    else {
      struct mailmime_mechanism * mechanism;
      struct mailmime_field * field;
      
      mechanism =
        mailmime_mechanism_new(MAILMIME_MECHANISM_QUOTED_PRINTABLE, NULL);
      if (mechanism == NULL)
        return;
      
      field = mailmime_field_new(MAILMIME_FIELD_TRANSFER_ENCODING,
          NULL, mechanism, NULL, NULL, 0, NULL, NULL);
      if (field == NULL) {
        mailmime_mechanism_free(mechanism);
        return;
      }
      
      r = clist_append(mime->mime_fields->list, field);
      if (r < 0) {
        mailmime_field_free(field);
        return;
      }
    }
  }
      
  switch (mime->body->encoding) {
  case MAILMIME_MECHANISM_8BIT:
  case MAILMIME_MECHANISM_7BIT:
  case MAILMIME_MECHANISM_BINARY:
    mime->body->encoding = MAILMIME_MECHANISM_QUOTED_PRINTABLE;
    mime->body->encoded = 0;
    break;
  }
}

/*
  prepare_mime()
  
  we assume we built ourself the message.
*/

static void prepare_mime(struct mailmime * mime)
{
  clistiter * cur;
  
  switch (mime->type) {
  case MAILMIME_SINGLE:
    if (mime->body != NULL) {
      prepare_mime_single(mime);
    }
    break;
    
  case MAILMIME_MULTIPLE:
    for(cur = clist_begin(mime->list) ; cur != NULL ; cur = clist_next(cur)) {
      struct mailmime * child;
      
      child = cur->data;
      
      prepare_mime(child);
    }
    break;
    
  case MAILMIME_MESSAGE:
    if (mime->msg_mime) {
      prepare_mime(mime->msg_mime);
    }
    break;
  }
}
#endif

static int pgp_sign_mime(struct mailprivacy * privacy,
    mailmessage * msg,
    struct mailmime * mime, struct mailmime ** result)
{
  char to_sign_filename[PATH_MAX];
  char quoted_to_sign_filename[PATH_MAX];
  FILE * to_sign_f;
  int res;
  int r;
  int col;
  char description_filename[PATH_MAX];
  char signature_filename[PATH_MAX];
  char command[PATH_MAX];
  char default_key[PATH_MAX];
  struct mailmime * signature_mime;
  struct mailmime * multipart;
  struct mailmime_content * content;
  struct mailmime_parameter * param;
  struct mailmime * to_sign_msg_mime;
  char * dup_signature_filename;
  char * email;
  int sign_ok;
  
  /* get signing key */
  
  * default_key = '\0';
  email = get_first_from_addr(mime);
  if (email != NULL)
    snprintf(default_key, sizeof(default_key),
        "--default-key %s", email);
  
  /* part to sign */

  /* encode quoted printable all text parts */
  
  mailprivacy_prepare_mime(mime);
  
  to_sign_f = mailprivacy_get_tmp_file(privacy,
      to_sign_filename, sizeof(to_sign_filename));
  if (to_sign_f == NULL) {
    res = MAIL_ERROR_FILE;
    goto err;
  }
  
  col = 0;
  r = mailmime_write(to_sign_f, &col, mime);
  if (r != MAILIMF_NO_ERROR) {
    fclose(to_sign_f);
    res = MAIL_ERROR_FILE;
    goto unlink_to_sign;
  }
  
  fclose(to_sign_f);
  
  /* prepare destination file for signature */
  
  r = mailprivacy_get_tmp_filename(privacy, signature_filename,
      sizeof(signature_filename));
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto unlink_to_sign;
  }

  r = mailprivacy_get_tmp_filename(privacy, description_filename,
      sizeof(description_filename));
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto unlink_signature;
  }
  
  r = mail_quote_filename(quoted_to_sign_filename,
       sizeof(quoted_to_sign_filename), to_sign_filename);
  if (r < 0) {
    res = MAIL_ERROR_MEMORY;
    goto unlink_description;
  }
  
  snprintf(command, sizeof(command),
      "gpg --passphrase-fd=0 -a --batch --yes --digest-algo sha1 %s -b '%s'",
      default_key, quoted_to_sign_filename);
  
  sign_ok = 0;
  r = gpg_command_passphrase(privacy, msg, command, NULL,
      signature_filename, description_filename);
  switch (r) {
  case NO_ERROR_PGP:
    sign_ok = 1;
    break;
  case ERROR_PGP_NOPASSPHRASE:
  case ERROR_PGP_CHECK:
    sign_ok = 0;
    break;
  case ERROR_PGP_COMMAND:
    res = MAIL_ERROR_COMMAND;
    goto unlink_description;
  case ERROR_PGP_FILE:
    res = MAIL_ERROR_FILE;
    goto unlink_description;
  }
  
  if (!sign_ok) {
    res = MAIL_ERROR_COMMAND;
    goto unlink_description;
  }
  
  /* multipart */
  
  multipart = mailprivacy_new_file_part(privacy, NULL,
      "multipart/signed", -1);
  
  content = multipart->mm_content_type;
  
  param = mailmime_param_new_with_data("micalg", "pgp-sha1");
  if (param == NULL) {
    mailmime_free(multipart);
    res = MAIL_ERROR_MEMORY;
    goto unlink_description;
  }
  
  r = clist_append(content->ct_parameters, param);
  if (r < 0) {
    mailmime_parameter_free(param);
    mailmime_free(multipart);
    res = MAIL_ERROR_MEMORY;
    goto unlink_description;
  }
  
  param = mailmime_param_new_with_data("protocol",
      "application/pgp-signature");
  if (param == NULL) {
    mailmime_free(multipart);
    res = MAIL_ERROR_MEMORY;
    goto unlink_description;
  }

  r = clist_append(content->ct_parameters, param);
  if (r < 0) {
    mailmime_parameter_free(param);
    mailmime_free(multipart);
    res = MAIL_ERROR_MEMORY;
    goto unlink_description;
  }
  
  /* signed part */

  r = mailprivacy_get_part_from_file(privacy, 1, 0,
      to_sign_filename, &to_sign_msg_mime);
  if (r != MAIL_NO_ERROR) {
    mailprivacy_mime_clear(multipart);
    mailmime_free(multipart);
    res = r;
    goto unlink_description;
  }
  
  mailprivacy_prepare_mime(to_sign_msg_mime);
  
  r = mailmime_smart_add_part(multipart, to_sign_msg_mime);
  if (r != MAIL_NO_ERROR) {
    mailprivacy_mime_clear(to_sign_msg_mime);
    mailmime_free(to_sign_msg_mime);
    mailprivacy_mime_clear(multipart);
    mailmime_free(multipart);
    res = MAIL_ERROR_MEMORY;
    goto unlink_description;
  }

  /* signature part */
  
  /* reencode the signature file with CRLF */
  dup_signature_filename = mailprivacy_dup_imf_file(privacy,
      signature_filename);
  if (dup_signature_filename == NULL) {
    mailprivacy_mime_clear(multipart);
    mailmime_free(multipart);
    res = MAIL_ERROR_FILE;
    goto unlink_description;
  }
  
  /* replace the signature file */
  unlink(signature_filename);
  strncpy(signature_filename,
      dup_signature_filename, sizeof(signature_filename));
  signature_filename[sizeof(signature_filename) - 1] = '\0';
  
  signature_mime = mailprivacy_new_file_part(privacy,
      signature_filename,
      "application/pgp-signature",
      MAILMIME_MECHANISM_8BIT);
  if (signature_mime == NULL) {
    mailprivacy_mime_clear(multipart);
    mailmime_free(multipart);
    res = MAIL_ERROR_MEMORY;
    goto unlink_description;
  }
  
  r = mailmime_smart_add_part(multipart, signature_mime);
  if (r != MAIL_NO_ERROR) {
    mailprivacy_mime_clear(signature_mime);
    mailmime_free(signature_mime);
    mailprivacy_mime_clear(multipart);
    mailmime_free(multipart);
    res = MAIL_ERROR_MEMORY;
    goto unlink_description;
  }

  unlink(description_filename);
  unlink(signature_filename);
  unlink(to_sign_filename);
  
  * result = multipart;
  
  return MAIL_NO_ERROR;
  
 unlink_description:
  unlink(description_filename);
 unlink_signature:
  unlink(signature_filename);
 unlink_to_sign:
  unlink(to_sign_filename);
 err:
  return res;
}


/* ********************************************************************* */
/* find GPG recipient */

static int recipient_add_mb(char * recipient, size_t * len,
    struct mailimf_mailbox * mb)
{
  char buffer[PATH_MAX];
  size_t buflen;

  if (mb->mb_addr_spec != NULL) {
    snprintf(buffer, sizeof(buffer), "-r %s ", mb->mb_addr_spec);
    buflen = strlen(buffer);
    if (buflen >= * len)
      return MAIL_ERROR_MEMORY;

    strncat(recipient, buffer, * len);
    (* len) -= buflen;
  }

  return MAIL_NO_ERROR;
}

static int recipient_add_mb_list(char * recipient, size_t * len,
    struct mailimf_mailbox_list * mb_list)
{
  clistiter * cur;
  int r;

  for(cur = clist_begin(mb_list->mb_list) ; cur != NULL ;
      cur = clist_next(cur)) {
    struct mailimf_mailbox * mb;

    mb = clist_content(cur);

    r = recipient_add_mb(recipient, len, mb);
    if (r != MAIL_NO_ERROR)
      return r;
  }

  return MAIL_NO_ERROR;
}

static int recipient_add_group(char * recipient, size_t * len,
    struct mailimf_group * group)
{
  return recipient_add_mb_list(recipient, len, group->grp_mb_list);
}

static int recipient_add_addr(char * recipient, size_t * len,
    struct mailimf_address * addr)
{
  int r;

  switch (addr->ad_type) {
  case MAILIMF_ADDRESS_MAILBOX:
    r = recipient_add_mb(recipient, len, addr->ad_data.ad_mailbox);
    break;
  case MAILIMF_ADDRESS_GROUP:
    r = recipient_add_group(recipient, len, addr->ad_data.ad_group);
    break;
  default:
    r = MAIL_ERROR_INVAL;
  }
  
  return r;
}

static int recipient_add_addr_list(char * recipient, size_t * len,
    struct mailimf_address_list * addr_list)
{
  clistiter * cur;
  int r;

  for(cur = clist_begin(addr_list->ad_list) ; cur != NULL ;
      cur = clist_next(cur)) {
    struct mailimf_address * addr;

    addr = clist_content(cur);

    r = recipient_add_addr(recipient, len, addr);
    if (r != MAIL_NO_ERROR)
      return r;
  }

  return MAIL_NO_ERROR;
}


static int collect_recipient(char * recipient, size_t size,
    struct mailimf_fields * fields)
{
  struct mailimf_single_fields single_fields;
  int r;
  size_t remaining;
  int res;

  * recipient = '\0';
  remaining = size;
  
  if (fields != NULL)
    mailimf_single_fields_init(&single_fields, fields);
  
  if (single_fields.fld_to != NULL) {
    r = recipient_add_addr_list(recipient, &remaining,
        single_fields.fld_to->to_addr_list);
    if (r != MAIL_NO_ERROR) {
      res = r;
      goto err;
    }
  }

  if (single_fields.fld_cc != NULL) {
    r = recipient_add_addr_list(recipient, &remaining,
        single_fields.fld_cc->cc_addr_list);
    if (r != MAIL_NO_ERROR) {
      res = r;
      goto err;
    }
  }

  if (single_fields.fld_bcc != NULL) {
    if (single_fields.fld_bcc->bcc_addr_list != NULL) {
      r = recipient_add_addr_list(recipient, &remaining,
          single_fields.fld_bcc->bcc_addr_list);
      if (r != MAIL_NO_ERROR) {
        res = r;
        goto err;
      }
    }
  }
  
  return MAIL_NO_ERROR;
  
 err:
  return res;
}


#define PGP_VERSION "Version: 1\r\n"

static int pgp_sign_encrypt_mime(struct mailprivacy * privacy,
    mailmessage * msg,
    struct mailmime * mime, struct mailmime ** result)
{
  char original_filename[PATH_MAX];
  FILE * original_f;
  int res;
  int r;
  int col;
  char encrypted_filename[PATH_MAX];
  char description_filename[PATH_MAX];
  char version_filename[PATH_MAX];
  FILE * version_f;
  char command[PATH_MAX];
  char quoted_original_filename[PATH_MAX];
  struct mailmime * version_mime;
  struct mailmime * multipart;
  struct mailmime_content * content;
  struct mailmime_parameter * param;
  struct mailmime * encrypted_mime;
  char recipient[PATH_MAX];
  struct mailimf_fields * fields;
  struct mailmime * root;
  size_t written;
  char * email;
  int encrypt_ok;
  char default_key[PATH_MAX];
  
  /* get signing key */
  
  * default_key = '\0';
  email = get_first_from_addr(mime);
  if (email != NULL)
    snprintf(default_key, sizeof(default_key),
        "--default-key %s", email);
  
  root = mime;
  while (root->mm_parent != NULL)
    root = root->mm_parent;
  
  fields = NULL;
  if (root->mm_type == MAILMIME_MESSAGE)
    fields = root->mm_data.mm_message.mm_fields;
  
  /* recipient */
  
  collect_recipient(recipient, sizeof(recipient), fields);
  
  /* part to encrypt */
  
  /* encode quoted printable all text parts */
  
  mailprivacy_prepare_mime(mime);
  
  original_f = mailprivacy_get_tmp_file(privacy, original_filename,
      sizeof(original_filename));
  if (original_f == NULL) {
    res = MAIL_ERROR_FILE;
    goto err;
  }
  
  col = 0;
  r = mailmime_write(original_f, &col, mime);
  if (r != MAILIMF_NO_ERROR) {
    fclose(original_f);
    res = MAIL_ERROR_FILE;
    goto unlink_original;
  }
  
  fclose(original_f);
  
  /* prepare destination file for encryption */
  
  r = mailprivacy_get_tmp_filename(privacy, encrypted_filename,
      sizeof(encrypted_filename));
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto unlink_original;
  }
  
  r = mail_quote_filename(quoted_original_filename,
       sizeof(quoted_original_filename), original_filename);
  if (r < 0) {
    res = MAIL_ERROR_MEMORY;
    goto unlink_encrypted;
  }
  
  r = mailprivacy_get_tmp_filename(privacy, description_filename,
      sizeof(description_filename));
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto unlink_encrypted;
  }
  
  snprintf(command, sizeof(command),
      "gpg --passphrase-fd=0 %s -a --batch --yes --digest-algo sha1 -s %s -e '%s'",
      recipient, default_key, quoted_original_filename);
  
  encrypt_ok = 0;
  r = gpg_command_passphrase(privacy, msg, command, NULL,
      encrypted_filename, description_filename);
  switch (r) {
  case NO_ERROR_PGP:
    encrypt_ok = 1;
    break;
  case ERROR_PGP_NOPASSPHRASE:
  case ERROR_PGP_CHECK:
    encrypt_ok = 0;
    break;
  case ERROR_PGP_COMMAND:
    res = MAIL_ERROR_COMMAND;
    goto unlink_description;
  case ERROR_PGP_FILE:
    res = MAIL_ERROR_FILE;
    goto unlink_description;
  }
  
  if (!encrypt_ok) {
    res = MAIL_ERROR_COMMAND;
    goto unlink_description;
  }
  
  /* multipart */
  
  multipart = mailprivacy_new_file_part(privacy, NULL,
      "multipart/encrypted", -1);
  
  content = multipart->mm_content_type;
  
  param = mailmime_param_new_with_data("protocol",
      "application/pgp-encrypted");
  if (param == NULL) {
    mailmime_free(multipart);
    res = MAIL_ERROR_MEMORY;
    goto unlink_description;
  }
  
  r = clist_append(content->ct_parameters, param);
  if (r < 0) {
    mailmime_parameter_free(param);
    mailmime_free(multipart);
    res = MAIL_ERROR_MEMORY;
    goto unlink_description;
  }

  /* version part */
  
  version_f = mailprivacy_get_tmp_file(privacy,
      version_filename,
      sizeof(version_filename));
  if (version_f == NULL) {
    mailprivacy_mime_clear(multipart);
    mailmime_free(multipart);
    res = MAIL_ERROR_FILE;
    goto unlink_description;
  }
  written = fwrite(PGP_VERSION, 1, sizeof(PGP_VERSION) - 1, version_f);
  if (written != sizeof(PGP_VERSION) - 1) {
    fclose(version_f);
    mailprivacy_mime_clear(multipart);
    mailmime_free(multipart);
    res = MAIL_ERROR_FILE;
    goto unlink_description;
  }
  fclose(version_f);
  
  version_mime = mailprivacy_new_file_part(privacy,
      version_filename,
      "application/pgp-encrypted",
      MAILMIME_MECHANISM_8BIT);
  if (r != MAIL_NO_ERROR) {
    mailprivacy_mime_clear(multipart);
    mailmime_free(multipart);
    res = r;
    goto unlink_version;
  }

  r = mailmime_smart_add_part(multipart, version_mime);
  if (r != MAIL_NO_ERROR) {
    mailprivacy_mime_clear(version_mime);
    mailmime_free(version_mime);
    mailprivacy_mime_clear(multipart);
    mailmime_free(multipart);
    res = MAIL_ERROR_MEMORY;
    goto unlink_version;
  }
  
  /* encrypted part */
  
  encrypted_mime = mailprivacy_new_file_part(privacy,
      encrypted_filename,
      "application/octet-stream",
      MAILMIME_MECHANISM_8BIT);
  if (r != MAIL_NO_ERROR) {
    mailprivacy_mime_clear(multipart);
    mailmime_free(multipart);
    res = r;
    goto unlink_version;
  }
  
  r = mailmime_smart_add_part(multipart, encrypted_mime);
  if (r != MAIL_NO_ERROR) {
    mailprivacy_mime_clear(encrypted_mime);
    mailmime_free(encrypted_mime);
    mailprivacy_mime_clear(multipart);
    mailmime_free(multipart);
    res = MAIL_ERROR_MEMORY;
    goto unlink_version;
  }
  
  unlink(version_filename);
  unlink(description_filename);
  unlink(encrypted_filename);
  unlink(original_filename);
  
  * result = multipart;
  
  return MAIL_NO_ERROR;
  
 unlink_version:
  unlink(version_filename);
 unlink_description:
  unlink(description_filename);
 unlink_encrypted:
  unlink(encrypted_filename);
 unlink_original:
  unlink(original_filename);
 err:
  return res;
}


static int pgp_encrypt_mime(struct mailprivacy * privacy,
    mailmessage * msg,
    struct mailmime * mime, struct mailmime ** result)
{
  char original_filename[PATH_MAX];
  FILE * original_f;
  int res;
  int r;
  int col;
  char description_filename[PATH_MAX];
  char encrypted_filename[PATH_MAX];
  char version_filename[PATH_MAX];
  FILE * version_f;
  char command[PATH_MAX];
  char quoted_original_filename[PATH_MAX];
  struct mailmime * version_mime;
  struct mailmime * multipart;
  struct mailmime_content * content;
  struct mailmime_parameter * param;
  struct mailmime * encrypted_mime;
  char recipient[PATH_MAX];
  struct mailimf_fields * fields;
  struct mailmime * root;
  size_t written;
  int encrypt_ok;
  
  root = mime;
  while (root->mm_parent != NULL)
    root = root->mm_parent;
  
  fields = NULL;
  if (root->mm_type == MAILMIME_MESSAGE)
    fields = root->mm_data.mm_message.mm_fields;
  
  /* recipient */
  
  collect_recipient(recipient, sizeof(recipient), fields);
  
  /* part to encrypt */
  
  /* encode quoted printable all text parts */
  
  mailprivacy_prepare_mime(mime);
  
  original_f = mailprivacy_get_tmp_file(privacy,
      original_filename, sizeof(original_filename));
  if (original_f == NULL) {
    res = MAIL_ERROR_FILE;
    goto err;
  }
  
  col = 0;
  r = mailmime_write(original_f, &col, mime);
  if (r != MAILIMF_NO_ERROR) {
    fclose(original_f);
    res = MAIL_ERROR_FILE;
    goto unlink_original;
  }
  
  fclose(original_f);
  
  /* prepare destination file for encryption */
  
  r = mailprivacy_get_tmp_filename(privacy, encrypted_filename,
      sizeof(encrypted_filename));
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto unlink_original;
  }
  
  r = mail_quote_filename(quoted_original_filename,
       sizeof(quoted_original_filename), original_filename);
  if (r < 0) {
    res = MAIL_ERROR_MEMORY;
    goto unlink_encrypted;
  }
  
  r = mailprivacy_get_tmp_filename(privacy, description_filename,
      sizeof(description_filename));
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto unlink_encrypted;
  }
  
  snprintf(command, sizeof(command), "gpg %s -a --batch --yes -e '%s'",
      recipient, quoted_original_filename);
  
  encrypt_ok = 0;
  r = gpg_command_passphrase(privacy, msg, command, NULL,
      encrypted_filename, description_filename);
  switch (r) {
  case NO_ERROR_PGP:
    encrypt_ok = 1;
    break;
  case ERROR_PGP_NOPASSPHRASE:
  case ERROR_PGP_CHECK:
    encrypt_ok = 0;
    break;
  case ERROR_PGP_COMMAND:
    res = MAIL_ERROR_COMMAND;
    goto unlink_description;
  case ERROR_PGP_FILE:
    res = MAIL_ERROR_FILE;
    goto unlink_description;
  }
  
  if (!encrypt_ok) {
    res = MAIL_ERROR_COMMAND;
    goto unlink_description;
  }
  
  /* multipart */
  
  multipart = mailprivacy_new_file_part(privacy, NULL,
      "multipart/encrypted", -1);
  
  content = multipart->mm_content_type;
  
  param = mailmime_param_new_with_data("protocol",
      "application/pgp-encrypted");
  if (param == NULL) {
    mailmime_free(multipart);
    res = MAIL_ERROR_MEMORY;
    goto unlink_description;
  }
  
  r = clist_append(content->ct_parameters, param);
  if (r < 0) {
    mailmime_parameter_free(param);
    mailmime_free(multipart);
    res = MAIL_ERROR_MEMORY;
    goto unlink_description;
  }

  /* version part */
  
  version_f = mailprivacy_get_tmp_file(privacy,
      version_filename, sizeof(version_filename));
  if (version_f == NULL) {
    mailprivacy_mime_clear(multipart);
    mailmime_free(multipart);
    res = MAIL_ERROR_FILE;
    goto unlink_description;
  }
  written = fwrite(PGP_VERSION, 1, sizeof(PGP_VERSION) - 1, version_f);
  if (written != sizeof(PGP_VERSION) - 1) {
    fclose(version_f);
    mailprivacy_mime_clear(multipart);
    mailmime_free(multipart);
    res = MAIL_ERROR_FILE;
    goto unlink_description;
  }
  fclose(version_f);
  
  version_mime = mailprivacy_new_file_part(privacy,
      version_filename,
      "application/pgp-encrypted",
      MAILMIME_MECHANISM_8BIT);
  if (r != MAIL_NO_ERROR) {
    mailprivacy_mime_clear(multipart);
    mailmime_free(multipart);
    res = r;
    goto unlink_version;
  }

  r = mailmime_smart_add_part(multipart, version_mime);
  if (r != MAIL_NO_ERROR) {
    mailprivacy_mime_clear(version_mime);
    mailmime_free(version_mime);
    mailprivacy_mime_clear(multipart);
    mailmime_free(multipart);
    res = MAIL_ERROR_MEMORY;
    goto unlink_version;
  }
  
  /* encrypted part */
  
  encrypted_mime = mailprivacy_new_file_part(privacy,
      encrypted_filename,
      "application/octet-stream",
      MAILMIME_MECHANISM_8BIT);
  if (r != MAIL_NO_ERROR) {
    mailprivacy_mime_clear(multipart);
    mailmime_free(multipart);
    res = r;
    goto unlink_version;
  }
  
  r = mailmime_smart_add_part(multipart, encrypted_mime);
  if (r != MAIL_NO_ERROR) {
    mailprivacy_mime_clear(encrypted_mime);
    mailmime_free(encrypted_mime);
    mailprivacy_mime_clear(multipart);
    mailmime_free(multipart);
    res = MAIL_ERROR_MEMORY;
    goto unlink_version;
  }
  
  unlink(version_filename);
  unlink(description_filename);
  unlink(encrypted_filename);
  unlink(original_filename);
  
  * result = multipart;
  
  return MAIL_NO_ERROR;
  
 unlink_version:
  unlink(version_filename);
 unlink_description:
  unlink(description_filename);
 unlink_encrypted:
  unlink(encrypted_filename);
 unlink_original:
  unlink(original_filename);
 err:
  return res;
}

static int pgp_clear_sign(struct mailprivacy * privacy,
    mailmessage * msg,
    struct mailmime * mime, struct mailmime ** result)
{
  char default_key[PATH_MAX];
  char original_filename[PATH_MAX];
  FILE * original_f;
  char signed_filename[PATH_MAX];
  char description_filename[PATH_MAX];
  char quoted_original_filename[PATH_MAX];
  int col;
  struct mailmime * signed_mime;
  int res;
  int r;
  char command[PATH_MAX];
  struct mailmime_content * content_type;
  char * email;
  int sign_ok;
  
  if (mime->mm_type != MAILMIME_SINGLE) {
    res = MAIL_ERROR_INVAL;
    goto err;
  }
  
  if (mime->mm_data.mm_single == NULL) {
    res = MAIL_ERROR_INVAL;
    goto err;
  }
  
  /* get signing key */
  
  * default_key = '\0';
  email = get_first_from_addr(mime);
  if (email != NULL)
    snprintf(default_key, sizeof(default_key),
        "--default-key %s", email);
  
  /* get part to sign */
  
  original_f = mailprivacy_get_tmp_file(privacy,
      original_filename,
      sizeof(original_filename));
  if (original_f == NULL) {
    res = MAIL_ERROR_FILE;
    goto err;
  }
  
  col = 0;
  r = mailmime_data_write(original_f, &col, mime->mm_data.mm_single, 1);
  if (r != MAILIMF_NO_ERROR) {
    fclose(original_f);
    res = MAIL_ERROR_FILE;
    goto unlink_original;
  }
  fclose(original_f);

  r = mailprivacy_get_tmp_filename(privacy, signed_filename,
      sizeof(signed_filename));
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto unlink_original;
  }

  r = mailprivacy_get_tmp_filename(privacy, description_filename,
      sizeof(description_filename));
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto unlink_signed;
  }
  
  r = mail_quote_filename(quoted_original_filename,
      sizeof(quoted_original_filename), original_filename);
  if (r < 0) {
    res = MAIL_ERROR_MEMORY;
    goto unlink_description;
  }

  snprintf(command, sizeof(command),
      "gpg --passphrase-fd=0 --batch --yes --digest-algo sha1 %s --clearsign '%s'",
      default_key, quoted_original_filename);
  
  sign_ok = 0;
  r = gpg_command_passphrase(privacy, msg, command, NULL,
      signed_filename, description_filename);
  switch (r) {
  case NO_ERROR_PGP:
    sign_ok = 1;
    break;
  case ERROR_PGP_NOPASSPHRASE:
  case ERROR_PGP_CHECK:
    sign_ok = 0;
    break;
  case ERROR_PGP_COMMAND:
    res = MAIL_ERROR_COMMAND;
    goto unlink_description;
  case ERROR_PGP_FILE:
    res = MAIL_ERROR_FILE;
    goto unlink_description;
  }
  
  if (!sign_ok) {
    res = MAIL_ERROR_COMMAND;
    goto unlink_description;
  }
  
  /* building the signed part */
  
  signed_mime = mailprivacy_new_file_part(privacy, signed_filename,
      NULL, MAILMIME_MECHANISM_8BIT);
  if (signed_mime == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto unlink_description;
  }
  
  /* place original content type */
  
  content_type = mailmime_content_dup(mime->mm_content_type);
  if (content_type == NULL) {
    mailprivacy_mime_clear(signed_mime);
    mailmime_free(signed_mime);
    res = MAIL_ERROR_MEMORY;
    goto unlink_description;
  }
  
  mailmime_content_free(signed_mime->mm_content_type);
  signed_mime->mm_content_type = content_type;

  /* place original MIME fields */
  
  if (mime->mm_mime_fields != NULL) {
    struct mailmime_fields * mime_fields;
    clistiter * cur;
    
    mime_fields = mailprivacy_mime_fields_dup(privacy,
        mime->mm_mime_fields);
    if (mime_fields == NULL) {
      mailprivacy_mime_clear(signed_mime);
      mailmime_free(signed_mime);
      res = MAIL_ERROR_MEMORY;
      goto unlink_description;
    }
    for(cur = clist_begin(mime_fields->fld_list) ;
        cur != NULL ; cur = clist_next(cur)) {
      struct mailmime_field * field;
      
      field = clist_content(cur);
      if (field->fld_type == MAILMIME_FIELD_TRANSFER_ENCODING) {
        mailmime_field_free(field);
        clist_delete(mime_fields->fld_list, cur);
        break;
      }
    }
    clist_concat(signed_mime->mm_mime_fields->fld_list,
        mime_fields->fld_list);
    mailmime_fields_free(mime_fields);
  }
  
  unlink(description_filename);
  unlink(signed_filename);
  unlink(original_filename);
  
  * result = signed_mime;
  
  return MAIL_NO_ERROR;
  
 unlink_description:
  unlink(description_filename);
 unlink_signed:
  unlink(signed_filename);
 unlink_original:
  unlink(original_filename);
 err:
  return res;
}


static int pgp_armor_encrypt(struct mailprivacy * privacy,
    mailmessage * msg,
    struct mailmime * mime, struct mailmime ** result)
{
  char original_filename[PATH_MAX];
  FILE * original_f;
  char encrypted_filename[PATH_MAX];
  char quoted_original_filename[PATH_MAX];
  int col;
  struct mailmime * encrypted_mime;
  int res;
  int r;
  char command[PATH_MAX];
  struct mailmime_content * content_type;
  struct mailmime * root;
  struct mailimf_fields * fields;
  char recipient[PATH_MAX];
  int encrypt_ok;
  char description_filename[PATH_MAX];
  
  if (mime->mm_type != MAILMIME_SINGLE) {
    res = MAIL_ERROR_INVAL;
    goto err;
  }
  
  if (mime->mm_data.mm_single == NULL) {
    res = MAIL_ERROR_INVAL;
    goto err;
  }
  
  root = mime;
  while (root->mm_parent != NULL)
    root = root->mm_parent;
  
  fields = NULL;
  if (root->mm_type == MAILMIME_MESSAGE)
    fields = root->mm_data.mm_message.mm_fields;
  
  /* recipient */
  
  collect_recipient(recipient, sizeof(recipient), fields);
  
  /* get part to encrypt */
  
  original_f = mailprivacy_get_tmp_file(privacy, original_filename,
      sizeof(original_filename));
  if (original_f == NULL) {
    res = MAIL_ERROR_FILE;
    goto err;
  }
  
  col = 0;
  r = mailmime_data_write(original_f, &col, mime->mm_data.mm_single, 1);
  if (r != MAILIMF_NO_ERROR) {
    fclose(original_f);
    res = MAIL_ERROR_FILE;
    goto unlink_original;
  }
  fclose(original_f);

  r = mailprivacy_get_tmp_filename(privacy, encrypted_filename,
      sizeof(encrypted_filename));
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto unlink_original;
  }

  r = mailprivacy_get_tmp_filename(privacy, description_filename,
      sizeof(description_filename));
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto unlink_encrypted;
  }

  r = mail_quote_filename(quoted_original_filename,
      sizeof(quoted_original_filename), original_filename);
  if (r < 0) {
    res = MAIL_ERROR_MEMORY;
    goto unlink_description;
  }
  
  snprintf(command, sizeof(command), "gpg %s --batch --yes -e --armor '%s'",
      recipient, quoted_original_filename);
  
  encrypt_ok = 0;
  r = gpg_command_passphrase(privacy, msg, command, NULL,
      encrypted_filename, description_filename);
  switch (r) {
  case NO_ERROR_PGP:
    encrypt_ok = 1;
    break;
  case ERROR_PGP_NOPASSPHRASE:
  case ERROR_PGP_CHECK:
    encrypt_ok = 0;
    break;
  case ERROR_PGP_COMMAND:
    res = MAIL_ERROR_COMMAND;
    goto unlink_description;
  case ERROR_PGP_FILE:
    res = MAIL_ERROR_FILE;
    goto unlink_description;
  }
  
  if (!encrypt_ok) {
    res = MAIL_ERROR_COMMAND;
    goto unlink_description;
  }
  
  /* building the encrypted part */
  
  encrypted_mime = mailprivacy_new_file_part(privacy,
      encrypted_filename,
      "application/octet-stream",
      MAILMIME_MECHANISM_8BIT);
  if (encrypted_mime == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto unlink_description;
  }
  
  /* place original content type */
  
  content_type = mailmime_content_dup(mime->mm_content_type);
  if (content_type == NULL) {
    mailprivacy_mime_clear(encrypted_mime);
    mailmime_free(encrypted_mime);
    res = MAIL_ERROR_MEMORY;
    goto unlink_description;
  }
  
  mailmime_content_free(encrypted_mime->mm_content_type);
  encrypted_mime->mm_content_type = content_type;

  /* place original MIME fields */
  
  if (mime->mm_mime_fields != NULL) {
    struct mailmime_fields * mime_fields;
    clistiter * cur;
    
    mime_fields = mailprivacy_mime_fields_dup(privacy, mime->mm_mime_fields);
    if (mime_fields == NULL) {
      mailprivacy_mime_clear(encrypted_mime);
      mailmime_free(encrypted_mime);
      res = MAIL_ERROR_MEMORY;
      goto unlink_description;
    }
    for(cur = clist_begin(mime_fields->fld_list) ;
        cur != NULL ; cur = clist_next(cur)) {
      struct mailmime_field * field;
      
      field = clist_content(cur);
      if (field->fld_type == MAILMIME_FIELD_TRANSFER_ENCODING) {
        mailmime_field_free(field);
        clist_delete(mime_fields->fld_list, cur);
        break;
      }
    }
    clist_concat(encrypted_mime->mm_mime_fields->fld_list,
        mime_fields->fld_list);
    mailmime_fields_free(mime_fields);
  }

  unlink(description_filename);
  unlink(encrypted_filename);
  unlink(original_filename);
  
  * result = encrypted_mime;
  
  return MAIL_NO_ERROR;
  
 unlink_description:
  unlink(description_filename);
 unlink_encrypted:
  unlink(encrypted_filename);
 unlink_original:
  unlink(original_filename);
 err:
  return res;
}


static int pgp_armor_sign_encrypt(struct mailprivacy * privacy,
    mailmessage * msg,
    struct mailmime * mime, struct mailmime ** result)
{
  char default_key[PATH_MAX];
  char original_filename[PATH_MAX];
  FILE * original_f;
  char encrypted_filename[PATH_MAX];
  char description_filename[PATH_MAX];
  char quoted_original_filename[PATH_MAX];
  int col;
  struct mailmime * encrypted_mime;
  int res;
  int r;
  char command[PATH_MAX];
  struct mailmime_content * content_type;
  struct mailmime * root;
  struct mailimf_fields * fields;
  char recipient[PATH_MAX];
  char * email;
  int encrypt_ok;
  
  if (mime->mm_type != MAILMIME_SINGLE) {
    res = MAIL_ERROR_INVAL;
    goto err;
  }
  
  if (mime->mm_data.mm_single == NULL) {
    res = MAIL_ERROR_INVAL;
    goto err;
  }
  
  /* get signing key */
  
  * default_key = '\0';
  email = get_first_from_addr(mime);
  if (email != NULL)
    snprintf(default_key, sizeof(default_key),
        "--default-key %s", email);
  
  root = mime;
  while (root->mm_parent != NULL)
    root = root->mm_parent;
  
  fields = NULL;
  if (root->mm_type == MAILMIME_MESSAGE)
    fields = root->mm_data.mm_message.mm_fields;
  
  /* recipient */
  
  collect_recipient(recipient, sizeof(recipient), fields);
  
  /* get part to encrypt */
  
  original_f = mailprivacy_get_tmp_file(privacy,
      original_filename,
      sizeof(original_filename));
  if (original_f == NULL) {
    res = MAIL_ERROR_FILE;
    goto err;
  }
  
  col = 0;
  r = mailmime_data_write(original_f, &col, mime->mm_data.mm_single, 1);
  if (r != MAILIMF_NO_ERROR) {
    fclose(original_f);
    res = MAIL_ERROR_FILE;
    goto unlink_original;
  }
  fclose(original_f);

  r = mailprivacy_get_tmp_filename(privacy, encrypted_filename,
      sizeof(encrypted_filename));
  if (r != MAIL_NO_ERROR) {
    res = MAIL_ERROR_FILE;
    goto unlink_original;
  }

  r = mailprivacy_get_tmp_filename(privacy, description_filename,
      sizeof(description_filename));
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto unlink_encrypted;
  }

  r = mail_quote_filename(quoted_original_filename,
      sizeof(quoted_original_filename), original_filename);
  if (r < 0) {
    res = MAIL_ERROR_MEMORY;
    goto unlink_description;
  }

  snprintf(command, sizeof(command),
      "gpg --passphrase-fd=0 %s --batch --yes --digest-algo sha1 "
      "%s -e -s -a '%s'",
      recipient, default_key, quoted_original_filename);

  encrypt_ok = 0;
  r = gpg_command_passphrase(privacy, msg, command, NULL,
      encrypted_filename, description_filename);
  switch (r) {
  case NO_ERROR_PGP:
    encrypt_ok = 1;
    break;
  case ERROR_PGP_NOPASSPHRASE:
  case ERROR_PGP_CHECK:
    encrypt_ok = 0;
    break;
  case ERROR_PGP_COMMAND:
    res = MAIL_ERROR_COMMAND;
    goto unlink_description;
  case ERROR_PGP_FILE:
    res = MAIL_ERROR_FILE;
    goto unlink_description;
  }
  
  if (!encrypt_ok) {
    res = MAIL_ERROR_COMMAND;
    goto unlink_description;
  }
  
  /* building the encrypted part */
  
  encrypted_mime = mailprivacy_new_file_part(privacy,
      encrypted_filename,
      "application/octet-stream",
      MAILMIME_MECHANISM_8BIT);
  if (encrypted_mime == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto unlink_description;
  }
  
  /* place original content type */
  
  content_type = mailmime_content_dup(mime->mm_content_type);
  if (content_type == NULL) {
    mailprivacy_mime_clear(encrypted_mime);
    mailmime_free(encrypted_mime);
    res = MAIL_ERROR_MEMORY;
    goto unlink_description;
  }
  
  mailmime_content_free(encrypted_mime->mm_content_type);
  encrypted_mime->mm_content_type = content_type;

  /* place original MIME fields */
  
  if (mime->mm_mime_fields != NULL) {
    struct mailmime_fields * mime_fields;
    clistiter * cur;
    
    mime_fields = mailprivacy_mime_fields_dup(privacy, mime->mm_mime_fields);
    if (mime_fields == NULL) {
      mailprivacy_mime_clear(encrypted_mime);
      mailmime_free(encrypted_mime);
      res = MAIL_ERROR_MEMORY;
      goto unlink_description;
    }
    for(cur = clist_begin(mime_fields->fld_list) ;
        cur != NULL ; cur = clist_next(cur)) {
      struct mailmime_field * field;
      
      field = clist_content(cur);
      if (field->fld_type == MAILMIME_FIELD_TRANSFER_ENCODING) {
        mailmime_field_free(field);
        clist_delete(mime_fields->fld_list, cur);
        break;
      }
    }
    clist_concat(encrypted_mime->mm_mime_fields->fld_list,
        mime_fields->fld_list);
    mailmime_fields_free(mime_fields);
  }

  unlink(description_filename);
  unlink(encrypted_filename);
  unlink(original_filename);
  
  * result = encrypted_mime;
  
  return MAIL_NO_ERROR;
  
 unlink_description:
  unlink(description_filename);
 unlink_encrypted:
  unlink(encrypted_filename);
 unlink_original:
  unlink(original_filename);
 err:
  return res;
}

#if ! defined __STDC_VERSION__ || __STDC_VERSION__ < 199901L
static struct mailprivacy_encryption pgp_encryption_tab[] = {
  /* PGP signed part */
  {
    /* name */ "signed",
    /* description */ "PGP signed part",
    /* encrypt */ pgp_sign_mime,
  },
  
  /* pgp encrypted part */
  
  {
    /* name */ "encrypted",
    /* description */ "PGP encrypted part",
    /* encrypt */ pgp_encrypt_mime,
  },
  
  /* PGP signed & encrypted part */
  
  {
    /* name */ "signed-encrypted",
    /* description */ "PGP signed & encrypted part",
    /* encrypt */ pgp_sign_encrypt_mime,
  },

  /* PGP clear signed part */
  
  {
    /* name */ "clear-signed",
    /* description */ "PGP clear signed part",
    /* encrypt */ pgp_clear_sign,
  },

  /* PGP armor encrypted part */
  
  {
    /* name */ "encrypted-armor",
    /* description */ "PGP ASCII armor encrypted part",
    /* encrypt */ pgp_armor_encrypt,
  },

  /* PGP armor signed & encrypted part */
  
  {
    /* name */ "signed-encrypted-armor",
    /* description */ "PGP ASCII armor signed & encrypted part",
    /* encrypt */ pgp_armor_sign_encrypt,
  },
};
#else
static struct mailprivacy_encryption pgp_encryption_tab[] = {
  /* PGP signed part */
  {
    .name = "signed",
    .description = "PGP signed part",
    .encrypt = pgp_sign_mime,
  },
  
  /* pgp encrypted part */
  
  {
    .name = "encrypted",
    .description = "PGP encrypted part",
    .encrypt = pgp_encrypt_mime,
  },
  
  /* PGP signed & encrypted part */
  
  {
    .name = "signed-encrypted",
    .description = "PGP signed & encrypted part",
    .encrypt = pgp_sign_encrypt_mime,
  },

  /* PGP clear signed part */
  
  {
    .name = "clear-signed",
    .description = "PGP clear signed part",
    .encrypt = pgp_clear_sign,
  },

  /* PGP armor encrypted part */
  
  {
    .name = "encrypted-armor",
    .description = "PGP ASCII armor encrypted part",
    .encrypt = pgp_armor_encrypt,
  },

  /* PGP armor signed & encrypted part */
  
  {
    .name = "signed-encrypted-armor",
    .description = "PGP ASCII armor signed & encrypted part",
    .encrypt = pgp_armor_sign_encrypt,
  },
};
#endif

#if ! defined __STDC_VERSION__ || __STDC_VERSION__ < 199901L
static struct mailprivacy_protocol pgp_protocol = {
  /* name */ "pgp",
  /* description */ "OpenPGP",
  
  /* is_encrypted */ pgp_test_encrypted,
  /* decrypt */ pgp_handler,
  
  /* encryption_count */
  (sizeof(pgp_encryption_tab) / sizeof(pgp_encryption_tab[0])),
  
  /* encryption_tab */ pgp_encryption_tab,
};
#else
static struct mailprivacy_protocol pgp_protocol = {
  .name = "pgp",
  .description = "OpenPGP",
  
  .is_encrypted = pgp_test_encrypted,
  .decrypt = pgp_handler,
  
  .encryption_count =
  (sizeof(pgp_encryption_tab) / sizeof(pgp_encryption_tab[0])),
  
  .encryption_tab = pgp_encryption_tab,
};
#endif

int mailprivacy_gnupg_init(struct mailprivacy * privacy)
{
  return mailprivacy_register(privacy, &pgp_protocol);
}

void mailprivacy_gnupg_done(struct mailprivacy * privacy)
{
  mailprivacy_unregister(privacy, &pgp_protocol);
}


#ifdef LIBETPAN_REENTRANT
static pthread_mutex_t encryption_id_hash_lock = PTHREAD_MUTEX_INITIALIZER;
#endif
static chash * encryption_id_hash = NULL;

static clist * get_list(struct mailprivacy * privacy, mailmessage * msg)
{
  clist * encryption_id_list;
  
  encryption_id_list = NULL;
  if (encryption_id_hash != NULL) {
    chashdatum key;
    chashdatum value;
    int r;
    
    key.data = &msg;
    key.len = sizeof(msg);
    r = chash_get(encryption_id_hash, &key, &value);
    if (r == 0) {
      encryption_id_list = value.data;
    }
  }
  
  return encryption_id_list;
}

void mailprivacy_gnupg_encryption_id_list_clear(struct mailprivacy * privacy,
    mailmessage * msg)
{
  clist * encryption_id_list;
  clistiter * iter;
  
#ifdef LIBETPAN_REENTRANT
  pthread_mutex_lock(&encryption_id_hash_lock);
#endif
  encryption_id_list = get_list(privacy, msg);
  if (encryption_id_list != NULL) {
    chashdatum key;
    
    for(iter = clist_begin(encryption_id_list) ;
        iter != NULL ; iter = clist_next(iter)) {
      char * str;
      
      str = clist_content(iter);
      free(str);
    }
    clist_free(encryption_id_list);
    
    key.data = &msg;
    key.len = sizeof(msg);
    chash_delete(encryption_id_hash, &key, NULL);
    
    if (chash_count(encryption_id_hash) == 0) {
      chash_free(encryption_id_hash);
      encryption_id_hash = NULL;
    }
  }
#ifdef LIBETPAN_REENTRANT
  pthread_mutex_unlock(&encryption_id_hash_lock);
#endif
}

clist * mailprivacy_gnupg_encryption_id_list(struct mailprivacy * privacy,
    mailmessage * msg)
{
  clist * encryption_id_list;
  
#ifdef LIBETPAN_REENTRANT
  pthread_mutex_lock(&encryption_id_hash_lock);
#endif
  encryption_id_list = get_list(privacy, msg);
#ifdef LIBETPAN_REENTRANT
  pthread_mutex_unlock(&encryption_id_hash_lock);
#endif
  
  return encryption_id_list;
}

static int mailprivacy_gnupg_add_encryption_id(struct mailprivacy * privacy,
    mailmessage * msg, char * encryption_id)
{
  clist * encryption_id_list;
  int r;
  int res;
  
#ifdef LIBETPAN_REENTRANT
  pthread_mutex_lock(&encryption_id_hash_lock);
#endif
  
  res = -1;
  
  encryption_id_list = get_list(privacy, msg);
  if (encryption_id_list == NULL) {
    if (encryption_id_hash == NULL)
      encryption_id_hash = chash_new(CHASH_DEFAULTSIZE, CHASH_COPYKEY);
    
    if (encryption_id_hash != NULL) {
      encryption_id_list = clist_new();
      if (encryption_id_list != NULL) {
        chashdatum key;
        chashdatum value;
        
        key.data = &msg;
        key.len = sizeof(msg);
        value.data = encryption_id_list;
        value.len = 0;
        r = chash_set(encryption_id_hash, &key, &value, NULL);
        if (r < 0)
          clist_free(encryption_id_list);
      }
    }
  }
  
  encryption_id_list = get_list(privacy, msg);
  if (encryption_id_list != NULL) {
    char * str;
    
    str = strdup(encryption_id);
    if (str != NULL) {
      r = clist_append(encryption_id_list, str);
      if (r < 0) {
        free(str);
      }
      else {
        res = 0;
      }
    }
  }
  
#ifdef LIBETPAN_REENTRANT
  pthread_mutex_unlock(&encryption_id_hash_lock);
#endif
  
  return res;
}

#define MAX_EMAIL_SIZE 1024

static chash * passphrase_hash = NULL;

int mailprivacy_gnupg_set_encryption_id(struct mailprivacy * privacy,
    char * user_id, char * passphrase)
{
  chashdatum key;
  chashdatum value;
  int r;
  char buf[MAX_EMAIL_SIZE];
  char * n;
  
  strncpy(buf, user_id, sizeof(buf));
  buf[sizeof(buf) - 1] = '\0';
  for(n = buf ; * n != '\0' ; n ++)
    * n = toupper((unsigned char) * n);
  
  if (passphrase_hash == NULL) {
    passphrase_hash = chash_new(CHASH_DEFAULTSIZE, CHASH_COPYALL);
    if (passphrase_hash == NULL)
      return MAIL_ERROR_MEMORY;
  }
  
  key.data = buf;
  key.len = strlen(buf) + 1;
  value.data = passphrase;
  value.len = strlen(passphrase) + 1;
  
  r = chash_set(passphrase_hash, &key, &value, NULL);
  if (r < 0) {
    return MAIL_ERROR_MEMORY;
  }
  
  return MAIL_NO_ERROR;
}

static char * get_passphrase(struct mailprivacy * privacy,
    char * user_id)
{
  chashdatum key;
  chashdatum value;
  int r;
  char * passphrase;
  char buf[MAX_EMAIL_SIZE];
  char * n;
  
  strncpy(buf, user_id, sizeof(buf));
  buf[sizeof(buf) - 1] = '\0';
  for(n = buf ; * n != '\0' ; n ++)
    * n = toupper((unsigned char) * n);
  
  if (passphrase_hash == NULL)
    return NULL;
  
  key.data = buf;
  key.len = strlen(buf) + 1;
  
  r = chash_get(passphrase_hash, &key, &value);
  if (r < 0)
    return NULL;
  
  passphrase = strdup(value.data);
  
  return passphrase;
}
