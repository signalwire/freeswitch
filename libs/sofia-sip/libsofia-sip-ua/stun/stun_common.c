/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2005 Nokia Corporation.
 *
 * Contact: Pekka Pessi <pekka.pessi@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

/**@internal
 * @file stun_common.c
 * @brief
 *
 * @author Tat Chan <Tat.Chan@nokia.com>
 * @author Kai Vehmanen <kai.vehmanen@nokia.com>
 *
 * @date Created: Fri Oct  3 13:40:41 2003 ppessi
 *
 */

#include "config.h"

#ifdef USE_TURN
#include "../turn/turn_common.h"
#undef STUN_A_LAST_MANDATORY
#define STUN_A_LAST_MANDATORY TURN_LARGEST_ATTRIBUTE
#endif

#include "stun_internal.h"

#include <string.h>
#include <stdlib.h>
#include <assert.h>

#if HAVE_FUNC
#elif HAVE_FUNCTION
#define __func__ __FUNCTION__
#else
#define __func__ "stun_common"
#endif

const char stun_400_Bad_request[] = "Bad Request",
  stun_401_Unauthorized[] = "Unauthorized",
  stun_420_Unknown_attribute[] = "Unknown Attribute",
  stun_430_Stale_credentials[] = "Stale Credentials",
  stun_431_Integrity_check_failure[] = "Integrity Check Failure",
  stun_432_Missing_username[] = "Missing Username",
  stun_433_Use_tls[] = "Use TLS",
#ifdef USE_TURN
  turn_434_Missing_realm[] = "Missing Realm",
  turn_435_Missing_nonce[] = "Missing Nonce",
  turn_436_Unknown_username[] = "Unknown Username",
  turn_437_No_binding[] = "No Binding",
  turn_439_Illegal_port[] = "Illegal Port",
#endif
  stun_500_Server_error[] = "Server Error",
  stun_600_Global_failure[] = "Global Failure";

#define set16(b, offset, value)			\
  (((b)[(offset) + 0] = ((value) >> 8) & 255),	\
   ((b)[(offset) + 1] = (value) & 255))

#define get16(b, offset)	\
  (((b)[(offset) + 0] << 8) |	\
   ((b)[(offset) + 1] << 0))

int stun_parse_message(stun_msg_t *msg)
{
  unsigned len;
  int i;
  unsigned char *p;

  /* parse header first */
  p = msg->enc_buf.data;
  msg->stun_hdr.msg_type = get16(p, 0);
  msg->stun_hdr.msg_len = get16(p, 2);
  memcpy(msg->stun_hdr.tran_id, p + 4, STUN_TID_BYTES);

  SU_DEBUG_5(("%s: Parse STUN message: Length = %d\n", __func__,
	      msg->stun_hdr.msg_len));

  /* parse attributes */
  len = msg->stun_hdr.msg_len;
  p = msg->enc_buf.data + 20;
  msg->stun_attr = NULL;
  while (len > 0) {
    i = stun_parse_attribute(msg, p);
    if (i <= 0 || i > len) {
      SU_DEBUG_3(("%s: Error parsing attribute.\n", __func__));
      return -1;
    }
    p += i;
    len -= i;
  }

  return 0;
}

