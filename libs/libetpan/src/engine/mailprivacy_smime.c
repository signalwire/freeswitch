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
 * $Id: mailprivacy_smime.c,v 1.15 2006/08/29 22:27:35 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailprivacy_smime.h"
#include <string.h>
#include <pthread.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include "mailprivacy_tools.h"
#include "mailprivacy.h"
#include <stdlib.h>
#include <dirent.h>
#include <stdio.h>
#include <ctype.h>
#include <libetpan/libetpan-config.h>

/*
  global variable
  
  TODO : instance of privacy drivers
*/

static int smime_command_passphrase(struct mailprivacy * privacy,
    struct mailmessage * msg,
    char * command,
    char * passphrase,
    char * stdoutfile, char * stderrfile);
static char * get_passphrase(struct mailprivacy * privacy,
    char * user_id);
static int mailprivacy_smime_add_encryption_id(struct mailprivacy * privacy,
    mailmessage * msg, char * encryption_id);

static char cert_dir[PATH_MAX] = "";
static chash * certificates = NULL;
static chash * private_keys = NULL;
static char CAcert_dir[PATH_MAX] = "";
static char * CAfile = NULL;
static int CA_check = 1;
static int store_cert = 0;
static char private_keys_dir[PATH_MAX] = "";

static char * get_cert_file(char * email);

static char * get_private_key_file(char * email);


static int smime_is_signed(struct mailmime * mime)
{
  if (mime->mm_content_type != NULL) {
    if ((strcasecmp(mime->mm_content_type->ct_subtype, "x-pkcs7-mime") == 0) ||
        (strcasecmp(mime->mm_content_type->ct_subtype, "pkcs7-mime") == 0)) {
      clistiter * cur;
      
      for(cur = clist_begin(mime->mm_content_type->ct_parameters) ;
          cur != NULL ;
          cur = clist_next(cur)) {
        struct mailmime_parameter * param;
        
        param = cur->data;
        
        if ((strcasecmp(param->pa_name, "smime-type") == 0) &&
            (strcasecmp(param->pa_value, "signed-data") == 0))
          return 1;
      }
      
      return 0;
    }
    else {
      clistiter * cur;
      
      for(cur = clist_begin(mime->mm_content_type->ct_parameters) ;
          cur != NULL ;
          cur = clist_next(cur)) {
        struct mailmime_parameter * param;
        
        param = cur->data;
        
        if ((strcasecmp(param->pa_name, "protocol") == 0) &&
            ((strcasecmp(param->pa_value,
                  "application/x-pkcs7-signature") == 0) ||
                (strcasecmp(param->pa_value,
                    "application/pkcs7-signature") == 0)))
          return 1;
      }
    }
  }
  
  return 0;
}

static int smime_is_encrypted(struct mailmime * mime)
{
  if (mime->mm_content_type != NULL) {
    if ((strcasecmp(mime->mm_content_type->ct_subtype, "x-pkcs7-mime") == 0) ||
        (strcasecmp(mime->mm_content_type->ct_subtype, "pkcs7-mime") == 0)) {
      clistiter * cur;
      
      for(cur = clist_begin(mime->mm_content_type->ct_parameters) ;
          cur != NULL ;
          cur = clist_next(cur)) {
        struct mailmime_parameter * param;
        
        param = cur->data;
        
        if ((strcasecmp(param->pa_name, "smime-type") == 0) &&
            (strcasecmp(param->pa_value, "signed-data") == 0))
          return 0;
      }
      
      return 1;
    }
  }
  
  return 0;
}


enum {
  NO_ERROR_SMIME = 0,
  ERROR_SMIME_CHECK,
  ERROR_SMIME_COMMAND,
  ERROR_SMIME_FILE,
  ERROR_SMIME_NOPASSPHRASE
};

#define BUF_SIZE 1024

