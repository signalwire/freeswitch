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
 * @file urlmap.c
 * @brief Mapping with hierarchical URLs.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Wed Mar 10 17:05:23 2004 ppessi
 *
 */

#include "config.h"

#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <stdlib.h>

#include "urlmap.h"

/** Create map entry. */
UrlMap *url_map_new(su_home_t *home,
		    url_string_t const *url,
		    unsigned size)
{
  UrlMap *um;
  int xtra;
  url_t *u;

  xtra = url_xtra(url->us_url);
  um = su_zalloc(home, size + xtra);
  if (!um || url_dup((char *)um + size, xtra, um->um_url, url->us_url) < 0) {
    su_free(home, um);
    return NULL;
  }

  u = um->um_url;
  if (!u->url_path)
    u->url_path = "";

  return um;
}

static
void left_rotate(UrlMap **top, UrlMap *x)
{
  /*               x                c
   *              / \              / \
   * Convert     a   c    into    x   d
   *                / \          / \
   *               b   d        a   b
   */
  UrlMap *c = x->um_right, *dad = x->um_dad; assert(c);

  if ((x->um_right = c->um_left))
    x->um_right->um_dad = x;

  if (!(c->um_dad = dad))
    *top = c;
  else if (dad->um_left == x)
    dad->um_left = c;
  else
    assert(dad->um_right == x), dad->um_right = c;

  c->um_left = x;
  x->um_dad = c;
}

static
void right_rotate(UrlMap **top, UrlMap *x)
{
  /*               x                c
   *              / \              / \
   * Convert     c   f    into    a   x
   *            / \                  / \
   *           a   d                d   f
   */
  UrlMap *c = x->um_left, *dad = x->um_dad; assert(c);

  if ((x->um_left = c->um_right))
    x->um_left->um_dad = x;

  if (!(c->um_dad = dad))
    *top = c;
  else if (dad->um_right == x)
    dad->um_right = c;
  else
    assert(dad->um_left == x), dad->um_left = c;

  c->um_right = x;
  x->um_dad = c;
}

/** Balance Red-Black binary tree after inserting node @a um.
 *
 * The function red_black_balance_insert() balances a red-black tree after
 * insertion.
 */
static
void red_black_balance_insert(UrlMap **top, UrlMap *um)
{
  UrlMap *dad, *uncle, *granddad;

  um->um_black = 0;

  for (dad = um->um_dad; um != *top && !dad->um_black; dad = um->um_dad) {
    /* Repeat until we are parent or we have a black dad */
    granddad = dad->um_dad; assert(granddad);
    if (dad == granddad->um_left) {
      uncle = granddad->um_right;
      if (uncle && !uncle->um_black) {
	dad->um_black = 1;
	uncle->um_black = 1;
	granddad->um_black = 0;
	um = granddad;
      } else {
	if (um == dad->um_right) {
	  left_rotate(top, um = dad);
	  dad = um->um_dad; assert(dad);
	  granddad = dad->um_dad; assert(granddad);
	}
	dad->um_black = 1;
	granddad->um_black = 0;
	right_rotate(top, granddad);
      }
    } else { assert(dad == granddad->um_right);
      uncle = granddad->um_left;
      if (uncle && !uncle->um_black) {
	dad->um_black = 1;
	uncle->um_black = 1;
	granddad->um_black = 0;
	um = granddad;
      } else {
	if (um == dad->um_left) {
	  right_rotate(top, um = dad);
	  dad = um->um_dad; assert(dad);
	  granddad = dad->um_dad; assert(granddad);
	}
	dad->um_black = 1;
	granddad->um_black = 0;
	left_rotate(top, granddad);
      }
    }
  }

  assert(*top);

  (*top)->um_black = 1;
}

