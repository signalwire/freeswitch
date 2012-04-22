/*
 * Copyright (c) 1995  Colin Plumb.  All rights reserved.
 * For licensing and other legal details, see the file legal.c.
 *
 * lbn00.c - auto-size-detecting lbn??.c file.
 *
 * Written in 1995 by Colin Plumb.
 */

#include "bnsize00.h"

#if BNSIZE64

/* Include all of the C source file by reference */
#include "lbn64.c"

#elif BNSIZE32

/* Include all of the C source file by reference */
#include "lbn32.c"

#else /* BNSIZE16 */

/* Include all of the C source file by reference */
#include "lbn16.c"

#endif