static char * get_first_from_addr(struct mailmime * mime)
{
  clistiter * cur;
  struct mailimf_single_fields single_fields;
  struct mailimf_fields * fields;
  struct mailimf_mailbox * mb;
  
  while (mime->mm_parent != NULL)
    mime = mime->mm_parent;
  
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

#define SMIME_DECRYPT_DESCRIPTION "S/MIME encrypted part\r\n"
#define SMIME_DECRYPT_FAILED "S/MIME decryption FAILED\r\n"
#define SMIME_DECRYPT_SUCCESS "S/MIME decryption success\r\n"

/* passphrase will be needed */

static int smime_decrypt(struct mailprivacy * privacy,
    mailmessage * msg,
    struct mailmime * mime, struct mailmime ** result)
{
  char smime_filename[PATH_MAX];
  char quoted_smime_filename[PATH_MAX];
  char description_filename[PATH_MAX];
  char decrypted_filename[PATH_MAX];
  char command[PATH_MAX];
  struct mailmime * description_mime;
  struct mailmime * decrypted_mime;
  int r;
  int res;
  int sign_ok;
  struct mailmime * multipart;
  char * smime_cert;
  char * smime_key;
  char quoted_smime_cert[PATH_MAX];
  char quoted_smime_key[PATH_MAX];
  char * email;
  chashiter * iter;
  
  /* fetch the whole multipart and write it to a file */
  
  r = mailprivacy_fetch_mime_body_to_file(privacy,
      smime_filename, sizeof(smime_filename),
      msg, mime);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto err;
  }
  
  /* we are in a safe directory */
  
  r = mailprivacy_get_tmp_filename(privacy, decrypted_filename,
      sizeof(decrypted_filename));
  if (r != MAIL_NO_ERROR) {
    res = MAIL_ERROR_FILE;
    goto unlink_smime;
  }
  
  /* description */
  
  r = mailprivacy_get_tmp_filename(privacy, description_filename,
      sizeof(description_filename));
  if (r != MAIL_NO_ERROR) {
    res = MAIL_ERROR_FILE;
    goto unlink_decrypted;
  }
  
  sign_ok = 0;
  for(iter = chash_begin(private_keys) ; iter != NULL ;
      iter = chash_next(private_keys, iter)) {
    chashdatum key;
    char email_buf[BUF_SIZE];
    
    chash_key(iter, &key);
    
    if (key.len >= sizeof(email_buf))
      continue;
    
    strncpy(email_buf, key.data, key.len);
    email_buf[key.len] = '\0';
    email = email_buf;
    
    /* get encryption key */
    
    smime_key = get_private_key_file(email);
    smime_cert = get_cert_file(email);
    if ((smime_cert == NULL) || (smime_key == NULL)) {
      res = MAIL_ERROR_INVAL;
      goto unlink_description;
    }
  
    r = mail_quote_filename(quoted_smime_cert, sizeof(quoted_smime_cert),
        smime_cert);
    if (r < 0) {
      res = MAIL_ERROR_MEMORY;
      goto unlink_description;
    }
  
    r = mail_quote_filename(quoted_smime_key, sizeof(quoted_smime_key),
        smime_key);
    if (r < 0) {
      res = MAIL_ERROR_MEMORY;
      goto unlink_description;
    }
  
    /* run the command */
  
    r = mail_quote_filename(quoted_smime_filename,
        sizeof(quoted_smime_filename), smime_filename);
    if (r < 0) {
      res = MAIL_ERROR_MEMORY;
      goto unlink_description;
    }
    
    sign_ok = 0;
    snprintf(command, sizeof(command),
        "openssl smime -decrypt -passin fd:0 -in '%s' -inkey '%s' -recip '%s'",
        quoted_smime_filename, quoted_smime_key, quoted_smime_cert);
    
    unlink(description_filename);
    r = smime_command_passphrase(privacy, msg, command,
        email, decrypted_filename, description_filename);
    switch (r) {
    case NO_ERROR_SMIME:
      sign_ok = 1;
      break;
    case ERROR_SMIME_CHECK:
    case ERROR_SMIME_NOPASSPHRASE:
      sign_ok = 0;
      break;
    case ERROR_SMIME_COMMAND:
      res = MAIL_ERROR_COMMAND;
      goto unlink_description;
    case ERROR_SMIME_FILE:
      res = MAIL_ERROR_FILE;
      goto unlink_description;
    }
    
    if (sign_ok) {
      break;
    }
  }
  
  if (!sign_ok) {
    if (chash_count(private_keys) == 0) {
      FILE * description_f;
      
      description_f = mailprivacy_get_tmp_file(privacy, description_filename,
          sizeof(description_filename));
      if (description_f == NULL) {
        res = MAIL_ERROR_FILE;
        goto unlink_decrypted;
      }
      fprintf(description_f, SMIME_DECRYPT_FAILED);
      fclose(description_f);
    }
  }
  else {
    mailprivacy_smime_encryption_id_list_clear(privacy, msg);
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
  unlink(smime_filename);
  
  * result = multipart;
  
  return MAIL_NO_ERROR;
  
 unlink_description:
  unlink(description_filename);
 unlink_decrypted:
  unlink(decrypted_filename);
 unlink_smime:
  unlink(smime_filename);
 err:
  return res;
}


static int get_cert_from_sig(struct mailprivacy * privacy,
    mailmessage * msg,
    struct mailmime * mime);

#define SMIME_VERIFY_DESCRIPTION "S/MIME verify signed message\r\n"
#define SMIME_VERIFY_FAILED "S/MIME verification FAILED\r\n"
#define SMIME_VERIFY_SUCCESS "S/MIME verification success\r\n"

static int
smime_verify(struct mailprivacy * privacy,
    mailmessage * msg,
    struct mailmime * mime, struct mailmime ** result)
{
  char smime_filename[PATH_MAX];
  char quoted_smime_filename[PATH_MAX];
  int res;
  int r;
  char command[PATH_MAX];
  int sign_ok;
  struct mailmime * description_mime;
  char description_filename[PATH_MAX];
  struct mailmime * multipart;
  char stripped_filename[PATH_MAX];
  struct mailmime * stripped_mime;
  char check_CA[PATH_MAX];
  char quoted_CAfile[PATH_MAX];
  char noverify[PATH_MAX];
  
  if (store_cert)
    get_cert_from_sig(privacy, msg, mime);
  
  * check_CA = '\0';
  if (CAfile != NULL) {
    r = mail_quote_filename(quoted_CAfile, sizeof(quoted_CAfile), CAfile);
    if (r < 0) {
      res = MAIL_ERROR_MEMORY;
      goto err;
    }
    
    snprintf(check_CA, sizeof(check_CA), "-CAfile '%s'", quoted_CAfile);
  }
  
  * noverify = '\0';
  if (!CA_check) {
    snprintf(noverify, sizeof(noverify), "-noverify");
  }
  
  /* fetch the whole multipart and write it to a file */
  
  r = mailprivacy_fetch_mime_body_to_file(privacy,
      smime_filename, sizeof(smime_filename),
      msg, mime);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto err;
  }
  
  r = mailprivacy_get_tmp_filename(privacy,stripped_filename,
      sizeof(stripped_filename));
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto unlink_smime;
  }
  
  /* description */
  
  r = mailprivacy_get_tmp_filename(privacy, description_filename,
      sizeof(description_filename));
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto unlink_stripped;
  }
  
  /* run the command */

  r = mail_quote_filename(quoted_smime_filename,
      sizeof(quoted_smime_filename), smime_filename);
  if (r < 0) {
    res = MAIL_ERROR_MEMORY;
    goto unlink_description;
  }

  sign_ok = 0;
  snprintf(command, sizeof(command), "openssl smime -verify -in '%s' %s %s",
      quoted_smime_filename, check_CA, noverify);
  
  r = smime_command_passphrase(privacy, msg, command,
      NULL, stripped_filename, description_filename);
  switch (r) {
  case NO_ERROR_SMIME:
    sign_ok = 1;
    break;
  case ERROR_SMIME_NOPASSPHRASE:
  case ERROR_SMIME_CHECK:
    sign_ok = 0;
    break;
  case ERROR_SMIME_COMMAND:
    res = MAIL_ERROR_COMMAND;
    goto unlink_description;
  case ERROR_SMIME_FILE:
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
  
  /* insert the signed part */
  if (!sign_ok) {
    if (mime->mm_type == MAILMIME_MULTIPLE) {
      clistiter * child_iter;
      struct mailmime * child;
      
      child_iter = clist_begin(mime->mm_data.mm_multipart.mm_mp_list);
      child = clist_content(child_iter);
      
      r = mailprivacy_fetch_mime_body_to_file(privacy,
          stripped_filename, sizeof(stripped_filename),
          msg, child);
    }
  }
  
  r = mailprivacy_get_part_from_file(privacy, 1, 0,
      stripped_filename, &stripped_mime);
  if (r != MAIL_NO_ERROR) {
    mailprivacy_mime_clear(multipart);
    mailmime_free(multipart);
    res = r;
    goto unlink_description;
  }
  
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
  /* unlink(smime_filename); */
  
  * result = multipart;
  
  return MAIL_NO_ERROR;
  
 unlink_description:
  unlink(description_filename);
 unlink_stripped:
  unlink(stripped_filename);
 unlink_smime:
  unlink(smime_filename);
 err:
  return res;
}