static
void red_black_balance_delete(UrlMap **top, UrlMap *um)
{
  UrlMap *dad, *brother;

  for (dad = um->um_dad; um != *top && !dad->um_black; dad = um->um_dad) {
    if (um == dad->um_left) {
      brother = dad->um_right;

      if (!brother) {
	um = dad;
	continue;
      }

      assert(brother->um_black);
      if ((!brother->um_left || brother->um_left->um_black) &&
	  (!brother->um_right || brother->um_right->um_black)) {
	brother->um_black = 0;
	um = dad;
	continue;
      }

      if (!brother->um_right || brother->um_right->um_black) {
	brother->um_left->um_black = 1;
	brother->um_black = 0;
	right_rotate(top, brother);
	brother = dad->um_right;
      }

      brother->um_black = dad->um_black;
      dad->um_black = 1;
      if (brother->um_right)
	brother->um_right->um_black = 1;
      left_rotate(top, dad);
      um = *top;
      break;
    } else {
      assert(um == dad->um_right);
      brother = dad->um_left;

      if (!brother) {
	um = dad;
	continue;
      }

      assert(brother->um_black);

      if ((!brother->um_left || brother->um_left->um_black) &&
	  (!brother->um_right || brother->um_right->um_black)) {
	brother->um_black = 0;
	um = dad;
	continue;
      }

      if (!brother->um_left || brother->um_left->um_black) {
	brother->um_right->um_black = 1;
	brother->um_black = 0;
	left_rotate(top, brother);
	brother = dad->um_left;
      }

      brother->um_black = um->um_dad->um_black;
      um->um_dad->um_black = 1;
      if (brother->um_left)
	brother->um_left->um_black = 1;
      right_rotate(top, dad);
      um = *top;
      break;
    }
  }

  um->um_black = 1;
}

/** Compare paths. */
su_inline
int urlmap_pathcmp(url_t const *a, url_t const *b, int *return_hostmatch)
{
  int retval;

  retval = url_cmp(a, b);

  *return_hostmatch = !retval;

  if (retval)
    return retval;
  else
    return strcmp(a->url_path, b->url_path);
}

/** Insert URL into map. */
int url_map_insert(UrlMap ** const tree,
		   UrlMap * const um,
		   UrlMap **return_old)
{
  UrlMap *old, *dad, **tslot;
  url_t *u;
  int cmp, hostmatch;

  if (tree == NULL || um == NULL || um->um_inserted)
    return (errno = EINVAL), -1;

  u = um->um_url;

  /* Insert into red-black binary tree */

  tslot = tree;

  for (old = *tree, dad = NULL; old; old = *tslot) {
    cmp = urlmap_pathcmp(u, old->um_url, &hostmatch);
    if (cmp < 0)
      dad = old, tslot = &old->um_left;
    else if (cmp > 0)
      dad = old, tslot = &old->um_right;
    else
      break;
  }

  assert(old != um);

  if (old) {
    if ((um->um_left = old->um_left))
      um->um_left->um_dad = um;
    if ((um->um_right = old->um_right))
      um->um_right->um_dad = um;

    if (!(um->um_dad = old->um_dad))
      *tree = um;
    else if (um->um_dad->um_left == old)
      um->um_dad->um_left = um;
    else assert(um->um_dad->um_right == old),
      um->um_dad->um_right = um;

    um->um_black = old->um_black;

    old->um_left = NULL;
    old->um_right = NULL;
    old->um_dad = NULL;
    old->um_inserted = 0;
  } else {
    *tslot = um;
    um->um_dad = dad;
    if (tree != tslot) {
      red_black_balance_insert(tree, um);
    } else {
      um->um_black = 1;
    }
  }

  um->um_inserted = 1;

  if (return_old)
    *return_old = old;

  return 0;
}

/** Find a URL */
UrlMap *
url_map_find(UrlMap *root,
	     url_string_t const *url,
	     int relative)
{
  UrlMap *um, *maybe = NULL;
  url_t u[1];
  void *tbf = NULL;
  char *end;
  int cmp, hostmatch;

  if (root == NULL || url == NULL)
    return NULL;

  url = tbf = url_hdup(NULL, (url_t *)url);
  if (!url)
    return NULL;

  *u = *url->us_url;
  if (!u->url_path)
    u->url_path = "";

  for (um = root; um; um = cmp < 0 ? um->um_left : um->um_right) {
    cmp = urlmap_pathcmp(u, um->um_url, &hostmatch);
    if (cmp == 0)
      break;
    if (hostmatch && !maybe)
      maybe = um;
  }

  while (!um && relative && u->url_path[0]) {
    end = strrchr(u->url_path, '/');
    end = end ? end + 1 : (char *)u->url_path;
    if (*end)
      *end = '\0';
    for (um = maybe; um; um = cmp < 0 ? um->um_left : um->um_right) {
      if ((cmp = urlmap_pathcmp(u, um->um_url, &hostmatch)) == 0)
	break;
    }
  }

  su_free(NULL, tbf);

  return um;
}

