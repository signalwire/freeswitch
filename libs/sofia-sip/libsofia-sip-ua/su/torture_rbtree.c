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
 * @file torture_rbtree.c
 * @brief Test red-black tree
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

#include "sofia-sip/rbtree.h"

typedef struct node Node;

struct node {
  Node *left, *right, *parent;
  int black;
  int value;
  int inserted;
};

int tstflags;

#define TSTFLAGS tstflags

#include <stdio.h>
#include <sofia-sip/tstdef.h>

char const *name = "torture_rbtree";

/* Define accessor macros */
#define LEFT(node) ((node)->left)
#define RIGHT(node) ((node)->right)
#define PARENT(node) ((node)->parent)
#define SET_RED(node) ((node)->black = 0)
#define SET_BLACK(node) ((node)->black = 1)
#define CMP(a, b) ((a)->value - (b)->value)
#define IS_RED(node) ((node) && (node)->black == 0)
#define IS_BLACK(node) (!(node) || (node)->black == 1)
#define COPY_COLOR(dst, src) ((dst)->black = (src)->black)
#define INSERT(node) ((void)0)
#define REMOVE(node) ((node)->left = (node)->right = (node)->parent = NULL)

RBTREE_PROTOS(su_inline, redblack, Node);

RBTREE_BODIES(su_inline, redblack, Node, LEFT, RIGHT, PARENT,
	      IS_RED, SET_RED, IS_BLACK, SET_BLACK, COPY_COLOR,
	      CMP, INSERT, REMOVE);

#include <sofia-sip/su_alloc.h>

static
Node *node_new(su_home_t *home, int value)
{
  Node *n = su_zalloc(home, sizeof (*n));

  n->value = value;

  return n;
}

/** ceil of log2 */
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

static
Node *node_find(Node *tree, int value)
{
  while (tree) {
    if (tree->value == value)
      break;
    else if (tree->value < value)
      tree = tree->right;
    else
      tree = tree->left;
  }

  return tree;
}

/** Check consistency */
static
int redblack_check(Node const *n)
{
  Node const *l, *r;

  if (!n)
    return 1;

  l = n->left, r = n->right;

  if (n->black || ((!l || l->black) && (!r || r->black)))
    return (!l || redblack_check(l)) && (!r || redblack_check(r));
  else
    return 0;
}

int test_insert(void)
{
  su_home_t *home;
  Node *tree = NULL, *o, *old;
  Node *one, *three, *five, *six, *seven;

  BEGIN();

  home = su_home_clone(NULL, sizeof(*home)); TEST_1(home);
  one = node_new(home, 1);
  three = node_new(home, 3);
  five = node_new(home, 5);
  six = node_new(home, 6);
  seven = node_new(home, 7);

  TEST_1(one);
  TEST_1(three);
  TEST_1(five);
  TEST_1(six);
  TEST_1(seven);

  /* Check single node */
  TEST(redblack_insert(&tree, five, &o), 0); TEST_P(o, NULL);
  TEST_P(tree, five);
  TEST_P(five->left, NULL); TEST_P(five->right, NULL);
  TEST_P(five->parent, NULL); TEST(five->black, 1);

  /* Check after another node:
   *
   *         5b
   *        /
   *       3r
   */
  TEST(redblack_insert(&tree, three, &o), 0); TEST_P(o, NULL);
  TEST_P(tree->left, three); TEST(tree->black, 1);
  TEST_P(three->left, NULL); TEST_P(three->right, NULL);
  TEST_P(three->parent, tree); TEST(three->black, 0);

  /* Check third node
   *         5b
   *        / \
   *       3r  7r
   */
  TEST(redblack_insert(&tree, seven, &o), 0); TEST_P(o, NULL);
  TEST_P(tree->right, seven); TEST(tree->black, 1);
  TEST_P(seven->left, NULL); TEST_P(seven->right, NULL);
  TEST_P(seven->parent, tree); TEST(seven->black, 0);

  /* Check after fourth node:
   *         5b
   *        / \
   *       3b  7b
   *      /
   *     1r
   */
  TEST(redblack_insert(&tree, one, &o), 0); TEST_P(o, NULL);
  TEST_P(tree->left->left, one);
  TEST(tree->black, 1);
  TEST(tree->left->black, 1); TEST(tree->right->black, 1);
  TEST_P(one->left, NULL); TEST_P(one->right, NULL);
  TEST_P(one->parent, tree->left); TEST(one->black, 0);

  /* Checks that we got after fifth node:
   *         5b
   *        / \
   *       3b  7b
   *      /   /
   *     1r  6r
   */
  TEST(redblack_insert(&tree, six, &o), 0); TEST_P(o, NULL);
  TEST_P(tree, five); TEST(five->black, 1);
  TEST_P(tree->left, three); TEST(three->black, 1);
  TEST_P(tree->left->left, one); TEST(one->black, 0);
  TEST_P(tree->right, seven); TEST(seven->black, 1);
  TEST_P(tree->right->left, six); TEST(six->black, 0);

  /* Insert five second time */
  old = five;
  five = node_new(home, 5);
  TEST(redblack_insert(&tree, five, &o), 0); TEST_P(o, old);
  TEST_P(tree, five); TEST(five->black, 1);
  TEST_P(tree->left, three); TEST(three->black, 1);
  TEST_P(three->parent, five);
  TEST_P(tree->left->left, one); TEST(one->black, 0);
  TEST_P(tree->right, seven); TEST(seven->black, 1);
  TEST_P(seven->parent, five);
  TEST_P(tree->right->left, six); TEST(six->black, 0);

  su_home_check(home);
  su_home_zap(home);

  END();
}