int stun_parse_attribute(stun_msg_t *msg, unsigned char *p)
{
  int len;
  uint16_t attr_type;
  stun_attr_t *attr, *next;

  attr_type = get16(p, 0);
  len = get16(p, 2);

  SU_DEBUG_5(("%s: received attribute: Type %02X, Length %d - %s\n",
	      __func__, attr_type, len, stun_attr_phrase(attr_type)));

  if (attr_type > STUN_A_LAST_MANDATORY && attr_type < STUN_A_OPTIONAL) {
    return -1;
  }

  attr = (stun_attr_t *)calloc(1, sizeof(stun_attr_t));
  if (!attr)
    return -1;
  attr->next = NULL;
  attr->attr_type = attr_type;
  p += 4;

  switch (attr->attr_type) {
  case MAPPED_ADDRESS:
  case RESPONSE_ADDRESS:
  case SOURCE_ADDRESS:
  case CHANGED_ADDRESS:
  case REFLECTED_FROM:
#ifdef USE_TURN
  case TURN_ALTERNATE_SERVER:
  case TURN_DESTINATION_ADDRESS:
  case TURN_SOURCE_ADDRESS:
#endif
    if (stun_parse_attr_address(attr, p, len) < 0) {
      free(attr);
      return -1;
    }
    break;
  case ERROR_CODE:
    if (stun_parse_attr_error_code(attr, p, len) <0) { free(attr); return -1; }
    break;
  case UNKNOWN_ATTRIBUTES:
    if(stun_parse_attr_unknown_attributes(attr, p, len) <0) { free(attr); return -1; }
    break;
  case CHANGE_REQUEST:
#ifdef USE_TURN
  case TURN_LIFETIME:
  case TURN_MAGIC_COOKIE:
  case TURN_BANDWIDTH:
#endif
    if (stun_parse_attr_uint32(attr, p, len) <0) { free(attr); return -1; }
    break;
  case USERNAME:
  case PASSWORD:
  case STUN_A_REALM:
  case STUN_A_NONCE:
#ifdef USE_TURN
  case TURN_DATA:
  case TURN_NONCE:
#endif
    if (stun_parse_attr_buffer(attr, p, len) <0) { free(attr); return -1; }
    break;
  default:
    /* just copy as is */
    attr->pattr = NULL;
    attr->enc_buf.size = len;
    attr->enc_buf.data = (unsigned char *) malloc(len);
    memcpy(attr->enc_buf.data, p, len);
    break;
  }

  /* skip to end of list */
  if(msg->stun_attr==NULL) {
    msg->stun_attr = attr;
  }
  else {
    next = msg->stun_attr;
    while(next->next!=NULL) {
      next = next->next;
    }
    next->next = attr;
  }
  return len+4;
}

int stun_parse_attr_address(stun_attr_t *attr,
			    const unsigned char *p,
			    unsigned len)
{
  su_sockaddr_t *addr;
  int addrlen;
  char ipaddr[SU_ADDRSIZE + 2];

  if (len != 8) {
    return -1;
  }

  addrlen = sizeof(su_sockaddr_t);
  addr = (su_sockaddr_t *) malloc(addrlen);

  if (*(p+1) == 1) { /* expected value for IPv4 */
    addr->su_sin.sin_family = AF_INET;
  }
  else {
    free(addr);
    return -1;
  }
  memcpy(&addr->su_sin.sin_port, p + 2, 2);
  memcpy(&addr->su_sin.sin_addr.s_addr, p + 4, 4);

  SU_DEBUG_5(("%s: address attribute: %s:%d\n", __func__,
	      su_inet_ntop(addr->su_family, SU_ADDR(addr), ipaddr, sizeof(ipaddr)),
	      (unsigned) ntohs(addr->su_sin.sin_port)));

  attr->pattr = addr;
  stun_init_buffer(&attr->enc_buf);

  return 0;
}

int stun_parse_attr_error_code(stun_attr_t *attr, const unsigned char *p, unsigned len) {

  uint32_t tmp;
  stun_attr_errorcode_t *error;

  memcpy(&tmp, p, sizeof(uint32_t));
  tmp = ntohl(tmp);
  error = (stun_attr_errorcode_t *) malloc(sizeof(*error));

  error->code = (tmp & STUN_EC_CLASS)*100 + (tmp & STUN_EC_NUM);

  error->phrase = (char *) malloc(len-3);

  strncpy(error->phrase, (char*)p+4, len-4);
  error->phrase[len - 4] = '\0';

  attr->pattr = error;
  stun_init_buffer(&attr->enc_buf);

  return 0;
}