/** Remove URL. */
void url_map_remove(UrlMap **top, UrlMap *um)
{
  UrlMap *kid, *dad;
  int need_to_balance;

  if (top == NULL || um == NULL || !um->um_inserted)
    return;

  /* Make sure that node is in tree */
  for (dad = um; dad; dad = dad->um_dad)
    if (dad == *top)
      break;

  assert(dad);
  if (!dad)
    return;

  /* Find a successor node with a free branch */
  if (!um->um_left || !um->um_right)
    dad = um;
  else for (dad = um->um_right; dad->um_left; dad = dad->um_left)
    ;
  /* Dad has a free branch => kid is dad's only child */
  kid = dad->um_left ? dad->um_left : dad->um_right;

  /* Remove dad from tree */
  if (!(dad->um_dad))
    *top = kid;
  else if (dad->um_dad->um_left == dad)
    dad->um_dad->um_left = kid;
  else assert(dad->um_dad->um_right == dad),
    dad->um_dad->um_right = kid;
  if (kid)
    kid->um_dad = dad->um_dad;

  need_to_balance = kid && dad->um_black;

  /* Put dad in place of um */
  if (um != dad) {
    if (!(dad->um_dad = um->um_dad))
      *top = dad;
    else if (dad->um_dad->um_left == um)
      dad->um_dad->um_left = dad;
    else assert(dad->um_dad->um_right == um),
      dad->um_dad->um_right = dad;

    dad->um_black = um->um_black;

    if ((dad->um_left = um->um_left))
      dad->um_left->um_dad = dad;

    if ((dad->um_right = um->um_right))
      dad->um_right->um_dad = dad;
  }

  um->um_left = NULL;
  um->um_right = NULL;
  um->um_dad = NULL;
  um->um_black = 0;
  um->um_inserted = 0;

  if (need_to_balance)
    red_black_balance_delete(top, kid);
}

#if TEST_URLMAP
/* Functions currently used only by test cases */

/** Return inorder successor of node @a um. */
UrlMap *url_map_succ(UrlMap *um)
{
  UrlMap *dad;

  if (um->um_right) {
    for (um = um->um_right; um->um_left; um = um->um_left)
      ;
    return um;
  }

  for (dad = um->um_dad; dad && um == dad->um_right; dad = um->um_dad)
    um = dad;

  return dad;
}

/** Return inorder precedessor of node @a um. */
UrlMap *url_map_prec(UrlMap *um)
{
  UrlMap *dad;

  if (um->um_left) {
    for (um = um->um_left; um->um_right; um = um->um_right)
      ;
    return um;
  }

  for (dad = um->um_dad; dad && um == dad->um_left; dad = um->um_dad)
    um = dad;

  return dad;
}

/** Return first node in tree @a um. */
UrlMap *url_map_first(UrlMap *um)
{
  while (um && um->um_left)
    um = um->um_left;

  return um;
}

/** Return last node in tree @a um. */
UrlMap *url_map_last(UrlMap *um)
{
  while (um && um->um_right)
    um = um->um_right;

  return um;
}

/** Return height of the tree */
int url_map_height(UrlMap const *tree)
{
  int left, right;

  if (!tree)
    return 0;

  left = tree->um_left ? url_map_height(tree->um_left) : 0;
  right = tree->um_right ? url_map_height(tree->um_right) : 0;

  if (left > right)
    return left + 1;
  else
    return right + 1;
}

/** Check consistency */
static
int redblack_check(UrlMap const *n)
{
  UrlMap const *l, *r;

  if (!n)
    return 1;

  l = n->um_left, r = n->um_right;

  if (n->um_black || ((!l || l->um_black) && (!r || r->um_black)))
    return (!l || redblack_check(l)) && (!r || redblack_check(r));
  else
    return 0;
}

/* Testing functions */

int tstflags;

#define TSTFLAGS tstflags

#include <stdio.h>
#include <sofia-sip/tstdef.h>

char const *name = "test_urlmap";

int test_path(void)
{
  su_home_t *home;
  UrlMap *tree = NULL, *o;
  UrlMap *um, *um1, *um2, *um3;

  BEGIN();

  home = su_home_clone(NULL, sizeof(*home)); TEST_1(home);

  um1 = url_map_new(home, (void*)"http://host/aa/", sizeof *um1);
  TEST_1(um1);

  um2 = url_map_new(home, (void*)"http://host/aa/bb/", sizeof *um1);
  TEST_1(um2);

  um3 = url_map_new(home, (void*)"http://host/aa/bb/cc/",
				sizeof *um);
  TEST_1(um3);

  TEST_1(um1 != um2 && um1 != um3 && um2 != um3);

  o = (void *)-1;
  TEST(url_map_insert(&tree, um3, &o), 0);
  TEST_P(o, NULL); o = (void *)-1;
  TEST(url_map_insert(&tree, um2, &o), 0);
  TEST_P(o, NULL); o = (void *)-1;
  TEST(url_map_insert(&tree, um1, &o), 0);
  TEST_P(o, NULL);

  um = url_map_find(tree, (void*)"http://host/aa/bb/cc", 1); TEST_P(um, um2);
  um = url_map_find(tree, (void*)"http://host/aa/bb/cc/oo", 1);
                                                          TEST_P(um, um3);
  um = url_map_find(tree, (void*)"http://host/aa/bb", 1); TEST_P(um, um1);
  um = url_map_find(tree, (void*)"http://host/aa/bb", 0); TEST_P(um, NULL);
  um = url_map_find(tree, (void*)"http://host/aa/bb/", 1); TEST_P(um, um2);

  su_home_check(home);
  su_home_zap(home);

  END();
}