static int smime_test_encrypted(struct mailprivacy * privacy,
    mailmessage * msg,
    struct mailmime * mime)
{
  switch (mime->mm_type) {
  case MAILMIME_MULTIPLE:
    return smime_is_signed(mime);
    
  case MAILMIME_SINGLE:
    return smime_is_encrypted(mime) || smime_is_signed(mime);
  }
  
  return 0;
}

static int smime_handler(struct mailprivacy * privacy,
    mailmessage * msg,
    struct mailmime * mime, struct mailmime ** result)
{
  int r;
  struct mailmime * alternative_mime;
  
  alternative_mime = NULL;
  switch (mime->mm_type) {
  case MAILMIME_MULTIPLE:
    r = MAIL_ERROR_INVAL;
    if (smime_is_signed(mime))
      r = smime_verify(privacy, msg, mime, &alternative_mime);
    
    if (r != MAIL_NO_ERROR)
      return r;

    * result = alternative_mime;
    
    return MAIL_NO_ERROR;
    
  case MAILMIME_SINGLE:
    r = MAIL_ERROR_INVAL;
    if (smime_is_encrypted(mime))
      r = smime_decrypt(privacy, msg, mime, &alternative_mime);
    else if (smime_is_signed(mime))
      r = smime_verify(privacy, msg, mime, &alternative_mime);
    
    if (r != MAIL_NO_ERROR)
      return r;

    * result = alternative_mime;
    
    return MAIL_NO_ERROR;
  }
  
  return MAIL_ERROR_INVAL;
}




static void strip_mime_headers(struct mailmime * mime)
{
  struct mailmime_fields * fields;
  clistiter * cur;
  
  fields = mime->mm_mime_fields;
  
  if (fields == NULL)
    return;
  
  for(cur = clist_begin(fields->fld_list) ; cur != NULL ;
      cur = clist_next(cur)) {
    struct mailmime_field * field;
    
    field = clist_content(cur);
    
    if (field->fld_type == MAILMIME_FIELD_VERSION) {
      mailmime_field_free(field);
      clist_delete(fields->fld_list, cur);
      break;
    }
  }
}

