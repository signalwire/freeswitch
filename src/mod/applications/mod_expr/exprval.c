/*
    File: exprval.c
    Auth: Brian Allen Vanderburg II
    Date: Thursday, April 24, 2003
    Desc: Value lists for variables and constants

    This file is part of ExprEval.
*/

/* Includes */
#include "exprincl.h"

#include "exprpriv.h"
#include "exprmem.h"


/* Internal functions */
static exprVal *exprCreateVal(char *name, EXPRTYPE val, EXPRTYPE * addr);
static void exprValListFreeData(exprVal * val);
static void exprValListResetData(exprVal * val);

/* This function creates the value list, */
int exprValListCreate(exprValList ** vlist)
{
	exprValList *tmp;

	if (vlist == NULL)
		return EXPR_ERROR_NULLPOINTER;

	*vlist = NULL;				/* Set to NULL initially */

	tmp = exprAllocMem(sizeof(exprValList));

	if (tmp == NULL)
		return EXPR_ERROR_MEMORY;	/* Could not allocate memory */

	/* Update pointer */
	*vlist = tmp;

	return EXPR_ERROR_NOERROR;
}

/* Add a value to the list */
int exprValListAdd(exprValList * vlist, char *name, EXPRTYPE val)
{
	exprVal *tmp;
	exprVal *cur;
	int result;

	if (vlist == NULL)
		return EXPR_ERROR_NULLPOINTER;

	/* Make sure the name is valid */
	if (!exprValidIdent(name))
		return EXPR_ERROR_BADIDENTIFIER;

	if (vlist->head == NULL) {
		/* Create the node right here */
		tmp = exprCreateVal(name, val, NULL);

		if (tmp == NULL)
			return EXPR_ERROR_MEMORY;

		vlist->head = tmp;
		return EXPR_ERROR_NOERROR;
	}

	/* See if already exists */
	cur = vlist->head;

	while (cur) {
		result = strcmp(name, cur->vname);

		if (result == 0)
			return EXPR_ERROR_ALREADYEXISTS;

		cur = cur->next;
	}

	/* We did not find it, create it and add it to the beginning */
	tmp = exprCreateVal(name, val, NULL);

	if (tmp == NULL)
		return EXPR_ERROR_MEMORY;

	tmp->next = vlist->head;
	vlist->head = tmp;

	return EXPR_ERROR_NOERROR;
}

/* Set a value in the list */
int exprValListSet(exprValList * vlist, char *name, EXPRTYPE val)
{
	exprVal *cur;
	int result;

	if (vlist == NULL)
		return EXPR_ERROR_NULLPOINTER;

	if (name == NULL || name[0] == '\0')
		return EXPR_ERROR_NOTFOUND;

	/* Find and set it */
	cur = vlist->head;

	while (cur) {
		result = strcmp(name, cur->vname);

		if (result == 0) {
			if (cur->vptr)
				*(cur->vptr) = val;
			else
				cur->vval = val;

			return EXPR_ERROR_NOERROR;
		}

		cur = cur->next;
	}

	return EXPR_ERROR_NOTFOUND;
}

/* Get the value from a list  */
int exprValListGet(exprValList * vlist, char *name, EXPRTYPE * val)
{
	exprVal *cur;
	int result;

	if (vlist == NULL)
		return EXPR_ERROR_NULLPOINTER;

	if (name == NULL || name[0] == '\0')
		return EXPR_ERROR_NOTFOUND;

	/* Search for the item */
	cur = vlist->head;

	while (cur) {
		result = strcmp(name, cur->vname);

		if (result == 0) {
			/* We found it. */
			if (cur->vptr)
				*val = *(cur->vptr);
			else
				*val = cur->vval;

			/* return now */
			return EXPR_ERROR_NOERROR;
		}

		cur = cur->next;
	}

	/* If we got here, we did not find the item in the list */
	return EXPR_ERROR_NOTFOUND;
}