int test_insert(void)
{
  su_home_t *home;
  UrlMap *tree = NULL, *o, *old;
  UrlMap *one, *three, *five, *six, *seven;

  BEGIN();

  home = su_home_clone(NULL, sizeof(*home)); TEST_1(home);
  one = url_map_new(home, (void*)"/1", sizeof (UrlMap));
  three = url_map_new(home, (void*)"/3", sizeof (UrlMap));
  five = url_map_new(home, (void*)"/5", sizeof (UrlMap));
  six = url_map_new(home, (void*)"/6", sizeof (UrlMap));
  seven = url_map_new(home, (void*)"/7", sizeof (UrlMap));

  TEST_1(one);
  TEST_1(three);
  TEST_1(five);
  TEST_1(six);
  TEST_1(seven);

  /* Check single node */
  TEST(url_map_insert(&tree, five, &o), 0); TEST_P(o, NULL);
  TEST_P(tree, five);
  TEST_P(five->um_left, NULL); TEST_P(five->um_right, NULL);
  TEST_P(five->um_dad, NULL); TEST(five->um_black, 1);

  /* Check after another node:
   *
   *         5b
   *        /
   *       3r
   */
  TEST(url_map_insert(&tree, three, &o), 0); TEST_P(o, NULL);
  TEST_P(tree->um_left, three); TEST(tree->um_black, 1);
  TEST_P(three->um_left, NULL); TEST_P(three->um_right, NULL);
  TEST_P(three->um_dad, tree); TEST(three->um_black, 0);

  /* Check third node
   *         5b
   *        / \
   *       3r  7r
   */
  TEST(url_map_insert(&tree, seven, &o), 0); TEST_P(o, NULL);
  TEST_P(tree->um_right, seven); TEST(tree->um_black, 1);
  TEST_P(seven->um_left, NULL); TEST_P(seven->um_right, NULL);
  TEST_P(seven->um_dad, tree); TEST(seven->um_black, 0);

  /* Check after fourth node:
   *         5b
   *        / \
   *       3b  7b
   *      /
   *     1r
   */
  TEST(url_map_insert(&tree, one, &o), 0); TEST_P(o, NULL);
  TEST_P(tree->um_left->um_left, one);
  TEST(tree->um_black, 1);
  TEST(tree->um_left->um_black, 1); TEST(tree->um_right->um_black, 1);
  TEST_P(one->um_left, NULL); TEST_P(one->um_right, NULL);
  TEST_P(one->um_dad, tree->um_left); TEST(one->um_black, 0);

  /* Checks that we got after fifth node:
   *         5b
   *        / \
   *       3b  7b
   *      /   /
   *     1r  6r
   */
  TEST(url_map_insert(&tree, six, &o), 0); TEST_P(o, NULL);
  TEST_P(tree, five); TEST(five->um_black, 1);
  TEST_P(tree->um_left, three); TEST(three->um_black, 1);
  TEST_P(tree->um_left->um_left, one); TEST(one->um_black, 0);
  TEST_P(tree->um_right, seven); TEST(seven->um_black, 1);
  TEST_P(tree->um_right->um_left, six); TEST(six->um_black, 0);

  /* Insert five second time */
  old = five;
  five = url_map_new(home, (void*)"/5", sizeof (UrlMap));
  TEST(url_map_insert(&tree, five, &o), 0); TEST_P(o, old);
  TEST_P(tree, five); TEST(five->um_black, 1);
  TEST_P(tree->um_left, three); TEST(three->um_black, 1);
  TEST_P(three->um_dad, five);
  TEST_P(tree->um_left->um_left, one); TEST(one->um_black, 0);
  TEST_P(tree->um_right, seven); TEST(seven->um_black, 1);
  TEST_P(seven->um_dad, five);
  TEST_P(tree->um_right->um_left, six); TEST(six->um_black, 0);

  su_home_check(home);
  su_home_zap(home);

  END();
}