int test_rotate(void)
{
  su_home_t *home;
  Node *tree = NULL;
  Node *x, *y, *o;

  BEGIN();

  home = su_home_clone(NULL, sizeof(*home)); TEST_1(home);

  x = node_new(home, 1);
  y = node_new(home, 2);

  TEST_1(x);
  TEST_1(y);

  /*
   *              x                   y               x
   * Checks that   \  transforms to  /   and back to   \
   *                y               x                   y
   */
  TEST(redblack_insert(&tree, x, &o), 0); TEST_P(o, NULL);
  TEST(redblack_insert(&tree, y, &o), 0); TEST_P(o, NULL);

  TEST_P(tree, x); TEST_P(x->right, y);
  redblack_left_rotate(&tree, x);
  TEST_P(tree, y); TEST_P(y->left, x);
  redblack_right_rotate(&tree, y);
  TEST_P(tree, x); TEST_P(x->right, y);

  su_home_check(home);
  su_home_zap(home);

  END();
}

int test_simple(void)
{
  su_home_t *home;
  Node *tree = NULL, *o;
  int i;
  Node *um, *um103, *um497, *um995;

  BEGIN();

  home = su_home_clone(NULL, sizeof(*home)); TEST_1(home);

  for (i = 3; i < 1000; i += 3) {
    um = node_new(home, i); TEST_1(um);
    o = (void *)-1; TEST(redblack_insert(&tree, um, &o), 0); TEST_P(o, NULL);
  }

  um103 = node_new(home, 103); TEST_1(um103);
  um497 = node_new(home, 497); TEST_1(um497);
  um995 = node_new(home, 995); TEST_1(um995);

  o = (void *)-1; TEST(redblack_insert(&tree, um995, &o), 0); TEST_P(o, NULL);
  o = (void *)-1; TEST(redblack_insert(&tree, um497, &o), 0); TEST_P(o, NULL);
  o = (void *)-1; TEST(redblack_insert(&tree, um103, &o), 0); TEST_P(o, NULL);

  um = node_find(tree, 103); TEST_P(um, um103);
  um = node_find(tree, 497); TEST_P(um, um497);
  um = node_find(tree, 995); TEST_P(um, um995);
  um = node_find(tree, 994); TEST_P(um, NULL);
  um = node_find(tree, 1); TEST_P(um, NULL);

  su_home_check(home);
  su_home_zap(home);

  END();
}

