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

#ifndef RBTREE_H
/** Defined when <sofia-sip/rbtree.h> has been included. */
#define RBTREE_H

/**@file sofia-sip/rbtree.h
 *
 * Red-black tree.
 *
 * This file contain a red-black-tree template for C. The red-black-tree is
 * a balanced binary tree containing structures as nodes.
 *
 * The prototypes for red-black-tree functions are declared with macro
 * RBTREE_PROTOS(). The implementation is instantiated with macro
 * RBTREE_BODIES().
 *
 * When a entry with new identical key is added to the tree, it can be
 * either @e inserted (replacing other node with same key value) or @e
 * appended.
 *
 * Example code can be found from <rbtree_test.c>.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>.
 *
 * @date Created: Tue Sep  7 19:45:11 EEST 2004 ppessi
 *
 */

#if DOCUMENTATION_ONLY
/** Type used for rbtree nodes. */
typedef struct node Type;
#endif

  /*               x                c
   *              / \              / \
   * Convert     a   c    into    x   d
   *                / \          / \
   *               b   d        a   b
   */

#define RBTREE_LEFT_ROTATE(prefix, Type, left, right, parent)	\
su_inline \
void prefix ## _left_rotate(Type **top, Type *x)   \
{						   \
  Type *c = right(x), *dad = parent(x); assert(c); \
  						   \
  if ((right(x) = left(c)))			   \
    parent(right(x)) = x;			   \
						   \
  if (!(parent(c) = dad))			   \
    *top = c;					   \
  else if (left(dad) == x)			   \
    left(dad) = c;				   \
  else						   \
    assert(right(dad) == x), right(dad) = c;	   \
						   \
  left(c) = x;					   \
  parent(x) = c;				   \
} \
extern int const prefix##_dummy

  /*               x                c
   *              / \              / \
   * Convert     c   f    into    a   x
   *            / \                  / \
   *           a   d                d   f
   */

#define RBTREE_RIGHT_ROTATE(prefix, Type, left, right, parent)	\
su_inline \
void prefix ## _right_rotate(Type **top, Type *x)	\
{							\
  Type *c = left(x), *dad = parent(x); assert(c);	\
							\
  if ((left(x) = right(c)))				\
    parent(left(x)) = x;				\
							\
  if (!(parent(c) = dad))				\
    *top = c;						\
  else if (right(dad) == x)				\
    right(dad) = c;					\
  else							\
    assert(left(dad) == x), left(dad) = c;		\
							\
  right(c) = x;						\
  parent(x) = c;					\
} \
extern int const prefix##_dummy

#define RBTREE_BALANCE_INSERT1(prefix, Type, left, right, parent, IS_RED, SET_RED, IS_BLACK, SET_BLACK)				\
su_inline								\
void prefix ## _balance_insert(Type **top, Type *node)			\
{									\
  Type *dad, *uncle, *granddad;						\
} \
extern int const prefix##_dummy


/* Balance Red-Black binary tree after inserting node @a n.
 *
 * The function red_black_balance_insert() balances a red-black tree after
 * insertion.
 *
 * RED(node) - set node as red
 */
#define RBTREE_BALANCE_INSERT(prefix, Type, left, right, parent, IS_RED, SET_RED, IS_BLACK, SET_BLACK)				\
su_inline								\
void prefix ## _balance_insert(Type **top, Type *node)			\
{									\
  Type *dad, *uncle, *granddad;						\
									\
  SET_RED(node);							\
									\
  for (dad = parent(node); node != *top && IS_RED(dad); dad = parent(node)) { \
    /* Repeat until we are parent or we have a black dad */		\
    granddad = parent(dad); assert(granddad);				\
    if (dad == left(granddad)) {					\
      uncle = right(granddad);						\
      if (IS_RED(uncle)) {						\
	SET_BLACK(dad); SET_BLACK(uncle); SET_RED(granddad);		\
	node = granddad;						\
      } else {								\
	if (node == right(dad)) {					\
	  prefix##_left_rotate(top, node = dad);			\
	  dad = parent(node); assert(dad);				\
	  granddad = parent(dad); assert(granddad);			\
	}								\
	SET_BLACK(dad);							\
	SET_RED(granddad);						\
	prefix##_right_rotate(top, granddad);				\
      }									\
    } else { assert(dad == right(granddad));				\
      uncle = left(granddad);						\
      if (IS_RED(uncle)) {						\
	SET_BLACK(dad); SET_BLACK(uncle); SET_RED(granddad);		\
	node = granddad;						\
      } else {								\
	if (node == left(dad)) {					\
	  prefix##_right_rotate(top, node = dad);			\
	  dad = parent(node); assert(dad);				\
	  granddad = parent(dad); assert(granddad);			\
	}								\
	SET_BLACK(dad);							\
	SET_RED(granddad);						\
	prefix##_left_rotate(top, granddad);				\
      }									\
    }									\
  }									\
									\
  assert(*top);								\
									\
  SET_BLACK((*top));							\
} \
extern int const prefix##_dummy