int test_rotate(void)
{
  su_home_t *home;
  UrlMap *tree = NULL;
  UrlMap *x, *y, *o;

  BEGIN();

  home = su_home_clone(NULL, sizeof(*home)); TEST_1(home);

  x = url_map_new(home, (void*)"/x", sizeof *x);
  y = url_map_new(home, (void*)"/y", sizeof *y);

  TEST_1(x);
  TEST_1(y);

  /*
   *              x                   y               x
   * Checks that   \  transforms to  /   and back to   \
   *                y               x                   y
   */
  TEST(url_map_insert(&tree, x, &o), 0); TEST_P(o, NULL);
  TEST(url_map_insert(&tree, y, &o), 0); TEST_P(o, NULL);

  TEST_P(tree, x); TEST_P(x->um_right, y);
  left_rotate(&tree, x);
  TEST_P(tree, y); TEST_P(y->um_left, x);
  right_rotate(&tree, y);
  TEST_P(tree, x); TEST_P(x->um_right, y);

  su_home_check(home);
  su_home_zap(home);

  END();
}

/** ceil of log2 */
static
unsigned log2ceil(unsigned k)
{
  unsigned result = 0;

#if 0
  if (k > (1 << 32))
    result += 32, k = (k >> 32) + ((k & ((1 << 32) - 1)) != 0);
#endif
  if (k > (1 << 16))
    result += 16, k = (k >> 16) + ((k & ((1 << 16) - 1)) != 0);
  if (k > (1 << 8))
    result += 8, k = (k >> 8) + ((k & ((1 << 8) - 1)) != 0);
  if (k > (1 << 4))
    result += 4, k = (k >> 4) + ((k & 15) != 0);
  if (k > (1 << 2))
    result += 2, k = (k >> 2) + ((k & 3) != 0);
  if (k > (1 << 1))
    result += 1, k = (k >> 1) + (k & 1);
  if (k > 1)
    result += 1;

  return result;
}

typedef struct {
  UrlMap te_urlmap[1];
  int      te_value;
  int      te_inserted;
} TEntry;

