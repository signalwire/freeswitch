/*
    File: exprinit.c
    Auth: Brian Allen Vanderburg II
    Date: Thursday, May 1, 2003
    Desc: Extra functions and routines for ExprEval

    This file is part of ExprEval.
*/

/* Include files */
#include "exprincl.h"

#include "exprpriv.h"


/* Macro for adding a function node type */
#define EXPR_ADDFUNC_TYPE(name, type, argmin, argmax, refmin, refmax) \
err = exprFuncListAddType(flist, name, type, argmin, argmax, refmin, refmax); \
if (err != EXPR_ERROR_NOERROR) \
    return err;

/* Macro for adding a constant */
#define EXPR_ADDCONST(name, val) \
err = exprValListAdd(vlist, name, val); \
if (err != EXPR_ERROR_NOERROR) \
    return err;

/* Call this function to initialize these functions into a function list */
int exprFuncListInit(exprFuncList * flist)
{
	int err;

	if (flist == NULL)
		return EXPR_ERROR_NULLPOINTER;

	EXPR_ADDFUNC_TYPE("abs", EXPR_NODEFUNC_ABS, 1, 1, 0, 0);
	EXPR_ADDFUNC_TYPE("mod", EXPR_NODEFUNC_MOD, 2, 2, 0, 0);
	EXPR_ADDFUNC_TYPE("ipart", EXPR_NODEFUNC_IPART, 1, 1, 0, 0);
	EXPR_ADDFUNC_TYPE("fpart", EXPR_NODEFUNC_FPART, 1, 1, 0, 0);
	EXPR_ADDFUNC_TYPE("min", EXPR_NODEFUNC_MIN, 1, -1, 0, 0);
	EXPR_ADDFUNC_TYPE("max", EXPR_NODEFUNC_MAX, 1, -1, 0, 0);
	EXPR_ADDFUNC_TYPE("pow", EXPR_NODEFUNC_POW, 2, 2, 0, 0);
	EXPR_ADDFUNC_TYPE("sqrt", EXPR_NODEFUNC_SQRT, 1, 1, 0, 0);
	EXPR_ADDFUNC_TYPE("sin", EXPR_NODEFUNC_SIN, 1, 1, 0, 0);
	EXPR_ADDFUNC_TYPE("sinh", EXPR_NODEFUNC_SINH, 1, 1, 0, 0);
	EXPR_ADDFUNC_TYPE("asin", EXPR_NODEFUNC_ASIN, 1, 1, 0, 0);
	EXPR_ADDFUNC_TYPE("cos", EXPR_NODEFUNC_COS, 1, 1, 0, 0);
	EXPR_ADDFUNC_TYPE("cosh", EXPR_NODEFUNC_COSH, 1, 1, 0, 0);
	EXPR_ADDFUNC_TYPE("acos", EXPR_NODEFUNC_ACOS, 1, 1, 0, 0);
	EXPR_ADDFUNC_TYPE("tan", EXPR_NODEFUNC_TAN, 1, 1, 0, 0);
	EXPR_ADDFUNC_TYPE("tanh", EXPR_NODEFUNC_TANH, 1, 1, 0, 0);
	EXPR_ADDFUNC_TYPE("atan", EXPR_NODEFUNC_ATAN, 1, 1, 0, 0);
	EXPR_ADDFUNC_TYPE("atan2", EXPR_NODEFUNC_ATAN2, 2, 2, 0, 0);
	EXPR_ADDFUNC_TYPE("log", EXPR_NODEFUNC_LOG, 1, 1, 0, 0);
	EXPR_ADDFUNC_TYPE("pow10", EXPR_NODEFUNC_POW10, 1, 1, 0, 0);
	EXPR_ADDFUNC_TYPE("ln", EXPR_NODEFUNC_LN, 1, 1, 0, 0);
	EXPR_ADDFUNC_TYPE("exp", EXPR_NODEFUNC_EXP, 1, 1, 0, 0);
	EXPR_ADDFUNC_TYPE("logn", EXPR_NODEFUNC_LOGN, 2, 2, 0, 0);
	EXPR_ADDFUNC_TYPE("ceil", EXPR_NODEFUNC_CEIL, 1, 1, 0, 0);
	EXPR_ADDFUNC_TYPE("floor", EXPR_NODEFUNC_FLOOR, 1, 1, 0, 0);
	EXPR_ADDFUNC_TYPE("rand", EXPR_NODEFUNC_RAND, 0, 0, 1, 1);
	EXPR_ADDFUNC_TYPE("random", EXPR_NODEFUNC_RANDOM, 2, 2, 1, 1);
	EXPR_ADDFUNC_TYPE("randomize", EXPR_NODEFUNC_RANDOMIZE, 0, 0, 1, 1);
	EXPR_ADDFUNC_TYPE("deg", EXPR_NODEFUNC_DEG, 1, 1, 0, 0);
	EXPR_ADDFUNC_TYPE("rad", EXPR_NODEFUNC_RAD, 1, 1, 0, 0);
	EXPR_ADDFUNC_TYPE("recttopolr", EXPR_NODEFUNC_RECTTOPOLR, 2, 2, 0, 0);
	EXPR_ADDFUNC_TYPE("recttopola", EXPR_NODEFUNC_RECTTOPOLA, 2, 2, 0, 0);
	EXPR_ADDFUNC_TYPE("poltorectx", EXPR_NODEFUNC_POLTORECTX, 2, 2, 0, 0);
	EXPR_ADDFUNC_TYPE("poltorecty", EXPR_NODEFUNC_POLTORECTY, 2, 2, 0, 0);
	EXPR_ADDFUNC_TYPE("if", EXPR_NODEFUNC_IF, 3, 3, 0, 0);
	EXPR_ADDFUNC_TYPE("select", EXPR_NODEFUNC_SELECT, 3, 4, 0, 0);
	EXPR_ADDFUNC_TYPE("equal", EXPR_NODEFUNC_EQUAL, 2, 2, 0, 0);
	EXPR_ADDFUNC_TYPE("above", EXPR_NODEFUNC_ABOVE, 2, 2, 0, 0);
	EXPR_ADDFUNC_TYPE("below", EXPR_NODEFUNC_BELOW, 2, 2, 0, 0);
	EXPR_ADDFUNC_TYPE("avg", EXPR_NODEFUNC_AVG, 1, -1, 0, 0);
	EXPR_ADDFUNC_TYPE("clip", EXPR_NODEFUNC_CLIP, 3, 3, 0, 0);
	EXPR_ADDFUNC_TYPE("clamp", EXPR_NODEFUNC_CLAMP, 3, 3, 0, 0);
	EXPR_ADDFUNC_TYPE("pntchange", EXPR_NODEFUNC_PNTCHANGE, 5, 5, 0, 0);
	EXPR_ADDFUNC_TYPE("poly", EXPR_NODEFUNC_POLY, 2, -1, 0, 0);
	EXPR_ADDFUNC_TYPE("and", EXPR_NODEFUNC_AND, 2, 2, 0, 0);
	EXPR_ADDFUNC_TYPE("or", EXPR_NODEFUNC_OR, 2, 2, 0, 0);
	EXPR_ADDFUNC_TYPE("not", EXPR_NODEFUNC_NOT, 1, 1, 0, 0);
	EXPR_ADDFUNC_TYPE("for", EXPR_NODEFUNC_FOR, 4, -1, 0, 0);
	EXPR_ADDFUNC_TYPE("many", EXPR_NODEFUNC_MANY, 1, -1, 0, 0);

	return EXPR_ERROR_NOERROR;
}

/* Call this function to initialize some constants into a value list */
int exprValListInit(exprValList * vlist)
{
	int err;

	if (vlist == NULL)
		return EXPR_ERROR_NULLPOINTER;

	EXPR_ADDCONST("M_E", M_E);
	EXPR_ADDCONST("M_LOG2E", M_LOG2E);
	EXPR_ADDCONST("M_LOG10E", M_LOG10E);
	EXPR_ADDCONST("M_LN2", M_LN2);
	EXPR_ADDCONST("M_LN10", M_LN10);
	EXPR_ADDCONST("M_PI", M_PI);
	EXPR_ADDCONST("M_PI_2", M_PI_2);
	EXPR_ADDCONST("M_PI_4", M_PI_4);
	EXPR_ADDCONST("M_1_PI", M_1_PI);
	EXPR_ADDCONST("M_2_PI", M_2_PI);
	EXPR_ADDCONST("M_1_SQRTPI", M_1_SQRTPI);
	EXPR_ADDCONST("M_2_SQRTPI", M_2_SQRTPI);
	EXPR_ADDCONST("M_SQRT2", M_SQRT2);
	EXPR_ADDCONST("M_1_SQRT2", M_1_SQRT2);

	return EXPR_ERROR_NOERROR;
}