#define RBTREE_BALANCE_DELETE(prefix, Type, left, right, parent,	\
			      IS_RED, SET_RED, IS_BLACK, SET_BLACK,	\
                              COPY_COLOR)				\
su_inline								\
void prefix##_balance_delete(Type **top, Type *node)			\
{									\
  Type *dad, *brother;							\
									\
  for (dad = parent(node); node != *top && IS_RED(dad); dad = parent(node)) { \
    if (node == left(dad)) {						\
      brother = right(dad);						\
									\
      if (!brother) {							\
	node = dad;							\
	continue;							\
      }									\
									\
      assert(IS_BLACK(brother));					\
									\
      if (IS_BLACK(left(brother)) && IS_BLACK(right(brother))) {	\
	SET_RED(brother);						\
	node = dad;							\
	continue;							\
      }									\
									\
      if (IS_BLACK(right(brother))) {					\
	SET_RED(brother);						\
	SET_BLACK(left(brother));					\
	prefix##_right_rotate(top, brother);				\
	brother = right(dad);						\
      }									\
									\
      COPY_COLOR(brother, dad);						\
      SET_BLACK(dad);							\
      if (right(brother))						\
	SET_BLACK(right(brother));					\
      prefix##_left_rotate(top, dad);					\
      node = *top;							\
      break;								\
    } else {								\
      assert(node == right(dad));					\
									\
      brother = left(dad);						\
									\
      if (!brother) {							\
	node = dad;							\
	continue;							\
      }									\
									\
      assert(IS_BLACK(brother));					\
									\
      if (IS_BLACK(left(brother)) && IS_BLACK(right(brother))) {	\
	SET_RED(brother);						\
	node = dad;							\
	continue;							\
      }									\
									\
      if (IS_BLACK(left(brother))) {					\
	SET_BLACK(right(brother));					\
	SET_RED(brother);						\
	prefix##_left_rotate(top, brother);				\
	brother = left(dad);						\
      }									\
									\
      COPY_COLOR(brother, parent(node));				\
      SET_BLACK(parent(node));						\
      if (left(brother))						\
	SET_BLACK(left(brother));					\
      prefix##_right_rotate(top, dad);					\
      node = *top;							\
      break;								\
    }									\
  }									\
									\
  SET_BLACK(node);							\
}  \
extern int const prefix##_dummy

#if DOCUMENTATION_ONLY
/**Insert a @a node into the @a tree.
 *
 * @param tree pointer to the root of the tree
 * @param node pointer to node to be inserted
 * @param return_old return value parameter for matching node
 *                   already in the @a tree
 *
 * @retval 0 if node was inserted
 * @retval -1 if there already was an matching node
 *            and return_old is NULL.
 */
int rbtree_insert(Type **tree, Type *node, Type **return_old);
#endif

/* Insert node into tree. */
#define RBTREE_INSERT(SCOPE, prefix, Type, left, right, parent,		\
		      IS_RED, SET_RED, IS_BLACK, SET_BLACK, COPY_COLOR,	\
		      CMP, REMOVE, INSERT)				\
