/*
    File: expreval.c
    Auth: Brian Allen Vanderburg II
    Date: Wednesday, April 30, 2003
    Desc: Evaluation routines for the ExprEval library

    This file is part of ExprEval.
*/

/* Includes */
#include "exprincl.h"

#include "exprpriv.h"

/* Defines for error checking */
#include <errno.h>

#if (EXPR_ERROR_LEVEL >= EXPR_ERROR_LEVEL_CHECK)
#define EXPR_RESET_ERR() errno = 0
#define EXPR_CHECK_ERR() if (errno) return EXPR_ERROR_OUTOFRANGE
#else
#define EXPR_RESET_ERR()
#define EXPR_CHECK_ERR()
#endif


/* This routine will evaluate an expression */
int exprEval(exprObj * obj, EXPRTYPE * val)
{
	EXPRTYPE dummy;

	if (val == NULL)
		val = &dummy;

	/* Make sure it was parsed successfully */
	if (!obj->parsedbad && obj->parsedgood && obj->headnode) {
		/* Do NOT reset the break count.  Let is accumulate
		   between calls until breaker function is called */
		return exprEvalNode(obj, obj->headnode, 0, val);
	} else
		return EXPR_ERROR_BADEXPR;
}

/* Evaluate a node */
int exprEvalNode(exprObj * obj, exprNode * nodes, int curnode, EXPRTYPE * val)
{
	int err;
	int pos;
	EXPRTYPE d1, d2;

	if (obj == NULL || nodes == NULL)
		return EXPR_ERROR_NULLPOINTER;

	/* Update n to point to correct node */
	nodes += curnode;

	/* Check breaker count */
	if (obj->breakcur-- <= 0) {
		/* Reset count before returning */
		obj->breakcur = obj->breakcount;

		if (exprGetBreakResult(obj)) {
			return EXPR_ERROR_BREAK;
		}
	}

	switch (nodes->type) {
	case EXPR_NODETYPE_MULTI:
		{
			/* Multi for multiple expressions in one string */
			for (pos = 0; pos < nodes->data.oper.nodecount; pos++) {
				err = exprEvalNode(obj, nodes->data.oper.nodes, pos, val);
				if (err)
					return err;
			}
			break;
		}

	case EXPR_NODETYPE_ADD:
		{
			/* Addition */
			err = exprEvalNode(obj, nodes->data.oper.nodes, 0, &d1);

			if (!err)
				err = exprEvalNode(obj, nodes->data.oper.nodes, 1, &d2);

			if (!err)
				*val = d1 + d2;
			else
				return err;

			break;
		}

	case EXPR_NODETYPE_SUBTRACT:
		{
			/* Subtraction */
			err = exprEvalNode(obj, nodes->data.oper.nodes, 0, &d1);

			if (!err)
				err = exprEvalNode(obj, nodes->data.oper.nodes, 1, &d2);

			if (!err)
				*val = d1 - d2;
			else
				return err;

			break;
		}

	case EXPR_NODETYPE_MULTIPLY:
		{
			/* Multiplication */
			err = exprEvalNode(obj, nodes->data.oper.nodes, 0, &d1);

			if (!err)
				err = exprEvalNode(obj, nodes->data.oper.nodes, 1, &d2);

			if (!err)
				*val = d1 * d2;
			else
				return err;

			break;
		}

	case EXPR_NODETYPE_DIVIDE:
		{
			/* Division */
			err = exprEvalNode(obj, nodes->data.oper.nodes, 0, &d1);

			if (!err)
				err = exprEvalNode(obj, nodes->data.oper.nodes, 1, &d2);

			if (!err) {
				if (d2 != 0.0)
					*val = d1 / d2;
				else {
#if (EXPR_ERROR_LEVEL >= EXPR_ERROR_LEVEL_CHECK)
					return EXPR_ERROR_DIVBYZERO;
#else
					*val = 0.0;
					return EXPR_ERROR_NOERROR;
#endif
				}
			} else
				return err;

			break;
		}

	case EXPR_NODETYPE_EXPONENT:
		{
			/* Exponent */
			err = exprEvalNode(obj, nodes->data.oper.nodes, 0, &d1);

			if (!err)
				err = exprEvalNode(obj, nodes->data.oper.nodes, 1, &d2);

			if (!err) {
				EXPR_RESET_ERR();
				*val = pow(d1, d2);
				EXPR_CHECK_ERR();
			} else
				return err;

			break;
		}

	case EXPR_NODETYPE_NEGATE:
		{
			/* Negative value */
			err = exprEvalNode(obj, nodes->data.oper.nodes, 0, &d1);

			if (!err)
				*val = -d1;
			else
				return err;

			break;
		}


	case EXPR_NODETYPE_VALUE:
		{
			/* Directly access the value */
			*val = nodes->data.value.value;
			break;
		}

	case EXPR_NODETYPE_VARIABLE:
		{
			/* Directly access the variable or constant */
			*val = *(nodes->data.variable.vaddr);
			break;
		}

	case EXPR_NODETYPE_ASSIGN:
		{
			/* Evaluate assignment subnode */
			err = exprEvalNode(obj, nodes->data.assign.node, 0, val);

			if (!err) {
				/* Directly assign the variable */
				*(nodes->data.assign.vaddr) = *val;
			} else
				return err;

			break;
		}

	case EXPR_NODETYPE_FUNCTION:
		{
			/* Evaluate the function */
			if (nodes->data.function.fptr == NULL) {
				/* No function pointer means we are not using
				   function solvers.  See if the function has a
				   type to solve directly. */
				switch (nodes->data.function.type) {
					/* This is to keep the file from being too crowded.
					   See exprilfs.h for the definitions. */
#include "exprilfs.h"


				default:
					{
						return EXPR_ERROR_UNKNOWN;
					}
				}
			} else {
				/* Call the correct function */
				return (*(nodes->data.function.fptr)) (obj,
													   nodes->data.function.nodes, nodes->data.function.nodecount,
													   nodes->data.function.refs, nodes->data.function.refcount, val);
			}

			break;
		}

	default:
		{
			/* Unknown node type */
			return EXPR_ERROR_UNKNOWN;
		}
	}

	return EXPR_ERROR_NOERROR;
}
