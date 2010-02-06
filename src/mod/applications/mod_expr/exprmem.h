/*
    File: exprmem.h
    Auth: Brian Allen Vanderburg II
    Date: Wednesday, April 30, 2003
    Desc: Memory functions for ExprEval

    This file is part of ExprEval.
*/

#ifndef __BAVII_EXPRMEM_H
#define __BAVII_EXPRMEM_H

/* Needed for exprNode */
#include "exprpriv.h"

void *exprAllocMem(size_t size);
void exprFreeMem(void *data);
exprNode *exprAllocNodes(size_t count);


#endif /* __BAVII_EXPRMEM_H */