SCOPE									\
int prefix ## _insert(Type **const tree,				\
	 	      Type * const node,				\
		      Type **return_old)				\
{									\
  Type *old, *dad, **slot;						\
									\
  if (tree == NULL || node == NULL) return -1;				\
									\
  for (slot = tree, old = *slot, dad = NULL; old; old = *slot) {	\
    int result = CMP(node, old);					\
    if (result < 0)							\
      dad = old, slot = &(left(old));					\
    else if (result > 0)						\
      dad = old, slot = &(right(old));					\
    else								\
      break;								\
  }									\
									\
  if (old == node)							\
    old = NULL;								\
  else if (old) {							\
    if (!return_old) return -1;						\
									\
    if ((left(node) = left(old)))					\
      parent(left(node)) = node;					\
    if ((right(node) = right(old)))					\
      parent(right(node)) = node;					\
									\
    if (!(parent(node) = parent(old)))					\
      *tree = node;							\
    else if (left(parent(node)) == old)					\
      left(parent(node)) = node;					\
    else assert(right(parent(node)) == old),				\
      right(parent(node)) = node;					\
									\
    COPY_COLOR(node, old);						\
									\
    REMOVE(old);							\
									\
  } else {								\
    *slot = node;							\
    parent(node) = dad;							\
									\
    if (tree != slot) {							\
      prefix##_balance_insert(tree, node);				\
    } else {								\
      SET_BLACK(node);							\
    }									\
  }									\
									\
  INSERT(node);								\
									\
  if (return_old)							\
    *return_old = old;							\
									\
  return 0;								\
}  \
extern int const prefix##_dummy

#if DOCUMENTATION_ONLY
/** Append @a a node into the @a tree.
 *
 * @param tree pointer to the root of the tree
 * @param node pointer to node to be appended
 *
 * @retval 0 when successful (always)
 */
int rbtree_append(Type ** tree,
		  Type *  node);
#endif

/* Define function appending a node into the tree */
#define RBTREE_APPEND(SCOPE, prefix, Type, left, right, parent,		\
		      IS_RED, SET_RED, IS_BLACK, SET_BLACK, COPY_COLOR,	\
		      CMP, REMOVE, INSERT)				\
SCOPE									\
int prefix ## _append(Type **const tree,				\
	 	      Type * const node)				\
{									\
  Type *old, *dad, **slot;						\
									\
  if (tree == NULL || node == NULL) return -1;				\
									\
  for (slot = tree, old = *slot, dad = NULL; old; old = *slot) {	\
    if (old == node)							\
      return 0;								\
    if (CMP(node, old) < 0)						\
      dad = old, slot = &(left(old));					\
    else								\
      dad = old, slot = &(right(old));					\
  }									\
									\
  *slot = node;								\
  parent(node) = dad;							\
									\
  if (tree != slot) {							\
    prefix##_balance_insert(tree, node);				\
  } else {								\
    SET_BLACK(node);							\
  }									\
									\
  INSERT(node);								\
									\
  return 0;								\
}  \
extern int const prefix##_dummy

#if DOCUMENTATION_ONLY
/** Remove a @a node from the @a tree.
 *
 * @param tree pointer to the root of the tree
 * @param node pointer to node to be appended
 */
void rbtree_remove(Type **tree, Type *node);
#endif

#define RBTREE_REMOVE(SCOPE, prefix, Type, left, right, parent,		\
		      IS_RED, SET_RED, IS_BLACK, SET_BLACK, COPY_COLOR, \
		      REMOVE, INSERT)					\
SCOPE									\
void prefix##_remove(Type **top, Type *node)				\
{									\
  Type *kid, *dad;							\
  int need_to_balance;							\
									\
  if (top == NULL || node == NULL)					\
    return;								\
									\
  /* Make sure that node is in tree */					\
  for (dad = node; dad; dad = parent(dad))				\
    if (dad == *top)							\
      break;								\
									\
  if (!dad) return;							\
									\
  /* Find a successor node with a free branch */			\
  if (!left(node) || !right(node))					\
    dad = node;								\
  else for (dad = right(node); left(dad); dad = left(dad))		\
    ;									\
  /* Dad has a free branch => kid is dad's only child */		\
  kid = left(dad) ? left(dad) : right(dad);				\
									\
  /* Remove dad from tree */						\
  if (!(parent(dad)))							\
    *top = kid;								\
  else if (left(parent(dad)) == dad)					\
    left(parent(dad)) = kid;						\
  else assert(right(parent(dad)) == dad),				\
    right(parent(dad)) = kid;						\
  if (kid)								\
    parent(kid) = parent(dad);						\
									\
  need_to_balance = kid && IS_BLACK(dad);				\
									\
  /* Put dad in place of node */					\
  if (node != dad) {							\
    if (!(parent(dad) = parent(node)))					\
      *top = dad;							\
    else if (left(parent(dad)) == node)					\
      left(parent(dad)) = dad;						\
    else assert(right(parent(dad)) == node),				\
      right(parent(dad)) = dad;						\
									\
    COPY_COLOR(dad, node);						\
									\
    if ((left(dad) = left(node)))					\
      parent(left(dad)) = dad;						\
									\
    if ((right(dad) = right(node)))					\
      parent(right(dad)) = dad;						\
  }									\
									\
  REMOVE(node);								\
  SET_RED(node);							\
									\
  if (need_to_balance)							\
    prefix##_balance_delete(top, kid);					\
} \
extern int const prefix##_dummy