int test_balance(void)
{
  su_home_t *home;
  Node *tree = NULL, *o = NULL;
  Node *node, **nodes;
  int i, j;
  int const N = 1000;

  BEGIN();

  home = su_home_clone(NULL, sizeof(*home)); TEST_1(home);
  nodes = su_zalloc(home, (N + 2) * (sizeof *nodes)); TEST_1(nodes);
  nodes++;

  for (i = 0; i < N; i++) {
    nodes[i] = node = node_new(home, i); TEST_1(node);
    TEST(redblack_insert(&tree, node, &o), 0);
    TEST_P(o, NULL);
    TEST_1(redblack_height(tree) <= 2 * (int)log2ceil(i + 1 + 1));
    TEST_1(redblack_check(tree));
  }

  for (i = 0; i < N; i++) {
    node = node_find(tree, i);
    TEST_1(node);
    TEST(node->value, i);
    TEST_P(nodes[i], node);
  }

  node = node_find(tree, 0);

  for (i = 0; i < N; i++) {
    TEST_1(node); TEST(node->value, i);
    node = redblack_succ(node);
  }
  TEST_1(node == NULL);

  for (i = 0; i < N; i++) {
    TEST_P(redblack_succ(nodes[i]), nodes[i + 1]);
    TEST_P(redblack_prec(nodes[i]), nodes[i - 1]);
  }

  for (i = 0; i < N; i++) {
    node = node_find(tree, i);
    TEST_1(node); TEST(node->value, i);
    redblack_remove(&tree, node);
    TEST_1(node->parent == NULL &&
	   node->left == NULL &&
	   node->right == NULL);
    TEST_1(redblack_height(tree) <= 2 * (int)log2ceil(N - i + 1));
    TEST_1(redblack_check(tree));
  }

  TEST_P(tree, NULL);

  for (i = N - 1; i >= 0; i--) {
    o = (void *)-1;
    TEST(redblack_insert(&tree, nodes[i], &o), 0);
    TEST_P(o, NULL);
    TEST_1(redblack_height(tree) <= 2 * (int)log2ceil(N - i + 1));
    TEST_1(redblack_check(tree));
  }

  for (i = 0; i < N; i++) {
    TEST_P(redblack_succ(nodes[i]), nodes[i + 1]);
    TEST_P(redblack_prec(nodes[i]), nodes[i - 1]);
  }

  for (i = 0; i < N; i++) {
    redblack_remove(&tree, nodes[i]);
    TEST_1(redblack_height(tree) <= 2 * (int)log2ceil(N - i + 1));
    TEST_1(redblack_check(tree));
  }

  TEST_P(tree, NULL);

  for (i = 0; i < N; i++) {
    int sn = (i * 57) % N;
    o = (void *)-1;
    TEST(nodes[sn]->inserted, 0);
    TEST(redblack_insert(&tree, nodes[sn], &o), 0);
    nodes[sn]->inserted = 1;
    TEST_P(o, NULL);
    TEST_1(redblack_height(tree) <= 2 * (int)log2ceil(i + 1 + 1));
    TEST_1(redblack_check(tree));
  }

  for (i = 0; i < N; i++) {
    TEST(nodes[i]->inserted, 1);
    TEST_P(redblack_succ(nodes[i]), nodes[i + 1]);
    TEST_P(redblack_prec(nodes[i]), nodes[i - 1]);
  }

  for (i = 0; i < N; i++) {
    int sn = (i * 23) % N;	/* 23 is relative prime to N */
    TEST(nodes[sn]->inserted, 1);
    redblack_remove(&tree, nodes[sn]);
    nodes[sn]->inserted = 0;
    TEST_1(redblack_height(tree) <= 2 * (int)log2ceil(N - i + 1));
    TEST_1(redblack_check(tree));
  }

  TEST_P(tree, NULL);

  for (i = 0; i < N; i++) {
    int sn = (i * 517) % N;	/* relative prime to N */
    o = (void *)-1;
    TEST(nodes[sn]->inserted, 0);
    TEST(redblack_insert(&tree, nodes[sn], &o), 0);
    nodes[sn]->inserted = 1;
    TEST_P(o, NULL);
    TEST_1(redblack_height(tree) <= 2 * (int)log2ceil(i + 1 + 1));
    TEST_1(redblack_check(tree));
  }

  for (i = 0; i < N; i++) {
    TEST(nodes[i]->inserted, 1);
    TEST_P(redblack_succ(nodes[i]), nodes[i + 1]);
    TEST_P(redblack_prec(nodes[i]), nodes[i - 1]);
  }

  for (i = 0; i < N; i++) {
    int sn = (i * 497) % N;	/* relative prime to N */
    TEST(nodes[sn]->inserted, 1);
    redblack_remove(&tree, nodes[sn]);
    nodes[sn]->inserted = 0;
    TEST_1(redblack_height(tree) <= 2 * (int)log2ceil(N - i + 1));
    TEST_1(redblack_check(tree));
  }

  TEST_P(tree, NULL);

  for (i = 0; i < N; i++) {
    int sn = (i * 1957) % N;	/* relative prime to N */
    o = (void *)-1;
    TEST(nodes[sn]->inserted, 0);
    TEST(redblack_insert(&tree, nodes[sn], &o), 0);
    nodes[sn]->inserted = 1;
    TEST_P(o, NULL);
    TEST_1(redblack_height(tree) <= 2 * (int)log2ceil(i + 1 + 1));
    TEST_1(redblack_check(tree));
  }

  for (i = 0; i < N; i++) {
    TEST(nodes[i]->inserted, 1);
    TEST_P(redblack_succ(nodes[i]), nodes[i + 1]);
    TEST_P(redblack_prec(nodes[i]), nodes[i - 1]);
  }

  for (i = 0; i < N; i++) {
    int sn = (i * 1519) % N;	/* relative prime to N */
    TEST(nodes[sn]->inserted, 1);
    redblack_remove(&tree, nodes[sn]);
    nodes[sn]->inserted = 0;
    TEST_1(redblack_height(tree) <= 2 * (int)log2ceil(N - i + 1));
    TEST_1(redblack_check(tree));
  }

  TEST_P(tree, NULL);

  /* Insert small, big, small, big ... */

  for (i = 0; i < N / 2; i++) {
    int sn = N - i - 1;
    TEST(nodes[i]->inserted, 0);
    o = (void *)-1;
    TEST(redblack_insert(&tree, nodes[i], &o), 0);
    TEST_P(o, NULL);
    nodes[i]->inserted = 1;

    TEST(nodes[sn]->inserted, 0);
    o = (void *)-1;
    TEST(redblack_insert(&tree, nodes[sn], &o), 0);
    TEST_P(o, NULL);
    nodes[sn]->inserted = 1;
  }

  for (i = 0; i < N; i++) {
    TEST(nodes[i]->inserted, 1);
    TEST_P(redblack_succ(nodes[i]), nodes[i + 1]);
    TEST_P(redblack_prec(nodes[i]), nodes[i - 1]);
  }

  for (i = 0; i < N; i++) {
    node = ((i & 1) ? redblack_succ(tree) : redblack_prec(tree));
    if (node == NULL)
      node = tree;
    TEST(node->inserted, 1);
    redblack_remove(&tree, node);
    node->inserted = 0;
    TEST_1(redblack_height(tree) <= 2 * (int)log2ceil(N - i + 1));
    TEST_1(redblack_check(tree));
  }

  TEST_P(tree, NULL);

  /* Insert small, big, small, big ... */

  for (i = 0; i < N / 2; i++) {
    int sn = N - i - 1;
    TEST(nodes[i]->inserted, 0);
    o = (void *)-1;
    TEST(redblack_insert(&tree, nodes[i], &o), 0);
    TEST_P(o, NULL);
    nodes[i]->inserted = 1;

    TEST(nodes[sn]->inserted, 0);
    o = (void *)-1;
    TEST(redblack_insert(&tree, nodes[sn], &o), 0);
    TEST_P(o, NULL);
    nodes[sn]->inserted = 1;
  }

  for (i = 0; i < N; i++) {
    TEST(nodes[i]->inserted, 1);
    TEST_P(redblack_succ(nodes[i]), nodes[i + 1]);
    TEST_P(redblack_prec(nodes[i]), nodes[i - 1]);
  }

  /* Remove last, first, last, first, ... */
  for (i = 0; i < N; i++) {
    node = ((i & 1) ? redblack_first(tree) : redblack_last(tree));
    TEST_1(node);
    TEST(node->inserted, 1);
    redblack_remove(&tree, node);
    node->inserted = 0;
    TEST_1(redblack_height(tree) <= 2 * (int)log2ceil(N - i + 1));
    TEST_1(redblack_check(tree));
  }

  TEST_P(tree, NULL);

  /* Insert small, big, small, big ... */

  for (i = 0; i < N / 2; i++) {
    int sn = N / 2 + i;
    TEST(nodes[i]->inserted, 0);
    o = (void *)-1;
    TEST(redblack_insert(&tree, nodes[i], &o), 0);
    TEST_P(o, NULL);
    nodes[i]->inserted = 1;

    TEST(nodes[sn]->inserted, 0);
    o = (void *)-1;
    TEST(redblack_insert(&tree, nodes[sn], &o), 0);
    TEST_P(o, NULL);
    nodes[sn]->inserted = 1;
  }

  for (i = 0; i < N; i++) {
    TEST(nodes[i]->inserted, 1);
    TEST_P(redblack_succ(nodes[i]), nodes[i + 1]);
    TEST_P(redblack_prec(nodes[i]), nodes[i - 1]);
  }

  /* Remove last, first, last, first, ... */
  for (i = 0; i < N; i++) {
    node = ((i & 1) ? redblack_first(tree) : redblack_last(tree));
    TEST_1(node);
    TEST(node->inserted, 1);
    redblack_remove(&tree, node);
    node->inserted = 0;
    TEST_1(redblack_height(tree) <= 2 * (int)log2ceil(N - i + 1));
    TEST_1(redblack_check(tree));
  }

  TEST_P(tree, NULL);

  /* Insert in perfect order ... */

  for (j = N / 2; j > 0; j /= 2) {
    for (i = N - j; i >= 0; i -= j) {
      if (nodes[i]->inserted)
	continue;
      o = (void *)-1;
      TEST(redblack_insert(&tree, nodes[i], &o), 0);
      TEST_P(o, NULL);
      nodes[i]->inserted = 1;
    }
  }

  for (i = 0; i < N; i++) {
    TEST(nodes[i]->inserted, 1);
    TEST_P(redblack_succ(nodes[i]), nodes[i + 1]);
    TEST_P(redblack_prec(nodes[i]), nodes[i - 1]);
  }

  /* Remove such nodes that inserts red uncles into tree */
  for (i = 0; i < N; i++) {
    node = redblack_last(tree);
    for (o = node; o; o = redblack_prec(o)) {
      Node *dad, *granddad, *uncle, *to_be_removed;
      /* We must have a node with black dad, no brother, red granddad and uncle */
      if (!(dad = o->parent) || !dad->black)
	continue;
      if (dad->left && dad->right)
	continue;
      if (!(granddad = dad->parent) || granddad->black)
	continue;
      if (granddad->left == dad)
	uncle = granddad->right;
      else
	uncle = granddad->left;
      if (!uncle || uncle->black)
	continue;
      to_be_removed = redblack_prec(o->parent);
      if (to_be_removed == granddad || to_be_removed == uncle)
	continue;
      if (!to_be_removed->left || !to_be_removed->right)
	continue;
      node = to_be_removed;
      break;
    }
    TEST(node->inserted, 1);
    redblack_remove(&tree, node);
    node->inserted = 0;
    TEST_1(redblack_height(tree) <= 2 * (int)log2ceil(N - i + 1));
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
  Node *tree = NULL, *o = NULL;
  Node *node;
  unsigned i;
  unsigned int const N = 1000000U;

  BEGIN();

  home = su_home_clone(NULL, sizeof(*home)); TEST_1(home);

  for (i = 0; i < N; i++) {
    node = node_new(home, i);
    TEST(redblack_insert(&tree, node, &o), 0);
    TEST_P(o, NULL);
  }

  TEST_1(redblack_height(tree) <= 2 * (int)log2ceil(i + 1));

  for (i = 0; i < N; i++) {
    node = node_find(tree, i);
    TEST_1(node); TEST(node->value, i);
  }

  node = node_find(tree, 0);
  for (i = 0; i < N; i++) {
    TEST_1(node); TEST(node->value, i);
    node = redblack_succ(node);
  }
  TEST_1(node == NULL);

  su_home_check(home);
  su_home_zap(home);

  END();
}


void usage(int exitcode)
{
  fprintf(stderr,
	  "usage: %s [-v] [-a]\n",
	  name);
  exit(exitcode);
}

int main(int argc, char *argv[])
{
  int retval = 0;
  int i;

  for (i = 1; argv[i]; i++) {
    if (strcmp(argv[i], "-v") == 0)
      tstflags |= tst_verbatim;
    else if (strcmp(argv[i], "-a") == 0)
      tstflags |= tst_abort;
    else
      usage(1);
  }

  retval |= test_insert(); fflush(stdout);
  retval |= test_rotate(); fflush(stdout);
  retval |= test_simple(); fflush(stdout);
  retval |= test_balance(); fflush(stdout);
  retval |= test_speed(); fflush(stdout);

  return retval;
}