/* passphrase is needed */

static int smime_sign(struct mailprivacy * privacy,
    mailmessage * msg,
    struct mailmime * mime, struct mailmime ** result)
{
  char signed_filename[PATH_MAX];
  FILE * signed_f;
  int res;
  int r;
  int col;
  char description_filename[PATH_MAX];
  char signature_filename[PATH_MAX];
  char command[PATH_MAX];
  char quoted_signed_filename[PATH_MAX];
  struct mailmime * signed_mime;
  char * smime_cert;
  char * smime_key;
  char quoted_smime_cert[PATH_MAX];
  char quoted_smime_key[PATH_MAX];
  char * email;
  
  /* get signing key */
  
  email = get_first_from_addr(mime);
  if (email == NULL) {
    res = MAIL_ERROR_INVAL;
    goto err;
  }
  
  smime_key = get_private_key_file(email);
  smime_cert = get_cert_file(email);
  if ((smime_cert == NULL) || (smime_key == NULL)) {
    res = MAIL_ERROR_INVAL;
    goto err;
  }

  /* part to sign */

  /* encode quoted printable all text parts */
  
  mailprivacy_prepare_mime(mime);
  
  signed_f = mailprivacy_get_tmp_file(privacy,
      signed_filename, sizeof(signed_filename));
  if (signed_f == NULL) {
    res = MAIL_ERROR_FILE;
    goto err;
  }
  
  col = 0;
  r = mailmime_write(signed_f, &col, mime);
  if (r != MAILIMF_NO_ERROR) {
    fclose(signed_f);
    res = MAIL_ERROR_FILE;
    goto unlink_signed;
  }
  
  fclose(signed_f);
  
  /* prepare destination file for signature */
  
  r = mailprivacy_get_tmp_filename(privacy, signature_filename,
      sizeof(signature_filename));
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto unlink_signed;
  }

  r = mailprivacy_get_tmp_filename(privacy, description_filename,
      sizeof(description_filename));
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto unlink_signature;
  }
  
  r = mail_quote_filename(quoted_signed_filename,
       sizeof(quoted_signed_filename), signed_filename);
  if (r < 0) {
    res = MAIL_ERROR_MEMORY;
    goto unlink_description;
  }
  
  r = mail_quote_filename(quoted_smime_key,
       sizeof(quoted_smime_key), smime_key);
  if (r < 0) {
    res = MAIL_ERROR_MEMORY;
    goto unlink_description;
  }

  r = mail_quote_filename(quoted_smime_cert,
       sizeof(quoted_smime_cert), smime_cert);
  if (r < 0) {
    res = MAIL_ERROR_MEMORY;
    goto unlink_description;
  }

  snprintf(command, sizeof(command),
      "openssl smime -sign -passin fd:0 -in '%s' -signer '%s' -inkey '%s'",
      quoted_signed_filename,
      quoted_smime_cert, quoted_smime_key);

  r = smime_command_passphrase(privacy, msg, command,
      email, signature_filename, description_filename);
  switch (r) {
  case NO_ERROR_SMIME:
    break;
  case ERROR_SMIME_NOPASSPHRASE:
  case ERROR_SMIME_CHECK:
    res = MAIL_ERROR_COMMAND;
    goto unlink_description;
    break;
  case ERROR_SMIME_COMMAND:
    res = MAIL_ERROR_COMMAND;
    goto unlink_description;
  case ERROR_SMIME_FILE:
    res = MAIL_ERROR_FILE;
    goto unlink_description;
  }
  
  /* signature part */
  
  r = mailprivacy_get_part_from_file(privacy, 0, 0,
      signature_filename, &signed_mime);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto unlink_description;
  }
  strip_mime_headers(signed_mime);
  
  unlink(description_filename);
  /* unlink(signature_filename); */
  /* unlink(signed_filename); */
  
  * result = signed_mime;
  
  return MAIL_NO_ERROR;
  
 unlink_description:
  unlink(description_filename);
 unlink_signature:
  unlink(signature_filename);
 unlink_signed:
  unlink(signed_filename);
 err:
  return res;
}


/* ********************************************************************* */
/* find S/MIME recipient */

static int recipient_add_mb(char * recipient, size_t * len,
    struct mailimf_mailbox * mb)
{
  char * filename;
  char quoted_filename[PATH_MAX];
  size_t buflen;
  int r;
  
  if (mb->mb_addr_spec == NULL)
    return MAIL_NO_ERROR;
  
  filename = get_cert_file(mb->mb_addr_spec);
  if (filename == NULL)
    return MAIL_ERROR_INVAL;
  
  r = mail_quote_filename(quoted_filename, sizeof(quoted_filename),
      filename);
  if (r < 0)
    return MAIL_ERROR_MEMORY;
    
  buflen = strlen(quoted_filename + 1);
  if (buflen >= * len)
    return MAIL_ERROR_MEMORY;
  
