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

/**@CFILE sdp.c Simple SDP interface.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @author Kai Vehmanen <kai.vehmanen@nokia.com>
 *
 * @date Created: Fri Feb 18 10:25:08 2000 ppessi
 */

#include "config.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <assert.h>

#include <sofia-sip/su_alloc.h>
#include <sofia-sip/su_types.h>
#include <sofia-sip/su_string.h>

#include "sofia-sip/sdp.h"

struct align { void  *_a; char _b; };

#define ALIGN(v, n) ((n - (intptr_t)(v)) & (n - 1))
#define STRUCT_ALIGN_  (sizeof(struct align) - offsetof(struct align, _b))
#define STRUCT_ALIGN(v) ALIGN(v, sizeof(void *))
#define ASSERT_STRUCT_ALIGN(p) \
  (STRUCT_ALIGN(p) ? (void)assert(!"STRUCT_ALIGNED(" #p ")") : (void)0)

const unsigned sdp_struct_align_ = sizeof(void *) - STRUCT_ALIGN_;

#define STR_XTRA(rv, s)    ((s) ? rv += strlen((s)) + 1 : 0)
#define PTR_XTRA(rv, p, f) \
  ((p) ? (rv += STRUCT_ALIGN(rv) + f(p)) : 0)
#define LST_XTRA(rv, l, f) \
  ((l) ? (rv += STRUCT_ALIGN(rv) + list_xtra_all((xtra_f*)f, l)) : 0)


#define STRUCT_DUP(p, dst, src) \
  ASSERT_STRUCT_ALIGN(p); \
  ((*(int*)(src) >= (int)sizeof(*src)					\
    ? (dst = memcpy((p), (src), sizeof(*src)))				\
    : (dst = memcpy((p), (src), *(int*)(src))),				\
    memset((p)+*(int*)(src), 0, sizeof(*src) - *(int*)(src))), \
  ((p) += sizeof(*src)))

#define STRUCT_DUP2(p, dst, src) \
  ASSERT_STRUCT_ALIGN(p); assert(*(int*)(src) >= (int)sizeof(*src));	\
  (dst = memcpy((p), (src), *(int*)(src)), ((p) += *(int*)(src)))

#define STR_DUP(p, dst, src, m) \
 ((src->m) ? ((dst->m) = strcpy((p), (src->m)), (p) += strlen((p)) + 1) \
           : ((dst->m) = 0))
#define PTR_DUP(p, dst, src, m, dup) \
 ((dst->m) = (src->m)?((p += STRUCT_ALIGN(p)), ((dup)(&(p), (src->m)))): 0)
#define LST_DUP(p, dst, src, m, dup) \
 ((dst->m) = (src->m)?((p += STRUCT_ALIGN(p)),\
                       list_dup_all((dup_f*)(dup), &(p), src->m)) : 0)
#define MED_XTRA_EX(rv, l, c) \
  ((l) ? (rv += STRUCT_ALIGN(rv) + media_xtra_ex(l, c)) : 0)
#define MED_DUP_EX(p, dst, src, m, dst_c, src_c) \
 ((dst->m) = (src->m)?((p += STRUCT_ALIGN(p)),\
                       media_dup_all(&(p), src->m, dst, dst_c, src_c)) : 0)

#define MED_XTRA_ALL(rv, m) \
  ((m) ? (rv += STRUCT_ALIGN(rv) + media_xtra_all(m)) : 0)
#define MED_DUP_ALL(p, dst, src, m) \
 ((dst->m) = (src->m)?((p += STRUCT_ALIGN(p)),\
                       media_dup_all(&(p), src->m, dst)) : 0)

typedef size_t xtra_f(void const *);
typedef void *dup_f(char **bb, void const *src);

static size_t list_xtra_all(xtra_f *xtra, void const *v);
static void *list_dup_all(dup_f *dup, char **bb, void const *vsrc);

static size_t session_xtra(sdp_session_t const *o);
static sdp_session_t *session_dup(char **pp, sdp_session_t const *o);

static size_t origin_xtra(sdp_origin_t const *o);
static sdp_origin_t *origin_dup(char **pp, sdp_origin_t const *o);

static size_t connection_xtra(sdp_connection_t const *o);
static sdp_connection_t *connection_dup(char **pp, sdp_connection_t const *o);

static size_t bandwidth_xtra(sdp_bandwidth_t const *o);
static sdp_bandwidth_t *bandwidth_dup(char **pp, sdp_bandwidth_t const *o);

static size_t time_xtra(sdp_time_t const *o);
static sdp_time_t *time_dup(char **pp, sdp_time_t const *o);

static size_t repeat_xtra(sdp_repeat_t const *o);
static sdp_repeat_t *repeat_dup(char **pp, sdp_repeat_t const *o);

static size_t zone_xtra(sdp_zone_t const *o);
static sdp_zone_t *zone_dup(char **pp, sdp_zone_t const *o);

static size_t key_xtra(sdp_key_t const *o);
static sdp_key_t *key_dup(char **pp, sdp_key_t const *o);

static size_t attribute_xtra(sdp_attribute_t const *o);
static sdp_attribute_t *attribute_dup(char **pp, sdp_attribute_t const *o);

static size_t list_xtra(sdp_list_t const *o);
static sdp_list_t *list_dup(char **pp, sdp_list_t const *o);

static size_t rtpmap_xtra(sdp_rtpmap_t const *o);
static sdp_rtpmap_t *rtpmap_dup(char **pp, sdp_rtpmap_t const *o);

static size_t media_xtra(sdp_media_t const *o);
static sdp_media_t *media_dup(char **pp,
			      sdp_media_t const *o,
			      sdp_session_t *sdp);
#ifdef nomore
static size_t media_xtra_ex(sdp_media_t const *o,
			  sdp_connection_t const *c);
static sdp_media_t *media_dup_ex(char **pp,
				  sdp_media_t const *o,
				  sdp_session_t *sdp,
				  sdp_connection_t *dst_c,
				  sdp_connection_t const *src_c);
#endif
static size_t media_xtra_all(sdp_media_t const *o);
static sdp_media_t *media_dup_all(char **pp,
				  sdp_media_t const *o,
				  sdp_session_t *sdp);


/** Define a function body duplicating an SDP structure. */
#define SDP_DUP(type, name) \
  sdp_##type##_t *rv; size_t size; char *p, *end; \
  if (!name) return NULL; \
  size = type##_xtra(name); \
  p = su_alloc(h, size); end = p + size; \
  rv = type##_dup(&p, name); \
  assert(p == end); \
  return rv;

/** Define a function body duplicating a list of SDP structures. */
#define SDP_LIST_DUP(type, name) \
  sdp_##type##_t *rv; size_t size; char *p, *end; \
  if (!name) return NULL; \
  size = list_xtra_all((xtra_f*)type##_xtra, name); \
  rv = su_alloc(h, size); p = (char *)rv; end = p + size; \
  list_dup_all((dup_f*)type##_dup, &p, name); \
  assert(p == end); \
  return rv;

/**Duplicate an SDP origin description.
 *
 * The function sdp_origin_dup() duplicates (deeply copies) an SDP origin
 * description @a o allocating memory using memory @a home.
 *
 * @param h     Memory home
 * @param o     SDP origin description to be duplicated
 *
 * @note The duplicated structure is allocated using a single call to
 * su_alloc() and it can be freed with su_free().
 *
 * @return
 * If successful, a pointer to newly allocated sdp_origin_t structure is
 * returned, otherwise NULL is returned.
 */
sdp_origin_t *sdp_origin_dup(su_home_t *h, sdp_origin_t const *o)
{
  SDP_DUP(origin, o);
}

/**Duplicate an SDP connection description.
 *
 * The function sdp_connection_dup() duplicates (deeply copies) a list of
 * SDP connection description @a c allocating memory using memory @a home.
 *
 * @param h     Memory home
 * @param c     SDP connection description to be duplicated
 *
 * @note The duplicated list is allocated using a single call to
 * su_alloc() and it can be freed with su_free().
 *
 * @return
 * If successful, a pointer to newly allocated sdp_connection_t structure is
 * returned, otherwise NULL is returned.
 */
sdp_connection_t *sdp_connection_dup(su_home_t *h, sdp_connection_t const *c)
{
  SDP_LIST_DUP(connection, c);
}

/**Duplicate an SDP bandwidth description.
 *
 * The function sdp_bandwidth_dup() duplicates (deeply copies) a list of SDP
 * bandwidth descriptions @a b allocating memory using memory @a home.
 *
 * @param h     Memory home
 * @param b     SDP bandwidth description to be duplicated
 *
 * @note The duplicated list is allocated using a single call to
 * su_alloc() and it can be freed with su_free().
 *
 * @return
 * If successful, a pointer to newly allocated sdp_bandwidth_t structure is
 * returned, otherwise NULL is returned.
 */
sdp_bandwidth_t *sdp_bandwidth_dup(su_home_t *h, sdp_bandwidth_t const *b)
{
  SDP_LIST_DUP(bandwidth, b);
}

/**Duplicate an SDP time description.
 *
 * The function sdp_time_dup() duplicates (deeply copies) a list of SDP time
 * descriptions @a t allocating memory using memory @a home.
 *
 * @param h  Memory home
 * @param t  SDP time description to be duplicated
 *
 * @note The duplicated list is allocated using a single call to
 * su_alloc() and it can be freed with su_free().
 *
 * @return
 * If successful, a pointer to newly allocated sdp_time_t structure is
 * returned, otherwise NULL is returned.
 */
sdp_time_t *sdp_time_dup(su_home_t *h, sdp_time_t const *t)
{
  SDP_LIST_DUP(time, t);
}

/**Duplicate an SDP repeat description.
 *
 * The function sdp_repeat_dup() duplicates (deeply copies) an SDP repeat
 * description @a r allocating memory using memory @a home.
 *
 * @param h  Memory home
 * @param r  SDP repeat description to be duplicated
 *
 * @note The duplicated structure is allocated using a single call to
 * su_alloc() and it can be freed with su_free().
 *
 * @return
 * If successful, a pointer to newly allocated sdp_repeat_t structure is
 * returned, otherwise NULL is returned.
 */
sdp_repeat_t *sdp_repeat_dup(su_home_t *h, sdp_repeat_t const *r)
{
  SDP_DUP(repeat, r);
}

/**Duplicate an SDP zone description.
 *
 * The function sdp_zone_dup() duplicates (deeply copies) an SDP zone
 * description @a z allocating memory using memory @a home.
 *
 * @param h  Memory home
 * @param z  SDP zone description to be duplicated
 *
 * @note The duplicated structure is allocated using a single call to
 * su_alloc() and it can be freed with su_free().
 *
 * @return
 * If successful, a pointer to newly allocated sdp_zone_t structure is
 * returned, otherwise NULL is returned.
 */
sdp_zone_t *sdp_zone_dup(su_home_t *h, sdp_zone_t const *z)
{
  SDP_DUP(zone, z);
}

/**Duplicate an SDP key description.
 *
 * The function sdp_key_dup() duplicates (deeply copies) an SDP key
 * description @a k allocating memory using memory @a home.
 *
 * @param h  Memory home
 * @param k  SDP key description to be duplicated
 *
 * @note The duplicated structure is allocated using a single call to
 * su_alloc() and it can be freed with su_free().
 *
 * @return
 * If successful, a pointer to newly allocated sdp_key_t structure is
 * returned, otherwise NULL is returned.
 */
sdp_key_t *sdp_key_dup(su_home_t *h, sdp_key_t const *k)
{
  SDP_DUP(key, k);
}

/**Duplicate an SDP attribute list.
 *
 * The function sdp_attribute_dup() duplicates (deeply copies) an SDP
 * attribute list @a a allocating memory using memory @a home.
 *
 * @param h  Memory home
 * @param a  SDP attribute description to be duplicated
 *
 * @note The duplicated structure is allocated using a single call to
 * su_alloc() and it can be freed with su_free().
 *
 * @return
 * If successful, a pointer to newly allocated sdp_attribute_t structure is
 * returned, otherwise NULL is returned.
 */
sdp_attribute_t *sdp_attribute_dup(su_home_t *h, sdp_attribute_t const *a)
{
  SDP_LIST_DUP(attribute, a);
}

/**Duplicate an SDP list of text.
 *
 * The function sdp_list_dup() duplicates (deeply copies) an SDP text
 * list @a l allocating memory using memory @a home.
 *
 * @param h  Memory home
 * @param l  SDP list description to be duplicated
 *
 * @note The duplicated structure is allocated using a single call to
 * su_alloc() and it can be freed with su_free().
 *
 * @return
 * If successful, a pointer to newly allocated sdp_list_t structure is
 * returned, otherwise NULL is returned.
 */
sdp_list_t *sdp_list_dup(su_home_t *h, sdp_list_t const *l)
{
  SDP_LIST_DUP(list, l);
}

/**Duplicate an SDP rtpmap list.
 *
 * The function sdp_rtpmap_dup() duplicates (deeply copies) an SDP rtpmap
 * list @a rm allocating memory using memory @a home.
 *
 * @param h  Memory home
 * @param rm SDP rtpmap description to be duplicated
 *
 * @note The duplicated structure is allocated using a single call to
 * su_alloc() and it can be freed with su_free().
 *
 * @return
 * If successful, a pointer to newly allocated sdp_rtpmap_t structure is
 * returned, otherwise NULL is returned.
 */
sdp_rtpmap_t *sdp_rtpmap_dup(su_home_t *h, sdp_rtpmap_t const *rm)
{
  SDP_LIST_DUP(rtpmap, rm);
}

/**Duplicate an SDP media description.
 *
 * The function sdp_media_dup() duplicates (deeply copies) an SDP media
 * description @a m allocating memory using memory @a home.
 *
 * @param h   Memory home
 * @param m   SDP media description to be duplicated
 * @param sdp SDP session description to which the newly allocated
 *            media description is linked
 *
 * @note The duplicated structure is allocated using a single call to
 * su_alloc() and it can be freed with su_free().
 *
 * @return
 * If successful, a pointer to newly allocated sdp_media_t structure is
 * returned, otherwise NULL is returned.
 */
sdp_media_t *sdp_media_dup(su_home_t *h, sdp_media_t const *m,
			   sdp_session_t *sdp)
{
  sdp_media_t *rv; size_t size; char *p, *end;
  size = media_xtra(m);
  p = su_alloc(h, size); end = p + size;
  rv = media_dup(&p, m, sdp);
  assert(p == end);
  return rv;
}

/**Duplicate an SDP media description.
 *
 * The function sdp_media_dup_all() duplicates (deeply copies) a list of SDP
 * media descriptions @a m allocating memory using memory @a home.
 *
 * @param h     Memory home
 * @param m     list of SDP media descriptions to be duplicated
 * @param sdp   SDP session description to which the newly allocated
 *              media descriptions are linked
 *
 * @note The duplicated list is allocated using a single call to
 * su_alloc() and it can be freed with su_free().
 *
 * @return
 * If successful, a pointer to a newly allocated list of sdp_media_t
 * structures is returned, otherwise NULL is returned.
 */
sdp_media_t *sdp_media_dup_all(su_home_t *h, sdp_media_t const *m,
			       sdp_session_t *sdp)
{
  sdp_media_t *rv; size_t size; char *p, *end;
  size = media_xtra_all(m);
  p = su_alloc(h, size); end = p + size;
  rv = media_dup_all(&p, m, sdp);
  assert(p == end);
  return rv;
}

#ifdef nomore			/* really deprecated */
/**Duplicate media description with common address.
 *
 * This function is provided in order to avoid duplicate @c c= lines.  If
 * the @c c= line in media description equals to @a src_c, it is not
 * duplicated but replaced with @a dst_c instead.
 *
 * @param home  Memory home
 * @param src   SDP media description to be duplicated
 * @param sdp   SDP session description to which the newly allocated
 *              media description is linked
 * @param dst_c Connection description used instead of duplicate of @a src_c.
 * @param src_c Connection description not to be duplicated

 * @return
 * If successful, a pointer to newly allocated sdp_media_t structure is
 * returned, otherwise NULL is returned.
 *
 * @deprecated
 * This function is deprecated. Use sdp_media_dup() instead.
 */
sdp_media_t *sdp_media_dup_ex(su_home_t *home,
			      sdp_media_t const *src,
			      sdp_session_t *sdp,
			      sdp_connection_t *dst_c,
			      sdp_connection_t const *src_c)
{
  sdp_media_t *rv; size_t size; char *p, *end;
  size = media_xtra_all(src, src_c);
  p = su_alloc(home, size); end = p + size;
  rv = media_dup_all(&p, src, sdp, dst_c, src_c);
  assert(p == end);
  return rv;
}
#endif

/* ---------------------------------------------------------------------- */

static size_t origin_xtra(sdp_origin_t const *o)
{
  size_t rv = sizeof(*o);
  STR_XTRA(rv, o->o_username);
  PTR_XTRA(rv, o->o_address, connection_xtra);
  return rv;
}

static
sdp_origin_t *origin_dup(char **pp, sdp_origin_t const *src)
{
  char *p;
  sdp_origin_t *o;

  p = *pp;
  STRUCT_DUP(p, o, src);
  STR_DUP(p, o, src, o_username);
  PTR_DUP(p, o, src, o_address, connection_dup);

  assert((size_t)(p - *pp) == origin_xtra(src));
  *pp = p;
  return o;
}

static size_t connection_xtra(sdp_connection_t const *c)
{
  size_t rv = sizeof(*c);
  STR_XTRA(rv, c->c_address);
  return rv;
}

static
sdp_connection_t *connection_dup(char **pp, sdp_connection_t const *src)
{
  char *p;
  sdp_connection_t *c;

  p = *pp;
  STRUCT_DUP(p, c, src);
  c->c_next = NULL;
  STR_DUP(p, c, src, c_address);

  assert((size_t)(p - *pp) == connection_xtra(src));
  *pp = p;
  return c;
}

static size_t bandwidth_xtra(sdp_bandwidth_t const *b)
{
  size_t rv = sizeof(*b);
  STR_XTRA(rv, b->b_modifier_name);
  return rv;
}

static
sdp_bandwidth_t *bandwidth_dup(char **pp, sdp_bandwidth_t const *src)
{
  char *p;
  sdp_bandwidth_t *b;

  p = *pp;
  STRUCT_DUP(p, b, src);
  b->b_next = NULL;
  STR_DUP(p, b, src, b_modifier_name);

  assert((size_t)(p - *pp) == bandwidth_xtra(src));
  *pp = p;
  return b;
}


static size_t time_xtra(sdp_time_t const *t)
{
  size_t rv = sizeof(*t);
  PTR_XTRA(rv, t->t_repeat, repeat_xtra);
  PTR_XTRA(rv, t->t_zone, zone_xtra);
  return rv;
}

static
sdp_time_t *time_dup(char **pp, sdp_time_t const *src)
{
  char *p;
  sdp_time_t *t;

  p = *pp;
  STRUCT_DUP(p, t, src);
  t->t_next = NULL;
  PTR_DUP(p, t, src, t_repeat, repeat_dup);
  PTR_DUP(p, t, src, t_zone, zone_dup);

  assert((size_t)(p - *pp) == time_xtra(src));
  *pp = p;
  return t;
}


static size_t repeat_xtra(sdp_repeat_t const *r)
{
  return (size_t)r->r_size;
}

static
sdp_repeat_t *repeat_dup(char **pp, sdp_repeat_t const *src)
{
  char *p;
  sdp_repeat_t *r;

  p = *pp;
  STRUCT_DUP2(p, r, src);

  assert((size_t)(p - *pp) == repeat_xtra(src));
  *pp = p;
  return r;
}


static size_t zone_xtra(sdp_zone_t const *z)
{
  return z->z_size;
}

static
sdp_zone_t *zone_dup(char **pp, sdp_zone_t const *src)
{
  char *p;
  sdp_zone_t *z;

  p = *pp;
  STRUCT_DUP2(p, z, src);

  assert((size_t)(p - *pp) == zone_xtra(src));
  *pp = p;
  return z;
}


static size_t key_xtra(sdp_key_t const *k)
{
  size_t rv = sizeof(*k);
  STR_XTRA(rv, k->k_method_name);
  STR_XTRA(rv, k->k_material);
  return rv;
}

static
sdp_key_t *key_dup(char **pp, sdp_key_t const *src)
{
  char *p;
  sdp_key_t *k;

  p = *pp;
  STRUCT_DUP(p, k, src);
  STR_DUP(p, k, src, k_method_name);
  STR_DUP(p, k, src, k_material);

  assert((size_t)(p - *pp) == key_xtra(src));
  *pp = p;
  return k;
}


static size_t attribute_xtra(sdp_attribute_t const *a)
{
  size_t rv = sizeof(*a);
  STR_XTRA(rv, a->a_name);
  STR_XTRA(rv, a->a_value);
  return rv;
}

static
sdp_attribute_t *attribute_dup(char **pp, sdp_attribute_t const *src)
{
  char *p;
  sdp_attribute_t *a;

  p = *pp;
  STRUCT_DUP(p, a, src);
  a->a_next = NULL;
  STR_DUP(p, a, src, a_name);
  STR_DUP(p, a, src, a_value);

  assert((size_t)(p - *pp) == attribute_xtra(src));
  *pp = p;
  return a;
}


static size_t media_xtra(sdp_media_t const *m)
{
  size_t rv = sizeof(*m);

  STR_XTRA(rv, m->m_type_name);
  STR_XTRA(rv, m->m_proto_name);
  LST_XTRA(rv, m->m_format, list_xtra);
  LST_XTRA(rv, m->m_rtpmaps, rtpmap_xtra);
  STR_XTRA(rv, m->m_information);
  LST_XTRA(rv, m->m_connections, connection_xtra);
  LST_XTRA(rv, m->m_bandwidths, bandwidth_xtra);
  PTR_XTRA(rv, m->m_key, key_xtra);
  LST_XTRA(rv, m->m_attributes, attribute_xtra);

  return rv;
}

static
sdp_media_t *media_dup(char **pp,
		       sdp_media_t const *src,
		       sdp_session_t *sdp)
{
  char *p;
  sdp_media_t *m;

  p = *pp;
  STRUCT_DUP(p, m, src);
  m->m_next = NULL;

  STR_DUP(p, m, src, m_type_name);
  STR_DUP(p, m, src, m_proto_name);
  LST_DUP(p, m, src, m_format, list_dup);
  LST_DUP(p, m, src, m_rtpmaps, rtpmap_dup);
  STR_DUP(p, m, src, m_information);
  LST_DUP(p, m, src, m_connections, connection_dup);
  LST_DUP(p, m, src, m_bandwidths, bandwidth_dup);
  PTR_DUP(p, m, src, m_key, key_dup);
  LST_DUP(p, m, src, m_attributes, attribute_dup);

  /* note! we must not implicitly use 'src->m_session' as it
           might point to a temporary session */
  m->m_session = sdp;

  m->m_rejected = src->m_rejected;
  m->m_mode = src->m_mode;

  assert((size_t)(p - *pp) == media_xtra(src));
  *pp = p;
  return m;
}

#ifdef nomore
static
int media_xtra_ex(sdp_media_t const *m, sdp_connection_t const *c)
{
  int rv = 0;

  for (; m; m = m->m_next) {
    rv += STRUCT_ALIGN(rv);
    rv += sizeof(*m);

    STR_XTRA(rv, m->m_type_name);
    STR_XTRA(rv, m->m_proto_name);
    LST_XTRA(rv, m->m_format, list_xtra);
    LST_XTRA(rv, m->m_rtpmaps, rtpmap_xtra);
    STR_XTRA(rv, m->m_information);
    if (c != m->m_connections)
      LST_XTRA(rv, m->m_connections, connection_xtra);
    LST_XTRA(rv, m->m_bandwidths, bandwidth_xtra);
    PTR_XTRA(rv, m->m_key, key_xtra);
    LST_XTRA(rv, m->m_attributes, attribute_xtra);
  }

  return rv;
}

static
sdp_media_t *media_dup_ex(char **pp,
			  sdp_media_t const *src,
			  sdp_session_t *sdp,
			  sdp_connection_t *dst_c,
			  sdp_connection_t const *src_c)
{
  char *p;
  sdp_media_t *retval = NULL, *m, **mm = &retval;
  int xtra = media_xtra_ex(src, src_c);

  p = *pp;

  for (; src; src = src->m_next) {
    p += STRUCT_ALIGN(p);
    STRUCT_DUP(p, m, src);
    m->m_next = NULL;

    STR_DUP(p, m, src, m_type_name);
    STR_DUP(p, m, src, m_proto_name);
    LST_DUP(p, m, src, m_format, list_dup);
    LST_DUP(p, m, src, m_rtpmaps, rtpmap_dup);
    STR_DUP(p, m, src, m_information);
    if (src_c != src->m_connections)
      LST_DUP(p, m, src, m_connections, connection_dup);
    else
      m->m_connections = dst_c;
    LST_DUP(p, m, src, m_bandwidths, bandwidth_dup);
    PTR_DUP(p, m, src, m_key, key_dup);
    LST_DUP(p, m, src, m_attributes, attribute_dup);

    /* note! we must not implicitly use 'src->m_session' as it
       might point to a temporary session */
    m->m_session = sdp;

    m->m_rejected = src->m_rejected;
    m->m_mode = src->m_mode;

    assert(m);
    *mm = m; mm = &m->m_next;
  }

  assert(p - *pp == xtra);


  *pp = p;

  return retval;
}
#endif

static size_t media_xtra_all(sdp_media_t const *m)
{
  size_t rv = 0;

  for (; m; m = m->m_next) {
    rv += STRUCT_ALIGN(rv);
    rv += media_xtra(m);
  }

  return rv;
}

static
sdp_media_t *media_dup_all(char **pp,
			   sdp_media_t const *src,
			   sdp_session_t *sdp)
{
  char *p;
  sdp_media_t *retval = NULL, *m, **mm = &retval;

  p = *pp;

  for (; src; src = src->m_next) {
    p += STRUCT_ALIGN(p);
    m = media_dup(&p, src, sdp);
    assert(m);
    *mm = m; mm = &m->m_next;
  }

  *pp = p;

  return retval;
}

static size_t list_xtra(sdp_list_t const *l)
{
  size_t rv = sizeof(*l);
  rv += strlen(l->l_text) + 1;
  return rv;
}

static
sdp_list_t *list_dup(char **pp, sdp_list_t const *src)
{
  char *p;
  sdp_list_t *l;

  p = *pp;
  STRUCT_DUP(p, l, src);
  l->l_next = NULL;
  STR_DUP(p, l, src, l_text);

  assert((size_t)(p - *pp) == list_xtra(src));
  *pp = p;
  return l;
}


static size_t rtpmap_xtra(sdp_rtpmap_t const *rm)
{
  size_t rv = sizeof(*rm);
  STR_XTRA(rv, rm->rm_encoding);
  STR_XTRA(rv, rm->rm_params);
  STR_XTRA(rv, rm->rm_fmtp);
  return rv;
}

static
sdp_rtpmap_t *rtpmap_dup(char **pp, sdp_rtpmap_t const *src)
{
  char *p;
  sdp_rtpmap_t *rm;

  p = *pp;
  STRUCT_DUP(p, rm, src);
  rm->rm_next = NULL;
  STR_DUP(p, rm, src, rm_encoding);
  STR_DUP(p, rm, src, rm_params);
  STR_DUP(p, rm, src, rm_fmtp);

  assert((size_t)(p - *pp) == rtpmap_xtra(src));
  *pp = p;
  return rm;
}

/** Return total size of a list, including size of all nodes */
static size_t list_xtra_all(xtra_f *xtra, void const *v)
{
  size_t rv = 0;
  sdp_list_t const *l;

  for (l = v; l; l = l->l_next) {
    rv += STRUCT_ALIGN(rv);
    rv += xtra(l);
  }

  return rv;
}

static
void *list_dup_all(dup_f *dup, char **pp, void const *vsrc)
{
  char *p;
  sdp_list_t const *src;
  sdp_list_t *retval = NULL, *l, **ll = &retval;

  p = *pp;

  for (src = vsrc; src; src = src->l_next) {
    p += STRUCT_ALIGN(p);
    l = dup(&p, src);
    assert(l);
    *ll = l; ll = &l->l_next;
  }

  *pp = p;

  return retval;
}

#if 0
static size_t XXX_xtra(sdp_XXX_t const *YYY)
{
  size_t rv = sizeof(*YYY);
  rv += strlen(YYY->YYY_encoding) + 1;
  if (YYY->YYY_params);
    rv += strlen(YYY->YYY_params) + 1;
  return rv;
}

static
sdp_XXX_t *XXX_dup(char **pp, sdp_XXX_t const *src)
{
  char *p;
  sdp_XXX_t *YYY;

  p = *pp; ASSERT_STRUCT_ALIGN(p);
  YYY = memcpy(p, src, src->YYY_size);
  p += src->YYY_size;
  YYY->YYY_next = NULL;
  ZZZ
  *pp = p;
  return YYY;
}

#endif

static size_t session_xtra(sdp_session_t const *sdp)
{
  size_t rv = sizeof(*sdp);

  PTR_XTRA(rv, sdp->sdp_origin, origin_xtra);
  STR_XTRA(rv, sdp->sdp_subject);
  STR_XTRA(rv, sdp->sdp_information);
  STR_XTRA(rv, sdp->sdp_uri);
  LST_XTRA(rv, sdp->sdp_emails, list_xtra);
  LST_XTRA(rv, sdp->sdp_phones, list_xtra);
  LST_XTRA(rv, sdp->sdp_connection, connection_xtra);
  LST_XTRA(rv, sdp->sdp_bandwidths, bandwidth_xtra);
  LST_XTRA(rv, sdp->sdp_time, time_xtra);
  PTR_XTRA(rv, sdp->sdp_key, key_xtra);
  LST_XTRA(rv, sdp->sdp_attributes, attribute_xtra);
  STR_XTRA(rv, sdp->sdp_charset);
  MED_XTRA_ALL(rv, sdp->sdp_media);

  return rv;
}

static
sdp_session_t *session_dup(char **pp, sdp_session_t const *src)
{
  char *p;
  sdp_session_t *sdp;

  p = *pp;
  STRUCT_DUP(p, sdp, src);
  sdp->sdp_next = NULL;

  PTR_DUP(p, sdp, src, sdp_origin, origin_dup);
  STR_DUP(p, sdp, src, sdp_subject);
  STR_DUP(p, sdp, src, sdp_information);
  STR_DUP(p, sdp, src, sdp_uri);
  LST_DUP(p, sdp, src, sdp_emails, list_dup);
  LST_DUP(p, sdp, src, sdp_phones, list_dup);
  LST_DUP(p, sdp, src, sdp_connection, connection_dup);
  LST_DUP(p, sdp, src, sdp_bandwidths, bandwidth_dup);
  LST_DUP(p, sdp, src, sdp_time, time_dup);
  PTR_DUP(p, sdp, src, sdp_key, key_dup);
  LST_DUP(p, sdp, src, sdp_attributes, attribute_dup);
  STR_DUP(p, sdp, src, sdp_charset);
  MED_DUP_ALL(p, sdp, src, sdp_media);

  assert((size_t)(p - *pp) == session_xtra(src));
  *pp = p;
  return sdp;
}

/**Duplicate an SDP session description.
 *
 * The function sdp_session_dup() duplicates (deeply copies) an SDP
 * session description @a sdp allocating memory using memory @a home.
 *
 * @param h   Memory home
 * @param sdp SDP session description to be duplicated
 *
 * @note The duplicated structure is allocated using a single call to
 * su_alloc() and it can be freed with su_free().
 *
 * @return
 * If successful, a pointer to newly allocated sdp_session_t structure is
 * returned, otherwise NULL is returned.
 */

sdp_session_t *sdp_session_dup(su_home_t *h, sdp_session_t const *sdp)
{
  SDP_DUP(session, sdp);
}

/* ---------------------------------------------------------------------- */

static size_t session_without_media_xtra(sdp_session_t const *sdp)
{
  size_t rv = sizeof(*sdp);

  PTR_XTRA(rv, sdp->sdp_origin, origin_xtra);
  STR_XTRA(rv, sdp->sdp_subject);
  STR_XTRA(rv, sdp->sdp_information);
  STR_XTRA(rv, sdp->sdp_uri);
  LST_XTRA(rv, sdp->sdp_emails, list_xtra);
  LST_XTRA(rv, sdp->sdp_phones, list_xtra);
  LST_XTRA(rv, sdp->sdp_connection, connection_xtra);
  LST_XTRA(rv, sdp->sdp_bandwidths, bandwidth_xtra);
  LST_XTRA(rv, sdp->sdp_time, time_xtra);
  PTR_XTRA(rv, sdp->sdp_key, key_xtra);
  LST_XTRA(rv, sdp->sdp_attributes, attribute_xtra);
  STR_XTRA(rv, sdp->sdp_charset);

  return rv;
}

static
sdp_session_t *session_without_media_dup(char **pp, sdp_session_t const *src)
{
  char *p;
  sdp_session_t *sdp;

  p = *pp;
  STRUCT_DUP(p, sdp, src);
  sdp->sdp_next = NULL;

  PTR_DUP(p, sdp, src, sdp_origin, origin_dup);
  STR_DUP(p, sdp, src, sdp_subject);
  STR_DUP(p, sdp, src, sdp_information);
  STR_DUP(p, sdp, src, sdp_uri);
  LST_DUP(p, sdp, src, sdp_emails, list_dup);
  LST_DUP(p, sdp, src, sdp_phones, list_dup);
  LST_DUP(p, sdp, src, sdp_connection, connection_dup);
  LST_DUP(p, sdp, src, sdp_bandwidths, bandwidth_dup);
  LST_DUP(p, sdp, src, sdp_time, time_dup);
  PTR_DUP(p, sdp, src, sdp_key, key_dup);
  LST_DUP(p, sdp, src, sdp_attributes, attribute_dup);
  STR_DUP(p, sdp, src, sdp_charset);

  sdp->sdp_media = NULL;

  assert((size_t)(p - *pp) == session_without_media_xtra(src));
  *pp = p;
  return sdp;
}

/* SDP_DUP macro requires this */
typedef sdp_session_t sdp_session_without_media_t;

/**Duplicate an SDP session description without media descriptions.
 *
 * The function sdp_session_dup() duplicates (deeply copies) an SDP session
 * description @a sdp allocating memory using memory @a home. It does not
 * copy the media descriptions, however.
 *
 * @param h     memory h
 * @param sdp   SDP session description to be duplicated
 *
 * @note The duplicated structure is allocated using a single call to
 * su_alloc() and it can be freed with su_free().
 *
 * @return
 * If successful, a pointer to newly allocated sdp_session_t structure is
 * returned, otherwise NULL is returned.
 */

sdp_session_t *sdp_session_dup_without_media(su_home_t *h,
					     sdp_session_t const *sdp)
{
  SDP_DUP(session_without_media, sdp);
}

/* ---------------------------------------------------------------------- */
/* SDP Tag classes */

#include <sofia-sip/su_tag_class.h>

size_t sdptag_session_xtra(tagi_t const *t, size_t offset)
{
  sdp_session_t const *sdp = (sdp_session_t *)t->t_value;

  if (sdp)
    return STRUCT_ALIGN(offset) + session_xtra(sdp);
  else
    return 0;
}

tagi_t *sdptag_session_dup(tagi_t *dst, tagi_t const *src, void **bb)
{
  sdp_session_t *sdp;
  sdp_session_t const *srcsdp;
  char *b;

  assert(src); assert(*bb);

  b = *bb;
  b += STRUCT_ALIGN(b);
  srcsdp = (sdp_session_t *)src->t_value;

  sdp = srcsdp ? session_dup(&b, srcsdp) : NULL;

  dst->t_tag = src->t_tag;
  dst->t_value = (tag_value_t)sdp;

  *bb = b;

  return dst + 1;
}

int sdptag_session_snprintf(tagi_t const *t, char b[], size_t size)
{
  sdp_session_t const *sdp;
  sdp_printer_t *print;
  size_t retval;

  assert(t);

  if (!t || !t->t_value) {
    if (size && b) b[0] = 0;
    return 0;
  }

  sdp = (sdp_session_t const *)t->t_value;

  print = sdp_print(NULL, sdp, b, size, 0);

  retval = sdp_message_size(print);

  sdp_printer_free(print);

  return (int)retval;
}

/** Tag class for SDP tags. @HIDE */
tag_class_t sdptag_session_class[1] =
  {{
    sizeof(sdptag_session_class),
    /* tc_next */     NULL,
    /* tc_len */      NULL,
    /* tc_move */     NULL,
    /* tc_xtra */     sdptag_session_xtra,
    /* tc_dup */      sdptag_session_dup,
    /* tc_free */     NULL,
    /* tc_find */     NULL,
    /* tc_snprintf */ sdptag_session_snprintf,
    /* tc_filter */   NULL /* msgtag_str_filter */,
    /* tc_ref_set */  t_ptr_ref_set,
  }};


/* ---------------------------------------------------------------------- */

/** Compare two session descriptions
 */
int sdp_session_cmp(sdp_session_t const *a, sdp_session_t const *b)
{
  int rv;
  sdp_bandwidth_t const *ab, *bb;
  sdp_attribute_t const *aa, *ba;
  sdp_media_t const *am, *bm;

  if ((rv = (a != NULL) - (b != NULL)))
    return rv;
  if (a == b)
    return 0;
  if ((rv = (a->sdp_version[0] - b->sdp_version[0])))
    return rv;
  if ((rv = sdp_origin_cmp(a->sdp_origin, b->sdp_origin)))
    return rv;
  if ((rv = su_strcmp(a->sdp_subject, b->sdp_subject)))
    return rv;
  if ((rv = su_strcmp(a->sdp_information, b->sdp_information)))
    return rv;
  if ((rv = su_strcmp(a->sdp_uri, b->sdp_uri)))
    return rv;
  if ((rv = sdp_list_cmp(a->sdp_emails, b->sdp_emails)))
    return rv;
  if ((rv = sdp_list_cmp(a->sdp_phones, b->sdp_phones)))
    return rv;
  if ((rv = sdp_connection_cmp(a->sdp_connection, b->sdp_connection)))
    return rv;

  for (ab = a->sdp_bandwidths, bb = b->sdp_bandwidths;
       ab || bb;
       ab = ab->b_next, bb = bb->b_next)
    if ((rv = sdp_bandwidth_cmp(a->sdp_bandwidths, b->sdp_bandwidths)))
      return rv;

  if ((rv = sdp_time_cmp(a->sdp_time, b->sdp_time)))
    return rv;
  if ((rv = sdp_key_cmp(a->sdp_key, b->sdp_key)))
    return rv;

  for (aa = a->sdp_attributes, ba = b->sdp_attributes;
       aa || bb;
       aa = aa->a_next, ba = ba->a_next)
    if ((rv = sdp_attribute_cmp(aa, ba)))
      return rv;

  for (am = a->sdp_media, bm = b->sdp_media;
       am || bm;
       am = am->m_next, bm = bm->m_next)
    if ((rv = sdp_media_cmp(am, bm)))
      return rv;

  return 0;
}

/** Compare two origin fields
 */
int sdp_origin_cmp(sdp_origin_t const *a, sdp_origin_t const *b)
{
  int rv;

  if ((rv = (a != NULL) - (b != NULL)))
    return rv;
  if (a == b)
    return 0;
  if (a->o_version != b->o_version)
    return a->o_version < b->o_version ? -1 : 1;
  if (a->o_id != b->o_id)
    return a->o_id < b->o_id ? -1 : 1;
  if ((rv = su_strcasecmp(a->o_username, b->o_username)))
    return rv;
  if ((rv = su_strcasecmp(a->o_address->c_address, b->o_address->c_address)))
    return rv;

  return 0;
}

/** Compare two connection fields
 */
int sdp_connection_cmp(sdp_connection_t const *a, sdp_connection_t const *b)
{
  if (a == b)
    return 0;
  if ((a != NULL) != (b != NULL))
    return (a != NULL) < (b != NULL) ? -1 : 1;

  if (a->c_nettype != b->c_nettype)
    return a->c_nettype < b->c_nettype ? -1 : 1;
  if (a->c_addrtype != b->c_addrtype)
    return a->c_addrtype < b->c_addrtype ? -1 : 1;
  if (a->c_ttl != b->c_ttl)
    return a->c_ttl < b->c_ttl ? -1 : 1;
  if (a->c_groups != b->c_groups)
    return a->c_groups < b->c_groups ? -1 : 1;

  return strcmp(a->c_address, b->c_address);
}

/** Compare two bandwidth (b=) fields */
int sdp_bandwidth_cmp(sdp_bandwidth_t const *a, sdp_bandwidth_t const *b)
{
  int rv;

  if (a == b)
    return 0;
  if ((a != NULL) != (b != NULL))
    return (a != NULL) < (b != NULL) ? -1 : 1;

  if (a->b_modifier != b->b_modifier)
    return a->b_modifier < b->b_modifier ? -1 : 1;
  if (a->b_modifier == sdp_bw_x &&
      (rv = strcmp(a->b_modifier_name, b->b_modifier_name)))
    return rv;

  if (a->b_value != b->b_value)
    return a->b_value < b->b_value ? -1 : 1;

  return 0;
}

/** Compare two time fields */
int sdp_time_cmp(sdp_time_t const *a, sdp_time_t const *b)
{
  int rv;

  if ((rv = (a != NULL) - (b != NULL)))
    return rv;
  if (a == b)
    return 0;
  if (a->t_start != b->t_start)
    return a->t_start < b->t_start ? -1 : 1;
  if (a->t_stop != b->t_stop)
    return a->t_stop < b->t_stop ? -1 : 1;
  if ((rv = sdp_zone_cmp(a->t_zone, b->t_zone)))
    return rv;
  if ((rv = sdp_repeat_cmp(a->t_repeat, b->t_repeat)))
    return rv;
  return 0;
}

/** Compare two repeat (r=) fields */
int sdp_repeat_cmp(sdp_repeat_t const *a, sdp_repeat_t const *b)
{
  int i, n;

  if (a == b)
    return 0;
  if ((a != NULL) != (b != NULL))
    return (a != NULL) < (b != NULL) ? -1 : 1;

  if (a->r_interval != b->r_interval)
    return a->r_interval < b->r_interval ? -1 : 1;
  if (a->r_duration != b->r_duration)
    return a->r_duration < b->r_duration ? -1 : 1;
  n = a->r_number_of_offsets < b->r_number_of_offsets
    ? a->r_number_of_offsets : b->r_number_of_offsets;
  for (i = 0; i < n; i++)
    if (a->r_offsets[i] != b->r_offsets[i])
      return a->r_offsets[i] < b->r_offsets[i] ? -1 : 1;

  if (a->r_number_of_offsets != b->r_number_of_offsets)
    return a->r_number_of_offsets < b->r_number_of_offsets ? -1 : 1;

  return 0;
 }

/** Compare two zone (z=) fields */
int sdp_zone_cmp(sdp_zone_t const *a, sdp_zone_t const *b)
{
  int i, n;

  if (a == b)
    return 0;
  if ((a != NULL) != (b != NULL))
    return (a != NULL) < (b != NULL) ? -1 : 1;

  n = a->z_number_of_adjustments < b->z_number_of_adjustments
    ? a->z_number_of_adjustments : b->z_number_of_adjustments;
  for (i = 0; i < n; i++) {
    if (a->z_adjustments[i].z_at != b->z_adjustments[i].z_at)
      return a->z_adjustments[i].z_at < b->z_adjustments[i].z_at ? -1 : 1;
    if (a->z_adjustments[i].z_offset != b->z_adjustments[i].z_offset)
      return a->z_adjustments[i].z_offset < b->z_adjustments[i].z_offset
	? -1 : 1;
  }

  if (a->z_number_of_adjustments != b->z_number_of_adjustments)
    return a->z_number_of_adjustments < b->z_number_of_adjustments ? -1 : 1;

  return 0;
}

/** Compare two key (k=) fields */
int sdp_key_cmp(sdp_key_t const *a, sdp_key_t const *b)
{
  int rv;

  if (a == b)
    return 0;
  if ((a != NULL) != (b != NULL))
    return (a != NULL) < (b != NULL) ? -1 : 1;

  if (a->k_method != b->k_method)
    return a->k_method < b->k_method ? -1 : 1;
  if (a->k_method == sdp_key_x &&
      (rv = su_strcmp(a->k_method_name, b->k_method_name)))
    return rv;
  return su_strcmp(a->k_material, b->k_material);
}

/** Compare two attribute (a=) fields */
int sdp_attribute_cmp(sdp_attribute_t const *a, sdp_attribute_t const *b)
{
  int rv;

  if (a == b)
    return 0;
  if ((a != NULL) != (b != NULL))
    return (a != NULL) < (b != NULL) ? -1 : 1;

  if ((rv = su_strcmp(a->a_name, b->a_name)))
    return rv;
  return su_strcmp(a->a_value, b->a_value);
}

/** Compare two rtpmap structures. */
int sdp_rtpmap_cmp(sdp_rtpmap_t const *a, sdp_rtpmap_t const *b)
{
  int rv;

  if (a == b)
    return 0;
  if ((a != NULL) != (b != NULL))
    return (a != NULL) < (b != NULL) ? -1 : 1;

  if (a->rm_pt != b->rm_pt)
    return a->rm_pt < b->rm_pt ? -1 : 1;

  /* Case insensitive encoding */
  if ((rv = su_strcmp(a->rm_encoding, b->rm_encoding)))
    return rv;
  /* Rate */
  if (a->rm_rate != b->rm_rate)
    return a->rm_rate < b->rm_rate ? -1 : 1;

  {
    char const *a_param = "1", *b_param = "1";

    if (a->rm_params)
      a_param = a->rm_params;
    if (b->rm_params)
      b_param = b->rm_params;

    rv = su_strcasecmp(a_param, b_param);

    if (rv)
      return rv;
  }

  return su_strcasecmp(a->rm_fmtp, b->rm_fmtp);
}

/** Compare two lists. */
int sdp_list_cmp(sdp_list_t const *a, sdp_list_t const *b)
{
  int rv;

  for (;a || b; a = a->l_next, b = b->l_next) {
    if (a == b)
      return 0;
    if ((a != NULL) != (b != NULL))
      return (a != NULL) < (b != NULL) ? -1 : 1;
    if ((rv = su_strcmp(a->l_text, b->l_text)))
      return rv;
  }

  return 0;
}

/** Compare two media (m=) fields */
int sdp_media_cmp(sdp_media_t const *a, sdp_media_t const *b)
{
  int rv;

  sdp_connection_t const *ac, *bc;
  sdp_bandwidth_t const *ab, *bb;
  sdp_rtpmap_t const *arm, *brm;
  sdp_attribute_t const *aa, *ba;

  if (a == b)
    return 0;
  if ((rv = (a != NULL) - (b != NULL)))
    return rv;

  if (a->m_type != b->m_type)
    return a->m_type < b->m_type ? -1 : 1;
  if (a->m_type == sdp_media_x)
    if ((rv = su_strcmp(a->m_type_name, b->m_type_name)))
      return rv;
  if (a->m_port != b->m_port)
    return a->m_port < b->m_port ? -1 : 1;

  if (a->m_port == 0 /* && b->m_port == 0 */)
    /* Ignore transport protocol and media list if media has been rejected */
    return 0;

  if (a->m_number_of_ports != b->m_number_of_ports)
    return a->m_number_of_ports < b->m_number_of_ports ? -1 : 1;

  if (a->m_proto != b->m_proto)
    return a->m_proto < b->m_proto ? -1 : 1;
  if (a->m_proto == sdp_proto_x)
    if ((rv = su_strcmp(a->m_proto_name, b->m_proto_name)))
      return rv;

  if (a->m_mode != b->m_mode)
    return a->m_mode < b->m_mode ? -1 : 1;

  for (arm = a->m_rtpmaps, brm = b->m_rtpmaps;
       arm || brm;
       arm = arm->rm_next, brm = brm->rm_next)
    if ((rv = sdp_rtpmap_cmp(arm, brm)))
      return rv;

  if ((rv = sdp_list_cmp(a->m_format, b->m_format)))
    return rv;

  if ((rv = su_strcmp(a->m_information, b->m_information)))
    return rv;

  for (ac = a->m_connections, bc = b->m_connections;
       ac || bc;
       ac = ac->c_next, bc = bc->c_next)
  if ((rv = sdp_connection_cmp(ac, bc)))
    return rv;

  for (ab = a->m_bandwidths, bb = b->m_bandwidths;
       ab || bb;
       ab = ab->b_next, bb = bb->b_next)
    if ((rv = sdp_bandwidth_cmp(a->m_bandwidths, b->m_bandwidths)))
      return rv;

  if ((rv = sdp_key_cmp(a->m_key, b->m_key)))
    return rv;

  for (aa = a->m_attributes, ba = b->m_attributes;
       aa || bb;
       aa = aa->a_next, ba = ba->a_next)
    if ((rv = sdp_attribute_cmp(aa, ba)))
      return rv;

  return 0;
}

/* ---------------------------------------------------------------------- */

sdp_connection_t *sdp_media_connections(sdp_media_t const *m)
{
  if (m) {
    if (m->m_connections)
      return m->m_connections;
    if (m->m_session)
      return m->m_session->sdp_connection;
  }
  return NULL;
}

/* ---------------------------------------------------------------------- */

/** Find named attribute from given list. */
sdp_attribute_t *sdp_attribute_find(sdp_attribute_t const *a, char const *name)
{
  for (; a; a = a->a_next) {
    if (su_casematch(a->a_name, name))
      break;
  }

  return (sdp_attribute_t *)a;
}

/** Find named attribute from given lists (a or a2). */
sdp_attribute_t *sdp_attribute_find2(sdp_attribute_t const *a,
				     sdp_attribute_t const *a2,
				     char const *name)
{
  for (; a; a = a->a_next) {
    if (su_casematch(a->a_name, name))
      break;
  }

  if (a == 0)
    for (a = a2; a; a = a->a_next) {
      if (su_casematch(a->a_name, name))
	break;
    }

  return (sdp_attribute_t *)a;
}

/** Get session mode from attribute list. */
sdp_mode_t sdp_attribute_mode(sdp_attribute_t const *a, sdp_mode_t defmode)
{
  for (; a; a = a->a_next) {
    if (su_casematch(a->a_name, "sendrecv"))
      return sdp_sendrecv;
    if (su_casematch(a->a_name, "inactive"))
      return sdp_inactive;
    if (su_casematch(a->a_name, "recvonly"))
      return sdp_recvonly;
    if (su_casematch(a->a_name, "sendonly"))
      return sdp_sendonly;
  }

  return defmode;
}

/** Convert session mode as #sdp_attribute_t structure. */
sdp_attribute_t *sdp_attribute_by_mode(su_home_t *home, sdp_mode_t mode)
{
  sdp_attribute_t *a;
  char const *name;

  if (mode == sdp_inactive)
    name = "inactive";
  else if (mode == sdp_sendonly)
    name = "sendonly";
  else if (mode == sdp_recvonly)
    name = "recvonly";
  else if (mode == sdp_sendrecv)
    name = "sendrecv";
  else
    return NULL;

  a = su_salloc(home, sizeof(*a));
  if (a)
    a->a_name = name;

  return a;
}

/** Find a mapped attribute.
 *
 * A mapped attribute has form 'a=<name>:<pt> <value>' where pt is a RTP
 * payload type, integer in range 0..127. For example, "a=atmmap" [@RFC3108]
 * is a mapped attribute. Note that common mapped attributes, "a=rtpmap" and
 * "a=fmtp" are already parsed as list of #sdp_rtpmap_t in #sdp_media_t.
 *
 * @param a pointer to first attribute in the list
 * @param name name of the attribute
 * @param pt payload type number (must be 0..127)
 * @param return_result return value parameter for mapped attribute value
 *
 * @return Pointer to a matching attribute structure, or NULL.
 *
 * If a matching attribute is found, @a return_result will point to part of
 * the attribute after the payload type and whitespace.
 */
sdp_attribute_t *sdp_attribute_mapped_find(sdp_attribute_t const *a,
					   char const *name,
					   int pt, char **return_result)
{
  char pt_value[4];
  size_t pt_len;

  if (return_result)
    *return_result = NULL;

  if (0 > pt || pt > 127)
    return NULL;

  snprintf(pt_value, sizeof(pt_value), "%u", (unsigned)pt);
  pt_len = strlen(pt_value);

  for (; (a = sdp_attribute_find(a, name)); a = a->a_next) {
    char const *value = a->a_value;
    size_t wlen;

    if (strncmp(value, pt_value, pt_len))
      continue;

    wlen = strspn(value + pt_len, " \t");

    if (wlen == 0 || value[pt_len + wlen] == '\0')
      continue;

    if (return_result)
      *return_result = (char *)value + pt_len + wlen;

    return (sdp_attribute_t *)a;
  }

  return NULL;
}

/** Append a (list of) attribute(s) to a list of attributes. */
void sdp_attribute_append(sdp_attribute_t **list,
			  sdp_attribute_t const *a)
{
  assert(list);

  if (list == NULL || a == NULL)
    return;

  for (;*list; list = &(*list)->a_next)
    ;

  *list = (sdp_attribute_t *)a;
}

/**Replace or append a attribute within a list of attributes.
 *
 * @retval 1 if replaced existing attribute
 * @retval 0 if attribute was appended
 * @retval -1 upon an error
 */
int sdp_attribute_replace(sdp_attribute_t **list,
			  sdp_attribute_t *a,
			  sdp_attribute_t **return_replaced)
{
  sdp_attribute_t *replaced;

  assert(list);

  if (return_replaced)
    *return_replaced = NULL;

  if (list == NULL || a == NULL)
    return -1;

  assert(a->a_name != NULL); assert(a->a_next == NULL);

  for (; *list; list = &(*list)->a_next) {
    if (su_casematch((*list)->a_name, a->a_name))
      break;
  }

  replaced = *list, *list = a;

  if (replaced) {
    a->a_next = replaced->a_next;
    replaced->a_next = NULL;

    if (return_replaced)
      *return_replaced = replaced;

    return 1;
  }

  return 0;
}

/** Remove a named attribute from a list of attributes. */
sdp_attribute_t *sdp_attribute_remove(sdp_attribute_t **list,
				      char const *name)
{
  sdp_attribute_t *a;

  assert(list);

  if (list == NULL)
    return NULL;
  if (name == NULL)
    return NULL;

  for (a = *list; a; list = &a->a_next, a = *list) {
    if (su_casematch(name, a->a_name))
      break;
  }

  if (a) {
    *list = a->a_next;
    a->a_next = NULL;
  }

  return a;
}

/* Return 1 if m= line struct matches with given type and name */
unsigned sdp_media_match(sdp_media_t const *m,
			 sdp_media_e type,
			 sdp_text_t *type_name,
			 sdp_proto_e proto,
			 sdp_text_t *proto_name)
{
  if (m == NULL)
    return 0;

  if (type == sdp_media_any || m->m_type == sdp_media_any)
    return 1;

  if (type_name == NULL)
    type_name = "";

  if (type != m->m_type ||
      (type == sdp_media_x && !su_casematch(m->m_type_name, type_name)))
    return 0;

  if (proto == sdp_proto_any || m->m_proto == sdp_proto_any)
    return 1;

  if (proto_name == NULL)
    proto_name = "";

  if (proto != m->m_proto ||
      (proto == sdp_proto_x && !su_casematch(m->m_proto_name, proto_name)))
    return 0;

  return 1;
}

/* Return 1 if media type and protocol of m= line structs matches */
unsigned sdp_media_match_with(sdp_media_t const *a,
			      sdp_media_t const *b)
{
  if (a == NULL || b == NULL)
    return a == b;

  if (a->m_type == sdp_media_any || b->m_type == sdp_media_any)
    return 1;

  if (a->m_type != b->m_type ||
      (a->m_type == sdp_media_x
        && !su_casematch(b->m_type_name, a->m_type_name)))
    return 0;

  if (a->m_proto == sdp_proto_any || b->m_proto == sdp_proto_any)
    return 1;

  if (a->m_proto != b->m_proto ||
      (a->m_proto == sdp_proto_x
       && !su_casematch(b->m_proto_name, a->m_proto_name)))
    return 0;

  return 1;
}


/** Count matching media lines in SDP. */
unsigned sdp_media_count(sdp_session_t const *sdp,
			 sdp_media_e type,
			 sdp_text_t *type_name,
			 sdp_proto_e proto,
			 sdp_text_t *proto_name)
{
  unsigned count = 0;
  sdp_media_t const *m;

  if (sdp != NULL)
    for (m = sdp->sdp_media; m; m = m->m_next)
      count += sdp_media_match(m, type, type_name, proto, proto_name);

  return count;
}

/** Count matching media lines in SDP. */
unsigned sdp_media_count_with(sdp_session_t const *sdp,
			      sdp_media_t const *m0)
{
  unsigned count = 0;
  sdp_media_t const *m;

  if (sdp != NULL)
    for (m = sdp->sdp_media; m; m = m->m_next)
      count += sdp_media_match_with(m, m0);

  return count;
}

/** Return true if media uses RTP */
int sdp_media_uses_rtp(sdp_media_t const *m)
{
  return m &&
    (m->m_proto == sdp_proto_rtp ||
     m->m_proto == sdp_proto_srtp || m->m_proto == sdp_proto_extended_srtp ||
     (m->m_proto == sdp_proto_x && m->m_proto_name &&
      su_casenmatch(m->m_proto_name, "RTP/", 4)));
}

/** Check if payload type, rtp rate and parameters match in rtpmaps*/
int sdp_rtpmap_match(sdp_rtpmap_t const *a, sdp_rtpmap_t const *b)
{
  char const *aparam, *bparam;

  if (a == b)
    return 1;

  if (a == 0 || b == 0)
    return 0;

  if (a->rm_rate != b->rm_rate)
    return 0;

  if (!su_casematch(a->rm_encoding, b->rm_encoding))
    return 0;

  aparam = a->rm_params; bparam = b->rm_params;

  if (aparam == bparam)
    return 1;

  if (!aparam) aparam = "1"; if (!bparam) bparam = "1";

  if (!su_casematch(aparam, bparam))
    return 0;

  return 1;
}

/** Search for matching rtpmap from list.
 *
 * @note
 * The a=fmtp: for the codecs are not compared.
 */
sdp_rtpmap_t *sdp_rtpmap_find_matching(sdp_rtpmap_t const *list,
				       sdp_rtpmap_t const *rm)
{
  char const *lparam, *rparam;
  sdp_rtpmap_t const *cp_list = NULL;

  if (rm == NULL)
    return NULL;
    
  for (; list; list = list->rm_next) {
    if (rm->rm_rate != list->rm_rate)
      continue;

    if (!su_casematch(rm->rm_encoding, list->rm_encoding))
      continue;

    lparam = rm->rm_params; rparam = list->rm_params;

    if (lparam == rparam) {
          cp_list = list;
          if (rm->rm_pt != list->rm_pt) continue;
          break;
    }

    if (!lparam) lparam = "1"; if (!rparam) rparam = "1";
    if (!su_casematch(lparam, rparam))
      continue;

    break;
  }

  return cp_list ? (sdp_rtpmap_t *) cp_list : (sdp_rtpmap_t *)list;
}
