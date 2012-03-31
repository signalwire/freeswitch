/*
 * Copyright (c) 1995  Colin Plumb.  All rights reserved.
 * For licensing and other legal details, see the file legal.c.
 *
 * bn00.c - auto-size-detecting bn??.c file.
 *
 * Written in 1995 by Colin Plumb.
 */

#include "bnsize00.h"

#if BNSIZE64

/* Include all of the C source file by reference */
#include "bn64.c"
#include "bninit64.c"

#elif BNSIZE32

/* Include all of the C source file by reference */
#include "bn32.c"
#include "bninit32.c"

#else /* BNSIZE16 */

/* Include all of the C source file by reference */
#include "bn16.c"
#include "bninit16.c"

#endif
