/*
    File: exprmem.c
    Auth: Brian Allen Vanderburg II
    Date: Wednesday, April 30, 2003
    Desc: Memory functions for ExprEval

    This file is part of ExprEval.
*/

/* Includes */
#include "exprincl.h"

#include "exprmem.h"

/* Allocate memory and zero it */
void *exprAllocMem(size_t size)
{
	void *data = malloc(size);

	if (data) {
		memset(data, 0, size);
	}

	return data;
}

/* Free memory */
void exprFreeMem(void *data)
{
	if (data)
		free(data);
}

/* Allocate a list of nodes */
exprNode *exprAllocNodes(size_t count)
{
	return exprAllocMem(count * sizeof(exprNode));
}
