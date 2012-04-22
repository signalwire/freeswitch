/*
    File: exprconf.h
    Auth: Brian Allen Vanderburg II
    Date: Thursday, October 20, 2005
    Desc: Configuration for ExprEval

    This file is part of ExprEval.
*/

#ifndef __BAVII_EXPRCONF_H
#define __BAVII_EXPRCONF_H

/*
    Error checking level

    0: Don't check any errors (don't use errno).  Divide by 0
    is avoided and the part that divides by 0 is 0. For example
    '4+1/0' is 4.

    1: Check math errors.
*/
#define EXPR_ERROR_LEVEL_NONE 0
#define EXPR_ERROR_LEVEL_CHECK 1

#ifndef EXPR_ERROR_LEVEL
#define EXPR_ERROR_LEVEL EXPR_ERROR_LEVEL_CHECK
#endif

#endif /* __BAVII_EXPRCONF_H */