  strncat(recipient, "\'", * len);
  (* len) --;
  strncat(recipient, quoted_filename, * len);
  (* len) -= buflen;
  strncat(recipient, "\'", * len);
  (* len) --;
  strncat(recipient, " ", * len);
  (* len) --;
  
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

static int collect_smime_cert(char * recipient, size_t size,
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
      if (r < 0) {
        res = r;
        goto err;
      }
    }
  }
  
  return MAIL_NO_ERROR;
  
 err:
  return res;
}



static int smime_encrypt(struct mailprivacy * privacy,
    mailmessage * msg,
    struct mailmime * mime, struct mailmime ** result)
{
  char encrypted_filename[PATH_MAX];
  int res;
  int r;
  int col;
  char description_filename[PATH_MAX];
  char decrypted_filename[PATH_MAX];
  FILE * decrypted_f;
  char command[PATH_MAX];
  char quoted_decrypted_filename[PATH_MAX];
  struct mailmime * encrypted_mime;
  struct mailmime * root;
  struct mailimf_fields * fields;
  char recipient[PATH_MAX];
  
  root = mime;
  while (root->mm_parent != NULL)
    root = root->mm_parent;
  
  fields = NULL;
  if (root->mm_type == MAILMIME_MESSAGE)
    fields = root->mm_data.mm_message.mm_fields;

  /* recipient */
  r = collect_smime_cert(recipient, sizeof(recipient), fields);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto err;
  }

  /* part to encrypt */

  /* encode quoted printable all text parts */
  
  mailprivacy_prepare_mime(mime);
  
  decrypted_f = mailprivacy_get_tmp_file(privacy,
      decrypted_filename,
      sizeof(decrypted_filename));
  if (decrypted_f == NULL) {
    res = MAIL_ERROR_FILE;
    goto err;
  }
  
  col = 0;
  r = mailmime_write(decrypted_f, &col, mime);
  if (r != MAILIMF_NO_ERROR) {
    fclose(decrypted_f);
    res = MAIL_ERROR_FILE;
    goto unlink_decrypted;
  }
  
  fclose(decrypted_f);
  
  /* prepare destination file for encryption */
  
  r = mailprivacy_get_tmp_filename(privacy, encrypted_filename,
      sizeof(encrypted_filename));
  if (r != MAIL_NO_ERROR) {
    res = MAIL_ERROR_FILE;
    goto unlink_decrypted;
  }
  
  r = mailprivacy_get_tmp_filename(privacy, description_filename,
      sizeof(description_filename));
  if (r != MAIL_NO_ERROR) {
    res = MAIL_ERROR_FILE;
    goto unlink_encrypted;
  }
  
  r = mail_quote_filename(quoted_decrypted_filename,
      sizeof(quoted_decrypted_filename), decrypted_filename);
  if (r < 0) {
    res = MAIL_ERROR_MEMORY;
    goto unlink_description;
  }
  
  snprintf(command, sizeof(command), "openssl smime -encrypt -in '%s' %s",
      quoted_decrypted_filename, recipient);
  
  r = smime_command_passphrase(privacy, msg, command,
      NULL, encrypted_filename, description_filename);
  switch (r) {
  case NO_ERROR_SMIME:
    break;
  case ERROR_SMIME_NOPASSPHRASE:
  case ERROR_SMIME_CHECK:
    res = MAIL_ERROR_COMMAND;
    goto unlink_description;
    break;
  case ERROR_SMIME_COMMAND:
    res = MAIL_ERROR_COMMAND;
    goto unlink_encrypted;
  case ERROR_SMIME_FILE:
    res = MAIL_ERROR_FILE;
    goto unlink_description;
  }
  
  /* encrypted part */
  
  r = mailprivacy_get_part_from_file(privacy, 0, 0,
      encrypted_filename, &encrypted_mime);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto unlink_description;
  }
  strip_mime_headers(encrypted_mime);

  unlink(description_filename);
  unlink(encrypted_filename);
  unlink(decrypted_filename);
  
  * result = encrypted_mime;
  
  return MAIL_NO_ERROR;

 unlink_description:
  unlink(description_filename);
 unlink_encrypted:
  unlink(encrypted_filename);
 unlink_decrypted:
  unlink(decrypted_filename);
 err:
  return res;
}


/* passphrase will be needed */

static int smime_sign_encrypt(struct mailprivacy * privacy,
    mailmessage * msg,
    struct mailmime * mime, struct mailmime ** result)
{
  struct mailmime * signed_part;
  struct mailmime * encrypted;
  int r;
  int res;
  
  r = smime_sign(privacy, msg, mime, &signed_part);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto err;
  }
  
  r = smime_encrypt(privacy, msg, signed_part, &encrypted);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto free_signed;
  }
  
  * result = encrypted;
  
  return MAIL_NO_ERROR;
  
 free_signed:
  mailprivacy_mime_clear(signed_part);
  mailmime_free(signed_part);
 err:
  return res;
}


#if ! defined __STDC_VERSION__ || __STDC_VERSION__ < 199901L
static struct mailprivacy_encryption smime_encryption_tab[] = {
  /* S/MIME signed part */
  {
    /* name */ "signed",
    /* description */ "S/MIME signed part",
    /* encrypt */ smime_sign,
  },
  
  /* S/MIME encrypted part */
  