#if DOCUMENTATION_ONLY
/** Return inorder successor of node @a node.
 *
 * @param node pointer to node
 *
 * @return Pointer to successor, or NULL if @a node was last.
 */
Type *rbtree_succ(Type const *node);
#endif

/* Define function returning inorder successor of node @a node. */
#define RBTREE_SUCC(SCOPE, prefix, Type, left, right, parent)	\
SCOPE Type *prefix##_succ(Type const *node)				\
{									\
  Type const *dad;							\
									\
  if (right(node)) {							\
    for (node = right(node); left(node); node = left(node))		\
      ;									\
    return (Type *)node;						\
  }									\
									\
  for (dad = parent(node); dad && node == right(dad); dad = parent(node)) \
    node = dad;								\
									\
  return (Type *)dad;							\
} \
extern int const prefix##_dummy

#if DOCUMENTATION_ONLY
/** Return inorder precedessor of node @a node.
 *
 * @param node pointer to node
 *
 * @return Pointer to precedessor, or NULL if @a node was first.
 */
Type *rbtree_prec(Type const *node);
#endif

/* Define function returning inorder precedessor of node @a node. */
#define RBTREE_PREC(SCOPE, prefix, Type, left, right, parent)	\
  SCOPE Type *prefix##_prec(Type const *node)				\
{									\
  Type const *dad;							\
									\
  if (left(node)) {							\
    for (node = left(node); right(node); node = right(node))		\
      ;									\
    return (Type *)node;						\
  }									\
									\
  for (dad = parent(node); dad && node == left(dad); dad = parent(node)) \
    node = dad;								\
									\
  return (Type *)dad;							\
} \
extern int const prefix##_dummy

#if DOCUMENTATION_ONLY
/** Return first node in the tree to which @a node belongs to.
 *
 * @param node pointer to node
 *
 * @return Pointer to first node.
 */
Type *rbtree_first(Type const *node);
#endif

/* Define function returning first node in tree @a node. */
#define RBTREE_FIRST(SCOPE, prefix, Type, left, right, parent)	\
SCOPE Type *prefix##_first(Type const *node)    \
{						\
  while (node && left(node))			\
    node = left(node);				\
						\
  return (Type *)node;				\
} \
extern int const prefix##_dummy

#if DOCUMENTATION_ONLY
/** Return last node in the tree to which @a node belongs to.
 *
 * @param node pointer to node
 *
 * @return Pointer to last node.
 */
Type *rbtree_last(Type const *node);
#endif

/* Define function returning last node in tree @a node. */
#define RBTREE_LAST(SCOPE, prefix, Type, left, right, parent)	\
SCOPE Type *prefix##_last(Type const *node)	\
{						\
  while (node && right(node))			\
    node = right(node);				\
						\
  return (Type *)node;				\
} \
extern int const prefix##_dummy

#if DOCUMENTATION_ONLY
/** Return height of the tree below @a node.
 *
 * @param node pointer to node
 *
 * @return Height of the tree.
 */
int rbtree_height(Type const *node);
#endif

/* Define function returning height of the tree from the @a node. */
#define RBTREE_HEIGHT(SCOPE, prefix, Type, getleft, getright, getparent)	\
SCOPE int prefix##_height(Type const *node)			\
{								\
  int left, right;						\
								\
  if (!node)							\
    return 0;							\
								\
  left = getleft(node) ? prefix##_height(getleft(node)) : 0;	\
  right = getright(node) ? prefix##_height(getright(node)) : 0;	\
								\
  if (left > right)						\
    return left + 1;						\
  else								\
    return right + 1;						\
} \
extern int const prefix##_dummy

/** Define prototypes for red-black tree functions. @HIDE
 *
 * @param SCOPE function scope (e.g., su_inline)
 * @param prefix function prefix (e.g., rbtree)
 * @param Type node type
 *
 * @par Example
 * @code
 * RBTREE_PROTOS(su_inline, rbtree, struct node);
 * @endcode
 */