int stun_parse_attr_uint32(stun_attr_t *attr, const unsigned char *p, unsigned len)
{
  uint32_t tmp;
  stun_attr_changerequest_t *cr;
  cr = (stun_attr_changerequest_t *) malloc(sizeof(*cr));
  memcpy(&tmp, p, sizeof(uint32_t));
  cr->value = ntohl(tmp);
  attr->pattr = cr;
  stun_init_buffer(&attr->enc_buf);

  return 0;
}

int stun_parse_attr_buffer(stun_attr_t *attr, const unsigned char *p, unsigned len)
{
  stun_buffer_t *buf;
  buf = (stun_buffer_t *) malloc(sizeof(stun_buffer_t));
  buf->size = len;
  buf->data = (unsigned char *) malloc(len);
  memcpy(buf->data, p, len);
  attr->pattr = buf;
  stun_init_buffer(&attr->enc_buf);

  return 0;
}

int stun_parse_attr_unknown_attributes(stun_attr_t *attr,
				       const unsigned char *p,
				       unsigned len)
{
  return 0;
}

/** scan thru attribute list and return the next requested attr */
stun_attr_t *stun_get_attr(stun_attr_t *attr, uint16_t attr_type) {
  stun_attr_t *p;

  for (p = attr; p != NULL; p = p->next) {
    if (p->attr_type == attr_type)
      break;
  }

  return p;
}

void stun_init_buffer(stun_buffer_t *p) {
  p->data = NULL;
  p->size = 0;
}

int stun_free_buffer(stun_buffer_t *p) {
  if (p->data)
    free(p->data), p->data = NULL;
  p->size = 0;
  return 0;
}

int stun_copy_buffer(stun_buffer_t *p, stun_buffer_t *p2) {
  stun_free_buffer(p); /* clean up existing data */
  p->size = p2->size;
  p->data = (unsigned char *) malloc(p->size);
  memcpy(p->data, p2->data, p->size);
  return p->size;
}

const char *stun_response_phrase(int status) {
  if (status <100 || status >600)
    return NULL;

  switch (status) {
  case STUN_400_BAD_REQUEST: return stun_400_Bad_request;
  case STUN_401_UNAUTHORIZED: return stun_401_Unauthorized;
  case STUN_420_UNKNOWN_ATTRIBUTE: return stun_420_Unknown_attribute;
  case STUN_430_STALE_CREDENTIALS: return stun_430_Stale_credentials;
  case STUN_431_INTEGRITY_CHECK_FAILURE: return stun_431_Integrity_check_failure;
  case STUN_432_MISSING_USERNAME: return stun_432_Missing_username;
  case STUN_433_USE_TLS: return stun_433_Use_tls;
#ifdef USE_TURN
  case TURN_MISSING_REALM: return turn_434_Missing_realm;
  case TURN_MISSING_NONCE: return turn_435_Missing_nonce;
  case TURN_UNKNOWN_USERNAME: return turn_436_Unknown_username;
  case TURN_NO_BINDING: return turn_437_No_binding;
  case TURN_ILLEGAL_PORT: return turn_439_Illegal_port;
#endif
  case STUN_500_SERVER_ERROR: return stun_500_Server_error;
  case STUN_600_GLOBAL_FAILURE: return stun_600_Global_failure;
  }
  return "Response";
}

/** The set of functions encodes the corresponding attribute to
 *    network format, and save the result to the enc_buf. Return the
 *    size of the buffer.
 */


/* This function is used to encode any attribute of the form ADDRESS
   */
int stun_encode_address(stun_attr_t *attr) {
  stun_attr_sockaddr_t *a;
  uint16_t tmp;

  a = (stun_attr_sockaddr_t *)attr->pattr;

  if (stun_encode_type_len(attr, 8) < 0) {
    return -1;
  }

  tmp = htons(0x01); /* FAMILY = 0x01 */
  memcpy(attr->enc_buf.data+4, &tmp, sizeof(tmp));
  memcpy(attr->enc_buf.data+6, &a->sin_port, 2);
  memcpy(attr->enc_buf.data+8, &a->sin_addr.s_addr, 4);

  return attr->enc_buf.size;
}

