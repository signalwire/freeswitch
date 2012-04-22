/*
    File: exprutil.c
    Auth: Brian Allen Vanderburg II
    Date: Monday, April 28, 2003
    Desc: Utility functions for use by this library

    This file is part of ExprEval.
*/

/* Include files */
#include "exprincl.h"

#include "exprpriv.h"

/* Return the version number */
void exprGetVersion(int *major, int *minor)
{
	*major = EXPR_VERSIONMAJOR;
	*minor = EXPR_VERSIONMINOR;
}

/* This utility function determines if an identifier is valid */
int exprValidIdent(char *name)
{
	if (name == NULL)			/* Null string */
		return 0;

	/* First must be letter or underscore */
	if (switch_isalpha(*name) || *name == '_')
		name++;					/* Point to next letter */
	else
		return 0;				/* Not letter or underscore, maybe empty */

	/* others can be letter, number, or underscore */
	while (switch_isalnum(*name) || *name == '_')
		name++;

	/* When the while breaks out, we should be at the end */
	return (*name == '\0') ? 1 : 0;
}