/* Add an address to the list */
int exprValListAddAddress(exprValList * vlist, char *name, EXPRTYPE * addr)
{
	exprVal *tmp;
	exprVal *cur;
	int result;

	if (vlist == NULL)
		return EXPR_ERROR_NULLPOINTER;

	/* Make sure the name is valid */
	if (!exprValidIdent(name))
		return EXPR_ERROR_BADIDENTIFIER;

	if (vlist->head == NULL) {
		/* Create the node right here */
		tmp = exprCreateVal(name, (EXPRTYPE) 0.0, addr);

		if (tmp == NULL)
			return EXPR_ERROR_MEMORY;

		vlist->head = tmp;
		return EXPR_ERROR_NOERROR;
	}

	/* See if it already exists */
	cur = vlist->head;

	while (cur) {
		result = strcmp(name, cur->vname);

		if (result == 0)
			return EXPR_ERROR_ALREADYEXISTS;

		cur = cur->next;
	}

	/* Add it to the list */
	tmp = exprCreateVal(name, (EXPRTYPE) 0.0, addr);

	if (tmp == NULL)
		return EXPR_ERROR_MEMORY;

	tmp->next = vlist->head;
	vlist->head = tmp;

	return EXPR_ERROR_NOERROR;
}

/* Get memory address of a variable value in a value list */
int exprValListGetAddress(exprValList * vlist, char *name, EXPRTYPE ** addr)
{
	exprVal *cur;
	int result;

	if (vlist == NULL || addr == NULL)
		return EXPR_ERROR_NULLPOINTER;

	/* Not found yet */
	*addr = NULL;

	if (name == NULL || name[0] == '\0')
		return EXPR_ERROR_NOTFOUND;

	/* Search for the item */
	cur = vlist->head;

	while (cur) {
		result = strcmp(name, cur->vname);

		if (result == 0) {
			/* We found it. */
			if (cur->vptr)
				*addr = cur->vptr;
			else
				*addr = &(cur->vval);

			/* return now */
			return EXPR_ERROR_NOERROR;
		}

		cur = cur->next;
	}

	/* If we got here, we did not find it in the list */
	return EXPR_ERROR_NOTFOUND;
}

/* This function is used to enumerate the values in a value list */
void *exprValListGetNext(exprValList * vlist, char **name, EXPRTYPE * value, EXPRTYPE ** addr, void *cookie)
{
	exprVal *cur;

	if (vlist == NULL)
		return NULL;

	/* Get the current item */
	cur = (exprVal *) cookie;

	/* Find the next item */
	if (cur == NULL)
		cur = vlist->head;
	else
		cur = cur->next;

	/* Set up the data */
	if (cur) {
		if (name)
			*name = cur->vname;

		if (value) {
			if (cur->vptr)
				*value = *(cur->vptr);
			else
				*value = cur->vval;
		}

		if (addr) {
			if (cur->vptr)
				*addr = cur->vptr;
			else
				*addr = &(cur->vval);
		}
	}

	/* If there was no value, return NULL, otherwise, return the item */
	return (void *) cur;
}

/* This routine will free the value list */
int exprValListFree(exprValList * vlist)
{
	/* Make sure it exists, if not it is not error */
	if (vlist == NULL)
		return EXPR_ERROR_NOERROR;

	/* Free the nodes */
	exprValListFreeData(vlist->head);

	/* Freethe container */
	exprFreeMem(vlist);

	return EXPR_ERROR_NOERROR;
}

/* This routine will reset the value list to 0.0 */
int exprValListClear(exprValList * vlist)
{
	if (vlist == NULL)
		return EXPR_ERROR_NOERROR;

	exprValListResetData(vlist->head);

	return EXPR_ERROR_NOERROR;
}

/* This routine will free any child nodes, and then free itself */
static void exprValListFreeData(exprVal * val)
{
	exprVal *next;

	while (val) {
		/* Remember the next */
		next = val->next;

		/* Free name */
		exprFreeMem(val->vname);

		/* Free ourself */
		exprFreeMem(val);

		val = next;
	}
}

/* This routine will reset variables to 0.0 */
static void exprValListResetData(exprVal * val)
{
	while (val) {
		/* Reset data */
		if (val->vptr)
			*(val->vptr) = 0.0;

		val->vval = 0.0;

		val = val->next;
	}
}

/* This routine will create the value object */
static exprVal *exprCreateVal(char *name, EXPRTYPE val, EXPRTYPE * addr)
{
	exprVal *tmp;
	char *vtmp;

	/* Name already tested in exprValListAdd */

	/* Create it */
	tmp = exprAllocMem(sizeof(exprVal));
	if (tmp == NULL)
		return NULL;

	/* Allocate space for the name */
	vtmp = exprAllocMem(strlen(name) + 1);

	if (vtmp == NULL) {
		exprFreeMem(tmp);
		return NULL;
	}

	/* Copy the data over */
	strcpy(vtmp, name);
	tmp->vname = vtmp;
	tmp->vval = val;
	tmp->vptr = addr;

	return tmp;
}