int stun_encode_uint32(stun_attr_t *attr) {
  uint32_t tmp;

  if (stun_encode_type_len(attr, 4) < 0) {
    return -1;
  }

  tmp = htonl(((stun_attr_changerequest_t *) attr->pattr)->value);
  memcpy(attr->enc_buf.data+4, &tmp, 4);
  return attr->enc_buf.size;
}

int stun_encode_error_code(stun_attr_t *attr) {
  short int class, num;
  size_t phrase_len, padded;
  stun_attr_errorcode_t *error;

  error = (stun_attr_errorcode_t *) attr->pattr;
  class = error->code / 100;
  num = error->code % 100;

  phrase_len = strlen(error->phrase);
  if (phrase_len + 8 > 65536)
    phrase_len = 65536 - 8;

  /* note: align the phrase len (see RFC3489:11.2.9) */
  padded = phrase_len + (phrase_len % 4 == 0 ? 0 : 4 - (phrase_len % 4));

  /* note: error-code has four octets of headers plus the
   *       reason field -> len+4 octets */
  if (stun_encode_type_len(attr, (uint16_t)(padded + 4)) < 0) {
    return -1;
  }
  else {
    assert(attr->enc_buf.size == padded + 8);
    memset(attr->enc_buf.data+4, 0, 2);
    attr->enc_buf.data[6] = class;
    attr->enc_buf.data[7] = num;
    /* note: 4 octets of TLV header and 4 octets of error-code header */
    memcpy(attr->enc_buf.data+8, error->phrase,
	   phrase_len);
    memset(attr->enc_buf.data + 8 + phrase_len, 0, padded - phrase_len);
  }

  return attr->enc_buf.size;
}

int stun_encode_buffer(stun_attr_t *attr) {
  stun_buffer_t *a;

  a = (stun_buffer_t *)attr->pattr;
  assert(a->size < 65536);
  if (stun_encode_type_len(attr, (uint16_t)a->size) < 0) {
    return -1;
  }

  memcpy(attr->enc_buf.data+4, a->data, a->size);
  return attr->enc_buf.size;
}

#if defined(HAVE_OPENSSL)
int stun_encode_message_integrity(stun_attr_t *attr,
				  unsigned char *buf,
				  int len,
				  stun_buffer_t *pwd) {
  int padded_len;
  unsigned int dig_len;
  unsigned char *padded_text = NULL;
  void *sha1_hmac;

  if (stun_encode_type_len(attr, 20) < 0) {
    return -1;
  }

  /* zero padding */
  if (len % 64 != 0) {

    padded_len = len + (64 - (len % 64));
    padded_text = (unsigned char *) malloc(padded_len);
    memcpy(padded_text, buf, len);
    memset(padded_text + len, 0, padded_len - len);

    sha1_hmac = HMAC(EVP_sha1(), pwd->data, pwd->size, padded_text, padded_len, NULL, &dig_len);
  }
  else {
    sha1_hmac = HMAC(EVP_sha1(), pwd->data, pwd->size, buf, len, NULL, &dig_len);
  }

  assert(dig_len == 20);

  memcpy(attr->enc_buf.data + 4, sha1_hmac, 20);
  free(padded_text);
  return attr->enc_buf.size;
}
#else
int stun_encode_message_integrity(stun_attr_t *attr,
				  unsigned char *buf,
				  int len,
				  stun_buffer_t *pwd) {

  return 0;
}
#endif /* HAVE_OPENSSL */

