/*
    File: exprobj.c
    Auth: Brian Allen Vanderburg II
    Date: Tuesday, April 29, 2003
    Desc: Functions for the exprObj type

    This file is part of ExprEval.
*/

/* Includes */
#include "exprincl.h"

#include "exprpriv.h"
#include "exprmem.h"

/* Internal functions */
static void exprFreeNodeData(exprNode * node);


/* Function to create an expression object */
int exprCreate(exprObj ** obj, exprFuncList * flist, exprValList * vlist, exprValList * clist, exprBreakFuncType breaker, void *userdata)
{
	exprObj *tmp;

	/* Allocate memory for the object */
	tmp = exprAllocMem(sizeof(exprObj));

	if (tmp == NULL)
		return EXPR_ERROR_MEMORY;


	/* Assign data */
	tmp->flist = flist;
	tmp->vlist = vlist;
	tmp->clist = clist;
	tmp->breakerfunc = breaker;
	tmp->userdata = userdata;
	tmp->breakcount = 100000;	/* Default breaker count setting */
	tmp->breakcur = 0;

	/* Update pointer */
	*obj = tmp;

	return EXPR_ERROR_NOERROR;
}


/* Free the expression */
int exprFree(exprObj * obj)
{
	if (obj == NULL)
		return EXPR_ERROR_NOERROR;

	/* First free the node data */
	exprFreeNodeData(obj->headnode);
	exprFreeMem(obj->headnode);

	/* Free ourself */
	exprFreeMem(obj);

	return EXPR_ERROR_NOERROR;
}

/* Clear expression, keep lists, etc */
int exprClear(exprObj * obj)
{
	if (obj == NULL)
		return EXPR_ERROR_NOERROR;

	/* Free the node data only, keep function, variable, constant lists */
	exprFreeNodeData(obj->headnode);
	exprFreeMem(obj->headnode);

	obj->headnode = NULL;
	obj->parsedbad = 0;
	obj->parsedgood = 0;

	return EXPR_ERROR_NOERROR;
}


/* Get functions to get information about the expression object */

/* Get the function list */
exprFuncList *exprGetFuncList(exprObj * obj)
{
	return (obj == NULL) ? NULL : obj->flist;
}

/* Get the variable list */
exprValList *exprGetVarList(exprObj * obj)
{
	return (obj == NULL) ? NULL : obj->vlist;
}

/* Get the constant list */
exprValList *exprGetConstList(exprObj * obj)
{
	return (obj == NULL) ? NULL : obj->clist;
}

/* Get the breaker function */
exprBreakFuncType exprGetBreakFunc(exprObj * obj)
{
	return (obj == NULL) ? NULL : obj->breakerfunc;
}

/* Check for break status */
int exprGetBreakResult(exprObj * obj)
{
	if (obj == NULL)
		return 0;

	if (obj->breakerfunc == NULL)
		return 0;

	return (*(obj->breakerfunc)) (obj);
}

/* Get the user data */
void *exprGetUserData(exprObj * obj)
{
	return (obj == NULL) ? NULL : obj->userdata;
}


/* Set functions to set certain data */

/* Set user data */
void exprSetUserData(exprObj * obj, void *userdata)
{
	if (obj)
		obj->userdata = userdata;
}


/* Set breaker count */
void exprSetBreakCount(exprObj * obj, int count)
{
	if (obj) {
		/* If count is negative, make it positive */
		if (count < 0)
			count = -count;

		obj->breakcount = count;

		/* Make sure the current value is not bigger than count */
		if (obj->breakcur > count)
			obj->breakcur = count;
	}
}

/* Get error position */
void exprGetErrorPosition(exprObj * obj, int *start, int *end)
{
	if (obj) {
		if (start)
			*start = obj->starterr;

		if (end)
			*end = obj->enderr;
	}
}

/* This function will free a node's data */
static void exprFreeNodeData(exprNode * node)
{
	int pos;

	if (node == NULL)
		return;

	/* free data based on type */
	switch (node->type) {
	case EXPR_NODETYPE_ADD:
	case EXPR_NODETYPE_SUBTRACT:
	case EXPR_NODETYPE_MULTIPLY:
	case EXPR_NODETYPE_DIVIDE:
	case EXPR_NODETYPE_EXPONENT:
	case EXPR_NODETYPE_NEGATE:
	case EXPR_NODETYPE_MULTI:
		/* Free operation data */
		if (node->data.oper.nodes) {
			for (pos = 0; pos < node->data.oper.nodecount; pos++)
				exprFreeNodeData(&(node->data.oper.nodes[pos]));

			exprFreeMem(node->data.oper.nodes);
		}

		break;


	case EXPR_NODETYPE_VALUE:
		/* Nothing to free for value */
		break;

	case EXPR_NODETYPE_VARIABLE:
		/* Nothing to free for variable */
		break;

	case EXPR_NODETYPE_FUNCTION:
		/* Free data of each subnode */
		if (node->data.function.nodes) {
			for (pos = 0; pos < node->data.function.nodecount; pos++)
				exprFreeNodeData(&(node->data.function.nodes[pos]));

			/* Free the subnode array */
			exprFreeMem(node->data.function.nodes);
		}

		/* Free reference variable list */
		if (node->data.function.refs)
			exprFreeMem(node->data.function.refs);

		break;

	case EXPR_NODETYPE_ASSIGN:
		/* Free subnode data */
		if (node->data.assign.node) {
			exprFreeNodeData(node->data.assign.node);

			/* Free the subnode */
			exprFreeMem(node->data.assign.node);
		}

		break;
	}
}