  {
    /* name */ "encrypted",
    /* description */ "S/MIME encrypted part",
    /* encrypt */ smime_encrypt,
  },

  /* S/MIME signed & encrypted part */
  
  {
    /* name */ "signed-encrypted",
    /* description */ "S/MIME signed & encrypted part",
    /* encrypt */ smime_sign_encrypt,
  },
};
#else
static struct mailprivacy_encryption smime_encryption_tab[] = {
  /* S/MIME signed part */
  {
    .name = "signed",
    .description = "S/MIME signed part",
    .encrypt = smime_sign,
  },
  
  /* S/MIME encrypted part */
  
  {
    .name = "encrypted",
    .description = "S/MIME encrypted part",
    .encrypt = smime_encrypt,
  },

  /* S/MIME signed & encrypted part */
  
  {
    .name = "signed-encrypted",
    .description = "S/MIME signed & encrypted part",
    .encrypt = smime_sign_encrypt,
  },
};
#endif

#if ! defined __STDC_VERSION__ || __STDC_VERSION__ < 199901L
static struct mailprivacy_protocol smime_protocol = {
  /* name */ "smime",
  /* description */ "S/MIME",
  
  /* is_encrypted */ smime_test_encrypted,
  /* decrypt */ smime_handler,
  
  /* encryption_count */
  (sizeof(smime_encryption_tab) / sizeof(smime_encryption_tab[0])),
  
  /* encryption_tab */ smime_encryption_tab,
};
#else
static struct mailprivacy_protocol smime_protocol = {
  .name = "smime",
  .description = "S/MIME",
  
  .is_encrypted = smime_test_encrypted,
  .decrypt = smime_handler,
  
  .encryption_count =
  (sizeof(smime_encryption_tab) / sizeof(smime_encryption_tab[0])),
  
  .encryption_tab = smime_encryption_tab,
};
#endif

int mailprivacy_smime_init(struct mailprivacy * privacy)
{
  certificates = chash_new(CHASH_DEFAULTSIZE, CHASH_COPYALL);
  if (certificates == NULL)
    goto err;

  private_keys = chash_new(CHASH_DEFAULTSIZE, CHASH_COPYALL);
  if (private_keys == NULL)
    goto free_cert;
  
  CAcert_dir[0] = '\0';
  
  return mailprivacy_register(privacy, &smime_protocol);
  
 free_cert:
  chash_free(certificates);
 err:
  return MAIL_ERROR_MEMORY;
}

void mailprivacy_smime_done(struct mailprivacy * privacy)
{
  mailprivacy_unregister(privacy, &smime_protocol);
  chash_free(private_keys);
  private_keys = NULL;
  chash_free(certificates);
  certificates = NULL;
  if (CAfile != NULL) {
    unlink(CAfile);
    free(CAfile);
  }
  CAfile = NULL;
  CAcert_dir[0] = '\0';
}


static void strip_string(char * str)
{
  char * p;
  size_t len;

  p = strchr(str, '\r');
  if (p != NULL)
    * p = 0;

  p = strchr(str, '\n');
  if (p != NULL)
    * p = 0;

  p = str;
  while ((* p == ' ') || (* p == '\t')) {
    p ++;
  }
  
  len = strlen(p);
  memmove(str, p, len);
  str[len] = 0;
  
  if (len == 0)
    return;
  
  p = str;
  len = len - 1;
  while ((p[len] == ' ') || (p[len] == '\t')) {
    p[len] = '\0';
    
    if (len == 0)
      break;
    
    len --;
  }
}



#define MAX_EMAIL_SIZE 1024

static void set_file(chash * hash, char * email, char * filename)
{
  char * n;
  char buf[MAX_EMAIL_SIZE];
  chashdatum key;
  chashdatum data;

  strncpy(buf, email, sizeof(buf));
  buf[sizeof(buf) - 1] = '\0';
  for(n = buf ; * n != '\0' ; n ++)
    * n = toupper((unsigned char) * n);
  strip_string(buf);
  
  key.data = buf;
  key.len = strlen(buf);
  data.data = filename;
  data.len = strlen(filename) + 1;
  
  chash_set(hash, &key, &data, NULL);
}

static char * get_file(chash * hash, char * email)
{
  chashdatum key;
  chashdatum data;
  char buf[MAX_EMAIL_SIZE];
  char * n;
  int r;
  
  strncpy(buf, email, sizeof(buf));
  buf[sizeof(buf) - 1] = '\0';
  for(n = buf ; * n != '\0' ; n ++)
    * n = toupper((unsigned char) * n);
  
  strip_string(buf);
  key.data = buf;
  key.len = strlen(buf);
  r = chash_get(hash, &key, &data);
  if (r < 0)
    return NULL;
  
  return data.data;
}

#define CERTIFICATE_SUFFIX "-cert.pem"