/** this function allocates the enc_buf, fills in type, length */
int stun_encode_type_len(stun_attr_t *attr, uint16_t len) {
  uint16_t tmp;

  attr->enc_buf.data = (unsigned char *) malloc(len + 4);
  memset(attr->enc_buf.data, 0, len + 4);

  tmp = htons(attr->attr_type);
  memcpy(attr->enc_buf.data, &tmp, 2);

  tmp = htons(len);
  memcpy(attr->enc_buf.data + 2, &tmp, 2);
  attr->enc_buf.size = len + 4;

  return 0;
}

/**
 * Validate the message integrity based on given
 * STUN password 'pwd'. The received content should be
 * in msg->enc_buf.
 */
int stun_validate_message_integrity(stun_msg_t *msg, stun_buffer_t *pwd)
{

#if defined(HAVE_OPENSSL)
  int padded_len, len;
  unsigned int dig_len;
  unsigned char dig[20]; /* received sha1 digest */
  unsigned char *padded_text;
#endif

  /* password NULL so shared-secret not established and
     messege integrity checks can be skipped */
  if (pwd->data == NULL)
    return 0;

  /* otherwise the check must match */

#if defined(HAVE_OPENSSL)

  /* message integrity not received */
  if (stun_get_attr(msg->stun_attr, MESSAGE_INTEGRITY) == NULL) {
    SU_DEBUG_5(("%s: error: message integrity missing.\n", __func__));
    return -1;
  }

  /* zero padding */
  len = msg->enc_buf.size - 24;
  padded_len = len + (len % 64 == 0 ? 0 : 64 - (len % 64));
  padded_text = (unsigned char *) malloc(padded_len);
  memset(padded_text, 0, padded_len);
  memcpy(padded_text, msg->enc_buf.data, len);

  memcpy(dig, HMAC(EVP_sha1(), pwd->data, pwd->size, padded_text, padded_len, NULL, &dig_len), 20);

  if (memcmp(dig, msg->enc_buf.data + msg->enc_buf.size - 20, 20) != 0) {
    /* does not match, but try the test server's password */
    if (memcmp(msg->enc_buf.data+msg->enc_buf.size-20, "hmac-not-implemented", 20) != 0) {
      SU_DEBUG_5(("%s: error: message digest problem.\n", __func__));
      return -1;
    }
  }
  else {
    SU_DEBUG_5(("%s: message integrity validated.\n", __func__));
  }

  free(padded_text);

  return 0;
#else /* HAVE_OPENSSL */
  return -1;
#endif
}

void debug_print(stun_buffer_t *buf) {
  unsigned i;
  for(i = 0; i < buf->size/4; i++) {
    SU_DEBUG_9(("%02x %02x %02x %02x\n",
		*(buf->data + i*4),
		*(buf->data + i*4 +1),
		*(buf->data + i*4 +2),
		*(buf->data + i*4 +3)));
    if (i == 4)
		SU_DEBUG_9(("---------------------\n" VA_NONE));
  }
  SU_DEBUG_9(("\n" VA_NONE));
}

int stun_init_message(stun_msg_t *msg) {
  msg->stun_hdr.msg_type = 0;
  msg->stun_hdr.msg_len = 0;
  msg->stun_attr = NULL;
  stun_init_buffer(&msg->enc_buf);
  return 0;
}

int stun_free_message(stun_msg_t *msg) {

  stun_attr_t *p, *p2;

  /* clearing header */
  memset(&msg->stun_hdr, 0, sizeof msg->stun_hdr);

  /* clearing attr */
  p = msg->stun_attr;
  while(p) {
    if(p->pattr) {
      switch(p->attr_type) {
      case USERNAME:
      case PASSWORD:
#ifdef USE_TURN
      case TURN_DATA:
      case TURN_NONCE:
#endif
	stun_free_buffer(p->pattr);
	break;
      default:
	free(p->pattr);
      }
    }
    stun_free_buffer(&p->enc_buf);
    p2 = p->next;
    free(p);
    p = p2;
  }
  msg->stun_attr = NULL;

  /* clearing buffer */
  stun_free_buffer(&msg->enc_buf);

  return 0;
}