#define RBTREE_PROTOS(SCOPE, prefix, Type)				\
  SCOPE int prefix ## _insert(Type **, Type *, Type **return_old);	\
  SCOPE int prefix ## _append(Type **, Type *);				\
  SCOPE void prefix ## _remove(Type **, Type *);			\
  SCOPE Type *prefix ## _succ(Type const *);				\
  SCOPE Type *prefix ## _prec(Type const *);				\
  SCOPE Type *prefix ## _first(Type const *);				\
  SCOPE Type *prefix ## _last(Type const *);				\
  SCOPE int prefix ## _height(Type const *)

/** Define bodies for red-black tree functions. @HIDE
 *
 * @param SCOPE function scope (e.g., su_inline)
 * @param prefix function prefix (e.g., rbtree)
 * @param Type node type
 * @param left accessor of left node
 * @param right accessor of right node
 * @param parent accessor of parent node
 * @param IS_RED predicate testing if node is red
 * @param SET_RED setter marking node as red
 * @param IS_BLACK predicate testing if node is black
 * @param SET_BLACK setter marking node as black
 * @param COPY_COLOR method copying color from node to another
 * @param CMP method comparing two nodes
 * @param INSERT setter marking node as inserted to the tree
 * @param REMOVE method marking node as removed and possibly deleting node
 *
 * @par Example
 *
 * @code
 * #define LEFT(node) ((node)->left)
 * #define RIGHT(node) ((node)->right)
 * #define PARENT(node) ((node)->parent)
 * #define SET_RED(node) ((node)->black = 0)
 * #define SET_BLACK(node) ((node)->black = 1)
 * #define CMP(a, b) ((a)->value - (b)->value)
 * #define IS_RED(node) ((node) && (node)->black == 0)
 * #define IS_BLACK(node) (!(node) || (node)->black == 1)
 * #define COPY_COLOR(dst, src) ((dst)->black = (src)->black)
 * #define INSERT(node) ((node)->inserted = 1)
 * #define REMOVE(node) ((node)->left = (node)->right = (node)->parent = NULL, \
 *                       (node)->inserted = 0)
 *
 * RBTREE_BODIES(su_inline, rbtree, struct node,
 *               LEFT, RIGHT, PARENT,
 *               IS_RED, SET_RED, IS_BLACK, SET_BLACK, COPY_COLOR,
 *               CMP, INSERT, REMOVE);
 * @endcode
 */
#define RBTREE_BODIES(SCOPE, prefix, Type,				\
		      left, right, parent,				\
		      IS_RED, SET_RED, IS_BLACK, SET_BLACK, COPY_COLOR, \
		      CMP, INSERT, REMOVE)				\
  RBTREE_LEFT_ROTATE(prefix, Type, left, right, parent);		\
  RBTREE_RIGHT_ROTATE(prefix, Type, left, right, parent);		\
  RBTREE_BALANCE_INSERT(prefix, Type, left, right, parent,		\
			IS_RED, SET_RED, IS_BLACK, SET_BLACK);		\
  RBTREE_BALANCE_DELETE(prefix, Type, left, right, parent,		\
			IS_RED, SET_RED, IS_BLACK, SET_BLACK,		\
			COPY_COLOR);					\
  RBTREE_INSERT(SCOPE, prefix, Type, left, right, parent,		\
		IS_RED, SET_RED, IS_BLACK, SET_BLACK, COPY_COLOR,	\
		CMP, REMOVE, INSERT);					\
  RBTREE_APPEND(SCOPE, prefix, Type, left, right, parent,		\
		IS_RED, SET_RED, IS_BLACK, SET_BLACK, COPY_COLOR,	\
		CMP, REMOVE, INSERT);					\
  RBTREE_REMOVE(SCOPE, prefix, Type, left, right, parent,		\
		IS_RED, SET_RED, IS_BLACK, SET_BLACK, COPY_COLOR,	\
		REMOVE, INSERT);					\
  RBTREE_SUCC(SCOPE, prefix, Type, left, right, parent);		\
  RBTREE_PREC(SCOPE, prefix, Type, left, right, parent);		\
  RBTREE_FIRST(SCOPE, prefix, Type, left, right, parent);		\
  RBTREE_LAST(SCOPE, prefix, Type, left, right, parent);		\
  RBTREE_HEIGHT(SCOPE, prefix, Type, left, right, parent)

#endif /* !define(RBTREE_H) */