int test_balance(void)
{
  su_home_t *home;
  UrlMap *tree = NULL, *o = NULL;
  url_t *u;
  TEntry *te, **nodes;
  char path[16];
  int i, j;
  int const N = 1000;

  BEGIN();

  home = su_home_clone(NULL, sizeof(*home)); TEST_1(home);
  nodes = su_zalloc(home, (N + 2) * (sizeof *nodes)); TEST_1(nodes);
  nodes++;

  u = url_hdup(home, (url_t *)"http://host");
  u->url_path = path;

  for (i = 0; i < N; i++) {
    snprintf(path, (sizeof path), "p%07u", i);
    te = (TEntry *)url_map_new(home, (void *)u, sizeof *te);
    te->te_value = i;
    nodes[i] = te;
    TEST(url_map_insert(&tree, te->te_urlmap, &o), 0);
    TEST_P(o, NULL);
    TEST_1(url_map_height(tree) <= 2 * log2ceil(i + 1 + 1));
    TEST_1(redblack_check(tree));
  }

  for (i = 0; i < N; i++) {
    snprintf(path, (sizeof path), "p%07u", i);
    te = (TEntry *)url_map_find(tree, (void*)u, 1);
    TEST_1(te); TEST(te->te_value, i);
  }

  snprintf(path, (sizeof path), "p%07u", 0);

  te = (TEntry *)url_map_find(tree, (void*)u, 1);

  for (i = 0; i < N; i++) {
    TEST_1(te); TEST(te->te_value, i);
    te = (TEntry *)url_map_succ(te->te_urlmap);
  }
  TEST_1(te == NULL);

  for (i = 0; i < N; i++) {
    TEST_P(url_map_succ(nodes[i]->te_urlmap), nodes[i + 1]->te_urlmap);
    TEST_P(url_map_prec(nodes[i]->te_urlmap), nodes[i - 1]->te_urlmap);
  }

  for (i = 0; i < N; i++) {
    snprintf(path, (sizeof path), "p%07u", i);
    te = (TEntry *)url_map_find(tree, (void*)u, 1);
    TEST_1(te); TEST(te->te_value, i);
    url_map_remove(&tree, te->te_urlmap);
    TEST_1(te->te_urlmap->um_dad == NULL &&
	   te->te_urlmap->um_left == NULL &&
	   te->te_urlmap->um_right == NULL);
    TEST_1(url_map_height(tree) <= 2 * log2ceil(N - i + 1));
    TEST_1(redblack_check(tree));
  }

  TEST_P(tree, NULL);

  for (i = N - 1; i >= 0; i--) {
    o = (void *)-1;
    TEST(url_map_insert(&tree, nodes[i]->te_urlmap, &o), 0);
    TEST_P(o, NULL);
    TEST_1(url_map_height(tree) <= 2 * log2ceil(N - i + 1));
    TEST_1(redblack_check(tree));
  }

  for (i = 0; i < N; i++) {
    TEST_P(url_map_succ(nodes[i]->te_urlmap), nodes[i + 1]->te_urlmap);
    TEST_P(url_map_prec(nodes[i]->te_urlmap), nodes[i - 1]->te_urlmap);
  }

  for (i = 0; i < N; i++) {
    url_map_remove(&tree, nodes[i]->te_urlmap);
    TEST_1(url_map_height(tree) <= 2 * log2ceil(N - i + 1));
    TEST_1(redblack_check(tree));
  }

  TEST_P(tree, NULL);

  for (i = 0; i < N; i++) {
    int sn = (i * 57) % N;
    o = (void *)-1;
    TEST(nodes[sn]->te_inserted, 0);
    TEST(url_map_insert(&tree, nodes[sn]->te_urlmap, &o), 0);
    nodes[sn]->te_inserted = 1;
    TEST_P(o, NULL);
    TEST_1(url_map_height(tree) <= 2 * log2ceil(i + 1 + 1));
    TEST_1(redblack_check(tree));
  }

  for (i = 0; i < N; i++) {
    TEST(nodes[i]->te_inserted, 1);
    TEST_P(url_map_succ(nodes[i]->te_urlmap), nodes[i + 1]->te_urlmap);
    TEST_P(url_map_prec(nodes[i]->te_urlmap), nodes[i - 1]->te_urlmap);
  }

  for (i = 0; i < N; i++) {
    int sn = (i * 23) % N;	/* 23 is relative prime to N */
    TEST(nodes[sn]->te_inserted, 1);
    url_map_remove(&tree, nodes[sn]->te_urlmap);
    nodes[sn]->te_inserted = 0;
    TEST_1(url_map_height(tree) <= 2 * log2ceil(N - i + 1));
    TEST_1(redblack_check(tree));
  }

  TEST_P(tree, NULL);

  for (i = 0; i < N; i++) {
    int sn = (i * 517) % N;	/* relative prime to N */
    o = (void *)-1;
    TEST(nodes[sn]->te_inserted, 0);
    TEST(url_map_insert(&tree, nodes[sn]->te_urlmap, &o), 0);
    nodes[sn]->te_inserted = 1;
    TEST_P(o, NULL);
    TEST_1(url_map_height(tree) <= 2 * log2ceil(i + 1 + 1));
    TEST_1(redblack_check(tree));
  }

  for (i = 0; i < N; i++) {
    TEST(nodes[i]->te_inserted, 1);
    TEST_P(url_map_succ(nodes[i]->te_urlmap), nodes[i + 1]->te_urlmap);
    TEST_P(url_map_prec(nodes[i]->te_urlmap), nodes[i - 1]->te_urlmap);
  }

  for (i = 0; i < N; i++) {
    int sn = (i * 497) % N;	/* relative prime to N */
    TEST(nodes[sn]->te_inserted, 1);
    url_map_remove(&tree, nodes[sn]->te_urlmap);
    nodes[sn]->te_inserted = 0;
    TEST_1(url_map_height(tree) <= 2 * log2ceil(N - i + 1));
    TEST_1(redblack_check(tree));
  }

  TEST_P(tree, NULL);

  for (i = 0; i < N; i++) {
    int sn = (i * 1957) % N;	/* relative prime to N */
    o = (void *)-1;
    TEST(nodes[sn]->te_inserted, 0);
    TEST(url_map_insert(&tree, nodes[sn]->te_urlmap, &o), 0);
    nodes[sn]->te_inserted = 1;
    TEST_P(o, NULL);
    TEST_1(url_map_height(tree) <= 2 * log2ceil(i + 1 + 1));
    TEST_1(redblack_check(tree));
  }

  for (i = 0; i < N; i++) {
    TEST(nodes[i]->te_inserted, 1);
    TEST_P(url_map_succ(nodes[i]->te_urlmap), nodes[i + 1]->te_urlmap);
    TEST_P(url_map_prec(nodes[i]->te_urlmap), nodes[i - 1]->te_urlmap);
  }

  for (i = 0; i < N; i++) {
    int sn = (i * 1519) % N;	/* relative prime to N */
    TEST(nodes[sn]->te_inserted, 1);
    url_map_remove(&tree, nodes[sn]->te_urlmap);
    nodes[sn]->te_inserted = 0;
    TEST_1(url_map_height(tree) <= 2 * log2ceil(N - i + 1));
    TEST_1(redblack_check(tree));
  }

  TEST_P(tree, NULL);

  /* Insert small, big, small, big ... */

  for (i = 0; i < N / 2; i++) {
    int sn = N - i - 1;
    TEST(nodes[i]->te_inserted, 0);
    o = (void *)-1;
    TEST(url_map_insert(&tree, nodes[i]->te_urlmap, &o), 0);
    TEST_P(o, NULL);
    nodes[i]->te_inserted = 1;

    TEST(nodes[sn]->te_inserted, 0);
    o = (void *)-1;
    TEST(url_map_insert(&tree, nodes[sn]->te_urlmap, &o), 0);
    TEST_P(o, NULL);
    nodes[sn]->te_inserted = 1;
  }

  for (i = 0; i < N; i++) {
    TEST(nodes[i]->te_inserted, 1);
    TEST_P(url_map_succ(nodes[i]->te_urlmap), nodes[i + 1]->te_urlmap);
    TEST_P(url_map_prec(nodes[i]->te_urlmap), nodes[i - 1]->te_urlmap);
  }

  for (i = 0; i < N; i++) {
    te = (TEntry *)((i & 1) ? url_map_succ(tree) : url_map_prec(tree));
    if (te == NULL)
      te = (TEntry *)tree;
    TEST(te->te_inserted, 1);
    url_map_remove(&tree, te->te_urlmap);
    te->te_inserted = 0;
    TEST_1(url_map_height(tree) <= 2 * log2ceil(N - i + 1));
    TEST_1(redblack_check(tree));
  }

  TEST_P(tree, NULL);

  /* Insert small, big, small, big ... */

  for (i = 0; i < N / 2; i++) {
    int sn = N - i - 1;
    TEST(nodes[i]->te_inserted, 0);
    o = (void *)-1;
    TEST(url_map_insert(&tree, nodes[i]->te_urlmap, &o), 0);
    TEST_P(o, NULL);
    nodes[i]->te_inserted = 1;

    TEST(nodes[sn]->te_inserted, 0);
    o = (void *)-1;
    TEST(url_map_insert(&tree, nodes[sn]->te_urlmap, &o), 0);
    TEST_P(o, NULL);
    nodes[sn]->te_inserted = 1;
  }

  for (i = 0; i < N; i++) {
    TEST(nodes[i]->te_inserted, 1);
    TEST_P(url_map_succ(nodes[i]->te_urlmap), nodes[i + 1]->te_urlmap);
    TEST_P(url_map_prec(nodes[i]->te_urlmap), nodes[i - 1]->te_urlmap);
  }

  /* Remove last, first, last, first, ... */
  for (i = 0; i < N; i++) {
    te = (TEntry *)((i & 1) ? url_map_first(tree) : url_map_last(tree));
    TEST_1(te);
    TEST(te->te_inserted, 1);
    url_map_remove(&tree, te->te_urlmap);
    te->te_inserted = 0;
    TEST_1(url_map_height(tree) <= 2 * log2ceil(N - i + 1));
    TEST_1(redblack_check(tree));
  }

  TEST_P(tree, NULL);

  /* Insert small, big, small, big ... */

  for (i = 0; i < N / 2; i++) {
    int sn = N / 2 + i;
    TEST(nodes[i]->te_inserted, 0);
    o = (void *)-1;
    TEST(url_map_insert(&tree, nodes[i]->te_urlmap, &o), 0);
    TEST_P(o, NULL);
    nodes[i]->te_inserted = 1;

    TEST(nodes[sn]->te_inserted, 0);
    o = (void *)-1;
    TEST(url_map_insert(&tree, nodes[sn]->te_urlmap, &o), 0);
    TEST_P(o, NULL);
    nodes[sn]->te_inserted = 1;
  }

  for (i = 0; i < N; i++) {
    TEST(nodes[i]->te_inserted, 1);
    TEST_P(url_map_succ(nodes[i]->te_urlmap), nodes[i + 1]->te_urlmap);
    TEST_P(url_map_prec(nodes[i]->te_urlmap), nodes[i - 1]->te_urlmap);
  }

  /* Remove last, first, last, first, ... */
  for (i = 0; i < N; i++) {
    te = (TEntry *)((i & 1) ? url_map_first(tree) : url_map_last(tree));
    TEST_1(te);
    TEST(te->te_inserted, 1);
    url_map_remove(&tree, te->te_urlmap);
    te->te_inserted = 0;
    TEST_1(url_map_height(tree) <= 2 * log2ceil(N - i + 1));
    TEST_1(redblack_check(tree));
  }

  TEST_P(tree, NULL);

  /* Insert in perfect order ... */

  for (j = N / 2; j > 0; j /= 2) {
    for (i = N - j; i >= 0; i -= j) {
      if (nodes[i]->te_inserted)
	continue;
      o = (void *)-1;
      TEST(url_map_insert(&tree, nodes[i]->te_urlmap, &o), 0);
      TEST_P(o, NULL);
      nodes[i]->te_inserted = 1;
    }
  }

  for (i = 0; i < N; i++) {
    TEST(nodes[i]->te_inserted, 1);
    TEST_P(url_map_succ(nodes[i]->te_urlmap), nodes[i + 1]->te_urlmap);
    TEST_P(url_map_prec(nodes[i]->te_urlmap), nodes[i - 1]->te_urlmap);
  }

  /* Remove such nodes that insert red uncles into tree */
  for (i = 0; i < N; i++) {
    te = (TEntry *)url_map_last(tree);
    for (o = te->te_urlmap; o; o = url_map_prec(o)) {
      UrlMap *dad, *granddad, *uncle, *to_be_removed;
      /* We must have a node with black dad, no brother, red granddad and uncle */
      if (!(dad = o->um_dad) || !dad->um_black)
	continue;
      if (dad->um_left && dad->um_right)
	continue;
      if (!(granddad = dad->um_dad) || granddad->um_black)
	continue;
      if (granddad->um_left == dad)
	uncle = granddad->um_right;
      else
	uncle = granddad->um_left;
      if (!uncle || uncle->um_black)
	continue;
      to_be_removed = url_map_prec(o->um_dad);
      if (to_be_removed == granddad || to_be_removed == uncle)
	continue;
      if (!to_be_removed->um_left || !to_be_removed->um_right)
	continue;
      te = (TEntry *)to_be_removed;
      break;
    }
    TEST(te->te_inserted, 1);
    url_map_remove(&tree, te->te_urlmap);
    te->te_inserted = 0;
    TEST_1(url_map_height(tree) <= 2 * log2ceil(N - i + 1));
    TEST_1(redblack_check(tree));
  }

  TEST_P(tree, NULL);

  su_home_check(home);
  su_home_zap(home);

  END();
}