int stun_send_message(su_socket_t s, su_sockaddr_t *to_addr,
		      stun_msg_t *msg, stun_buffer_t *pwd)
{
  int err;
  char ipaddr[SU_ADDRSIZE + 2];
  stun_attr_t **a, *b;

  stun_encode_message(msg, pwd);

  err = su_sendto(s, msg->enc_buf.data, msg->enc_buf.size, 0,
		  to_addr, SU_SOCKADDR_SIZE(to_addr));

  free(msg->enc_buf.data), msg->enc_buf.data = NULL;
  msg->enc_buf.size = 0;

  for (a = &msg->stun_attr; *a;) {

    if ((*a)->pattr)
      free((*a)->pattr), (*a)->pattr = NULL;

    if ((*a)->enc_buf.data)
      free((*a)->enc_buf.data), (*a)->enc_buf.data = NULL;

    b = *a;
    b = b->next;
    free(*a);
    *a = NULL;
    *a = b;
  }

  if (err > 0) {
    su_inet_ntop(to_addr->su_family, SU_ADDR(to_addr), ipaddr, sizeof(ipaddr));
    SU_DEBUG_5(("%s: message sent to %s:%u\n", __func__,
		ipaddr, ntohs(to_addr->su_port)));

    debug_print(&msg->enc_buf);
  }
  else
    STUN_ERROR(errno, sendto);

  return err;
}


/** Send a STUN message.
 *  This will convert the stun_msg_t to the binary format based on the
 *  spec
 */
int stun_encode_message(stun_msg_t *msg, stun_buffer_t *pwd) {

  int z = -1, len, buf_len = 0;
  unsigned char *buf;
  stun_attr_t *attr, *msg_int=NULL;

  if (msg->enc_buf.data == NULL) {
    /* convert msg to binary format */
    /* convert attributes to binary format for transmission */
    len = 0;
    for (attr = msg->stun_attr; attr ; attr = attr->next) {
      switch(attr->attr_type) {
      case RESPONSE_ADDRESS:
      case MAPPED_ADDRESS:
      case SOURCE_ADDRESS:
      case CHANGED_ADDRESS:
      case REFLECTED_FROM:
#ifdef USE_TURN
      case TURN_ALTERNATE_SERVER:
      case TURN_DESTINATION_ADDRESS:
      case TURN_SOURCE_ADDRESS:
#endif
	z = stun_encode_address(attr);
	break;
      case CHANGE_REQUEST:
#ifdef USE_TURN
      case TURN_LIFETIME:
      case TURN_MAGIC_COOKIE:
      case TURN_BANDWIDTH:
#endif
	z = stun_encode_uint32(attr);
	break;

      case USERNAME:
      case PASSWORD:
#ifdef USE_TURN
      case TURN_REALM:
      case TURN_NONCE:
      case TURN_DATA:
#endif
	z = stun_encode_buffer(attr);
	break;
      case MESSAGE_INTEGRITY:
	msg_int = attr;
	z = 24;
	break;
      case ERROR_CODE:
	z = stun_encode_error_code(attr);
      default:
	break;
      }

      if(z < 0) return z;

      len += z;
    }

    msg->stun_hdr.msg_len = len;
    buf_len = 20 + msg->stun_hdr.msg_len;
    buf = (unsigned char *) malloc(buf_len);

    /* convert to binary format for transmission */
    set16(buf, 0, msg->stun_hdr.msg_type);
    set16(buf, 2, msg->stun_hdr.msg_len);
    memcpy(buf + 4, msg->stun_hdr.tran_id, STUN_TID_BYTES);

    len = 20;

    /* attaching encoded attributes */
    attr = msg->stun_attr;
    while(attr) {
      /* attach only if enc_buf is not null */
      if(attr->enc_buf.data && attr->attr_type != MESSAGE_INTEGRITY) {
	memcpy(buf+len, (void *)attr->enc_buf.data, attr->enc_buf.size);
	len += attr->enc_buf.size;
      }
      attr = attr->next;
    }

    if (msg_int) {
      /* compute message integrity */
      if(stun_encode_message_integrity(msg_int, buf, len, pwd)!=24) {
	free(buf);
	return -1;
      }
      memcpy(buf+len, (void *)msg_int->enc_buf.data,
	     msg_int->enc_buf.size);
    }

    /* save binary buffer for future reference */
    if (msg->enc_buf.data)
      free(msg->enc_buf.data);

    msg->enc_buf.data = buf; msg->enc_buf.size = buf_len;
  }

  return 0;
}

