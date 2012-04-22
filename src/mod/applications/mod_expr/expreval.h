/*
    File: expreval.h
    Auth: Brian Allen Vanderburg II
    Date: Thursday, April 24, 2003
    Desc: Main include file for ExprEval library

    This file is part of ExprEval.
*/


/* Include once */
#ifndef __BAVII_EXPREVAL_H
#define __BAVII_EXPREVAL_H


#ifdef __cplusplus
extern "C" {
#endif


/* Define type of data to use */
	typedef double EXPRTYPE;

/* Defines for various things */

/* Max id size */
#define EXPR_MAXIDENTSIZE 255

/* Error values */
	enum {
		EXPR_ERROR_UNKNOWN = -1,	/* Unknown error */
		EXPR_ERROR_NOERROR = 0,	/* No Error */
		EXPR_ERROR_MEMORY,		/* Memory allocation failed */
		EXPR_ERROR_NULLPOINTER,	/* Null pointer passed to function */
		EXPR_ERROR_NOTFOUND,	/* Item not found in a list */
		EXPR_ERROR_UNMATCHEDCOMMENT,	/* Unmatched comment tags */
		EXPR_ERROR_INVALIDCHAR,	/* Invalid characters in expression */
		EXPR_ERROR_ALREADYEXISTS,	/* An item already called create */
		EXPR_ERROR_ALREADYPARSEDBAD,	/* Expression parsed already, but unsuccessfully. call free or clear */
		EXPR_ERROR_ALREADYPARSEDGOOD,	/* Expression parsed already, successfully, call free or clear */
		EXPR_ERROR_EMPTYEXPR,	/* Empty expression string passed to parse */
		EXPR_ERROR_UNMATCHEDPAREN,	/* Unmatched parenthesis */
		EXPR_ERROR_SYNTAX,		/* Syntax error in expression */
		EXPR_ERROR_MISSINGSEMICOLON,	/* Missing semicolon at end of expression */
		EXPR_ERROR_BADIDENTIFIER,	/* Identifier was to big or not formed right */
		EXPR_ERROR_NOSUCHFUNCTION,	/* Function does not exist in function list */
		EXPR_ERROR_BADNUMBERARGUMENTS,	/* Bad number of arguments in a function call */
		EXPR_ERROR_BADEXPR,		/* This is a bad expression to evaluate. It has not been parsed or has unsuccessfully */
		EXPR_ERROR_UNABLETOASSIGN,	/* Unable to do an assignment, maybe no variable list */
		EXPR_ERROR_DIVBYZERO,	/* Attempted a division by zero */
		EXPR_ERROR_NOVARLIST,	/* No variable list found but one is needed */
		EXPR_ERROR_BREAK,		/* Expression was broken by break function */
		EXPR_ERROR_CONSTANTASSIGN,	/* Assignment to a constant */
		EXPR_ERROR_REFCONSTANT,	/* Constant used as a reference parameter */
		EXPR_ERROR_OUTOFRANGE,	/* A bad value was passed to a function */

		EXPR_ERROR_USER			/* Custom errors should be larger than this */
	};

/* Macros */

/* Forward declarations */
	typedef struct _exprNode exprNode;
	typedef struct _exprFuncList exprFuncList;
	typedef struct _exprValList exprValList;
	typedef struct _exprObj exprObj;

/* Function types */
	typedef int (*exprFuncType) (exprObj * obj, exprNode * nodes, int nodecount, EXPRTYPE ** refs, int refcount, EXPRTYPE * val);
	typedef int (*exprBreakFuncType) (exprObj * obj);



/* Functions */

/* Version information function */
	void exprGetVersion(int *major, int *minor);

/* Functions for function lists */
	int exprFuncListCreate(exprFuncList ** flist);
	int exprFuncListAdd(exprFuncList * flist, char *name, exprFuncType ptr, int min, int max, int refmin, int refmax);
	int exprFuncListFree(exprFuncList * flist);
	int exprFuncListClear(exprFuncList * flist);
	int exprFuncListInit(exprFuncList * flist);

/* Functions for value lists */
	int exprValListCreate(exprValList ** vlist);
	int exprValListAdd(exprValList * vlist, char *name, EXPRTYPE val);
	int exprValListSet(exprValList * vlist, char *name, EXPRTYPE val);
	int exprValListGet(exprValList * vlist, char *name, EXPRTYPE * val);
	int exprValListAddAddress(exprValList * vlist, char *name, EXPRTYPE * addr);
	int exprValListGetAddress(exprValList * vlist, char *name, EXPRTYPE ** addr);
	void *exprValListGetNext(exprValList * vlist, char **name, EXPRTYPE * value, EXPRTYPE ** addr, void *cookie);
	int exprValListFree(exprValList * vlist);
	int exprValListClear(exprValList * vlist);
	int exprValListInit(exprValList * vlist);

/* Functions for expression objects */
	int exprCreate(exprObj ** obj, exprFuncList * flist, exprValList * vlist, exprValList * clist, exprBreakFuncType breaker, void *userdata);
	int exprFree(exprObj * obj);
	int exprClear(exprObj * obj);
	int exprParse(exprObj * obj, char *expr);
	int exprEval(exprObj * obj, EXPRTYPE * val);
	int exprEvalNode(exprObj * obj, exprNode * nodes, int curnode, EXPRTYPE * val);
	exprFuncList *exprGetFuncList(exprObj * obj);
	exprValList *exprGetVarList(exprObj * obj);
	exprValList *exprGetConstList(exprObj * obj);
	exprBreakFuncType exprGetBreakFunc(exprObj * obj);
	int exprGetBreakResult(exprObj * obj);
	void *exprGetUserData(exprObj * obj);
	void exprSetUserData(exprObj * obj, void *userdata);
	void exprSetBreakCount(exprObj * obj, int count);
	void exprGetErrorPosition(exprObj * obj, int *start, int *end);

/* Other useful routines */
	int exprValidIdent(char *name);

/* Name mangling */
#ifdef __cplusplus
}
#endif
#endif							/* __BAVII_EXPREVAL_H */