int test_speed(void)
{
  su_home_t *home;
  UrlMap *tree = NULL, *o = NULL;
  url_t *u;
  TEntry *te;
  unsigned i;
  char path[16];
  int const N = 1000000;

  BEGIN();

  home = su_home_clone(NULL, sizeof(*home)); TEST_1(home);

  u = url_hdup(home, (url_t *)"http://host");
  u->url_path = path;

  for (i = 0; i < N; i++) {
    snprintf(path, (sizeof path), "p%07u", i);
    te = (TEntry *)url_map_new(home, (void *)u, sizeof *te);
    te->te_value = i;
    TEST(url_map_insert(&tree, te->te_urlmap, &o), 0);
    TEST_P(o, NULL);
  }

  TEST_1(url_map_height(tree) <= 2 * log2ceil(i + 1));

  for (i = 0; i < N; i++) {
    snprintf(path, (sizeof path), "p%07u", i);
    te = (TEntry *)url_map_find(tree, (void*)u, 1);
    TEST_1(te); TEST(te->te_value, i);
  }

  snprintf(path, (sizeof path), "p%07u", 0);

  te = (TEntry *)url_map_find(tree, (void*)u, 1);

  for (i = 0; i < N; i++) {
    TEST_1(te); TEST(te->te_value, i);
    te = (TEntry *)url_map_succ(te->te_urlmap);
  }
  TEST_1(te == NULL);

  su_home_check(home);
  su_home_zap(home);

  END();
}



void usage(void)
{
  fprintf(stderr,
	  "usage: %s [-v]\n",
	  name);
}

int main(int argc, char *argv[])
{
  int retval = 0;
  int i;

  for (i = 1; argv[i]; i++) {
    if (strcmp(argv[i], "-v") == 0)
      tstflags |= tst_verbatim;
    else
      usage();
  }

  retval |= test_insert(); fflush(stdout);
  retval |= test_rotate(); fflush(stdout);
  retval |= test_path(); fflush(stdout);
  retval |= test_balance(); fflush(stdout);

  return retval;
}

#endif