#include <sofia-sip/su.h>
#include <sofia-sip/su_debug.h>
#include <sofia-sip/su_localinfo.h>

char *stun_determine_ip_address(int family)
{

  char *local_ip_address;
  su_localinfo_t *li = NULL, hints[1] = {{ LI_CANONNAME|LI_NUMERIC }};
  int error;
  size_t address_size;
  struct sockaddr_in *sa = NULL;
  su_sockaddr_t *temp;

  hints->li_family = family;
  hints->li_canonname = getenv("HOSTADDRESS");
  if ((error = su_getlocalinfo(hints, &li)) < 0) {
    SU_DEBUG_5(("%s: stun_determine_ip_address, su_getlocalinfo: %s\n",
		__func__, su_gli_strerror(error)));
    return NULL;
  }

  temp = li->li_addr;
  sa = &temp->su_sin;

  address_size = strlen(inet_ntoa(sa->sin_addr));

  local_ip_address = malloc(address_size + 1);
  strcpy(local_ip_address, (char *) inet_ntoa(sa->sin_addr)); /* otherwise? */

  su_freelocalinfo(li);

  return local_ip_address;

}

const char *stun_attr_phrase(uint16_t type)
{
  switch(type) {
  case MAPPED_ADDRESS: return "MAPPED-ADDRESS";
  case RESPONSE_ADDRESS: return "RESPONSE-ADDRESS";
  case CHANGE_REQUEST: return "CHANGE-REQUEST";
  case SOURCE_ADDRESS: return "SOURCE-ADDRESS";
  case CHANGED_ADDRESS: return "CHANGED-ADDRESS";
  case USERNAME: return "USERNAME";
  case PASSWORD: return "PASSWORD";
  case MESSAGE_INTEGRITY: return "MESSAGE-INTEGRITY";
  case ERROR_CODE: return "ERROR-CODE";
  case UNKNOWN_ATTRIBUTES: return "UNKNOWN-ATTRIBUTES";
  case REFLECTED_FROM: return "REFLECTED-FROM";
  case STUN_A_ALTERNATE_SERVER:
  case STUN_A_ALTERNATE_SERVER_DEP:
    return "ALTERNATE-SERVER";
  case STUN_A_REALM: return "REALM";
  case STUN_A_NONCE: return "NONCE";
  case STUN_A_XOR_MAPPED_ADDRESS: return "XOR-MAPPED-ADDRESS";
#ifdef USE_TURN
  case TURN_REALM: return "REALM";
  case TURN_LIFETIME: return "LIFETIME";
  case TURN_ALTERNATE_SERVER: return "ALTERNATE_SERVER";
  case TURN_MAGIC_COOKIE: return "MAGIC_COOKIE";
  case TURN_BANDWIDTH: return "BANDWIDTH";
  case TURN_DESTINATION_ADDRESS: return "DESTINATION_ADDRESS";
  case TURN_SOURCE_ADDRESS: return "SOURCE_ADDRESS";
  case TURN_DATA: return "DATA";
  case TURN_NONCE: return "NONCE";
#endif
  default: return "Attribute undefined";
  }
}