void mailprivacy_smime_set_cert_dir(struct mailprivacy * privacy,
    char * directory)
{
  DIR * dir;
  struct dirent * ent;

  chash_clear(certificates);
  
  if (directory == NULL)
    return;
  
  if (* directory == '\0')
    return;

  strncpy(cert_dir, directory, sizeof(cert_dir));
  cert_dir[sizeof(cert_dir) - 1] = '\0';
  
  dir = opendir(directory);
  if (dir == NULL)
    return;
  
  while ((ent = readdir(dir)) != NULL) {
#if 0
    char quoted_filename[PATH_MAX];
    char filename[PATH_MAX];
    char command[PATH_MAX];
    char buf[MAX_EMAIL_SIZE];
    FILE * p;
    int r;
    
    snprintf(filename, sizeof(filename),
        "%s/%s", directory, ent->d_name);
    
    r = mail_quote_filename(quoted_filename, sizeof(quoted_filename), filename);
    
    snprintf(command, sizeof(command),
        "openssl x509 -email -noout -in '%s' 2>/dev/null", quoted_filename);
    
    p = popen(command, "r");
    if (p == NULL)
      continue;
    
    while (fgets(buf, sizeof(buf), p) != NULL)
      set_file(certificates, buf, filename);
    
    pclose(p);
#endif
    char filename[PATH_MAX];
    char email[PATH_MAX];
    char * p;
    
    snprintf(filename, sizeof(filename),
        "%s/%s", directory, ent->d_name);
    
    strncpy(email, ent->d_name, sizeof(email));
    email[sizeof(email) - 1] = '\0';
    
    p = strstr(email, CERTIFICATE_SUFFIX);
    if (p == NULL)
      continue;
    
    if (strlen(p) != sizeof(CERTIFICATE_SUFFIX) - 1)
      continue;
    
    * p = 0;
    
    if (* email == '\0')
      continue;
    
    set_file(certificates, email, filename);
  }
  closedir(dir);
}

static char * get_cert_file(char * email)
{
  return get_file(certificates, email);
}

static char * get_private_key_file(char * email)
{
  return get_file(private_keys, email);
}

void mail_private_smime_clear_private_keys(struct mailprivacy * privacy)
{
  chash_clear(private_keys);
}

#define MAX_BUF 1024

void mailprivacy_smime_set_CA_dir(struct mailprivacy * privacy,
    char * directory)
{
  DIR * dir;
  struct dirent * ent;
  FILE * f_CA;
  char CA_filename[PATH_MAX];
  
  if (directory == NULL)
    return;
  
  if (* directory == '\0')
    return;
  
  /* make a temporary file that contains all the CAs */
  
  if (CAfile != NULL) {
    unlink(CAfile);
    free(CAfile);
    CAfile = NULL;
  }
  
  f_CA = mailprivacy_get_tmp_file(privacy, CA_filename, sizeof(CA_filename));
  if (f_CA == NULL)
    return;
  
  strncpy(CAcert_dir, directory, sizeof(CAcert_dir));
  CAcert_dir[sizeof(CAcert_dir) - 1] = '\0';
  
  dir = opendir(directory);
  if (dir == NULL) {
    fclose(f_CA);
    goto unlink_CA;
  }
  
  while ((ent = readdir(dir)) != NULL) {
    char filename[PATH_MAX];
    char buf[MAX_BUF];
    FILE * f;
    
    snprintf(filename, sizeof(filename),
        "%s/%s", directory, ent->d_name);
    
    f = fopen(filename, "r");
    if (f == NULL)
      continue;
    
    while (fgets(buf, sizeof(buf), f) != NULL)
      fputs(buf, f_CA);
    
    fclose(f);
  }
  
  closedir(dir);
  
  fclose(f_CA);
  
  CAfile = strdup(CA_filename);
  if (CAfile == NULL)
    goto unlink_CA;
  
  return;
  
 unlink_CA:
  unlink(CA_filename);
}

void mailprivacy_smime_set_CA_check(struct mailprivacy * privacy,
    int enabled)
{
  CA_check = enabled;
}

void mailprivacy_smime_set_store_cert(struct mailprivacy * privacy,
    int enabled)
{
  store_cert = enabled;
}

static int get_cert_from_sig(struct mailprivacy * privacy,
    mailmessage * msg,
    struct mailmime * mime)
{
  clistiter * cur;
  struct mailmime * signed_mime;
  struct mailmime * signature_mime;
  int res;
  char signature_filename[PATH_MAX];
  char quoted_signature_filename[PATH_MAX];
  char * email;
  char * cert_file;
  char store_cert_filename[PATH_MAX];
  char quoted_store_cert_filename[PATH_MAX];
  int r;
  char command[PATH_MAX];

  if (* cert_dir == '\0')
    return MAIL_ERROR_INVAL;

  if (mime->mm_type != MAILMIME_MULTIPLE)
    return MAIL_ERROR_INVAL;

  email = get_first_from_addr(mime);
  if (email == NULL)
    return MAIL_ERROR_INVAL;
  
  cert_file = get_cert_file(email);
  if (cert_file != NULL)
    return MAIL_NO_ERROR;

  /* get the two parts of the S/MIME message */
  
  cur = clist_begin(mime->mm_data.mm_multipart.mm_mp_list);
  if (cur == NULL) {
    res = MAIL_ERROR_INVAL;
    goto err;
  }
  
  signed_mime = cur->data;
  cur = clist_next(cur);
  if (cur == NULL) {
    res = MAIL_ERROR_INVAL;
    goto err;
  }

  signature_mime = cur->data;
  
  r = mailprivacy_fetch_decoded_to_file(privacy,
      signature_filename, sizeof(signature_filename),
      msg, signature_mime);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }
  
  r = mail_quote_filename(quoted_signature_filename,
       sizeof(quoted_signature_filename), signature_filename);
  if (r < 0) {
    res = MAIL_ERROR_MEMORY;
    goto unlink_signature;
  }
  
  snprintf(store_cert_filename, sizeof(store_cert_filename),
      "%s/%s" CERTIFICATE_SUFFIX, cert_dir, email);
  
  r = mail_quote_filename(quoted_store_cert_filename,
       sizeof(quoted_store_cert_filename), store_cert_filename);
  if (r < 0) {
    res = MAIL_ERROR_MEMORY;
    goto unlink_signature;
  }
  
  snprintf(command, sizeof(command),
      "openssl pkcs7 -inform DER -in '%s' -out '%s' -print_certs 2>/dev/null",
      quoted_signature_filename, quoted_store_cert_filename);
  
  r = system(command);
  if (WEXITSTATUS(r) != 0) {
    res = MAIL_ERROR_COMMAND;
    goto unlink_signature;
  }

  unlink(signature_filename);
  
  set_file(certificates, email, store_cert_filename);
  
  return MAIL_NO_ERROR;
  
 unlink_signature:
  unlink(signature_filename);
 err:
  return res;
}


static void set_private_key(struct mailprivacy * privacy,
    char * email, char * file)
{
  set_file(private_keys, email, file);
}

#define PRIVATE_KEY_SUFFIX "-private-key.pem"

void mailprivacy_smime_set_private_keys_dir(struct mailprivacy * privacy,
    char * directory)
{
  DIR * dir;
  struct dirent * ent;

  chash_clear(private_keys);
  
  if (directory == NULL)
    return;
  
  if (* directory == '\0')
    return;

  strncpy(private_keys_dir, directory, sizeof(private_keys_dir));
  private_keys_dir[sizeof(private_keys_dir) - 1] = '\0';
  
  dir = opendir(directory);
  if (dir == NULL)
    return;

  while ((ent = readdir(dir)) != NULL) {
    char filename[PATH_MAX];
    char email[PATH_MAX];
    char * p;
    
    snprintf(filename, sizeof(filename),
        "%s/%s", directory, ent->d_name);
    
    strncpy(email, ent->d_name, sizeof(email));
    email[sizeof(email) - 1] = '\0';
    
    p = strstr(email, PRIVATE_KEY_SUFFIX);
    if (p == NULL)
      continue;
    
    if (strlen(p) != sizeof(PRIVATE_KEY_SUFFIX) - 1)
      continue;
    
    * p = 0;
    
    if (* email == '\0')
      continue;
    
    set_private_key(privacy, email, filename);
  }
  closedir(dir);
}

/*
  - try private keys without passphrase and try those
      for which we already have a passphrase,
  - try recipient list and ask passphrase,
  - then, ask passphrase for all private keys
*/

static int smime_command_passphrase(struct mailprivacy * privacy,
    struct mailmessage * msg,
    char * command,
    char * userid,
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
    res = ERROR_SMIME_FILE;
    goto err;
  }
  
  fd_err = open(stderrfile, O_RDWR | O_CREAT | O_TRUNC, 0600);
  if (fd_err < 0) {
    res = ERROR_SMIME_FILE;
    goto close_out;
  }
  
  r = pipe(passphrase_input);
  if (r < 0) {
    res = ERROR_SMIME_FILE;
    goto close_err;
  }
  
  pid = fork();
  switch (pid) {
  case -1:
    return ERROR_SMIME_COMMAND;
    
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
      
      if ((passphrase != NULL) && (strlen(passphrase) > 0)) {
        write(passphrase_input[1], passphrase, strlen(passphrase));
      }
      else {
        /* dummy password */
        write(passphrase_input[1], "*dummy*", 7);
      }
      close(passphrase_input[1]);
      
      waitpid(pid, &status, 0);
      
      if (WEXITSTATUS(status) != 0)
        bad_passphrase = 1;
    }
    break;
  }
  
  if (bad_passphrase) {
    if (userid != NULL) {
      mailprivacy_smime_add_encryption_id(privacy, msg, userid);
      return ERROR_SMIME_NOPASSPHRASE;
    }
    
    return ERROR_SMIME_CHECK;
  }
  
  return NO_ERROR_SMIME;
  
 close_err:
  close(fd_err);
 close_out:
  close(fd_out);
 err:
  return res;
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

void mailprivacy_smime_encryption_id_list_clear(struct mailprivacy * privacy,
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

clist * mailprivacy_smime_encryption_id_list(struct mailprivacy * privacy,
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

static int mailprivacy_smime_add_encryption_id(struct mailprivacy * privacy,
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

static chash * passphrase_hash = NULL;

int mailprivacy_smime_set_encryption_id(struct mailprivacy * privacy,
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
