/*
 * SpanDSP - a series of DSP components for telephony
 *
 * alloc_tests.c - memory allocation handling tests.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2013 Steve Underwood
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*! \file */

/*! \page alloc_tests_page Memory allocation tests
\section alloc_tests_page_sec_1 What does it do?
???.

\section alloc_tests_page_sec_2 How does it work?
???.
*/

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <limits.h>
#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>
#include <stdlib.h>
#include <malloc.h>
#include <inttypes.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/time.h>
#include <time.h>

#include "spandsp.h"

int main(int argc, char *argv[])
{
    void *a;
    void *b;
    void *c;
    
    if (span_mem_allocators(malloc,
                            realloc,
                            free,
                            memalign,
                            free))
    {
        printf("Failed\n");
        exit(2);
    }
    a = span_aligned_alloc(8, 42);
    b = span_alloc(42);
    c = span_realloc(NULL, 42);
    printf("%p %p %p\n", a, b, c);
    span_aligned_free(a);
    span_free(b);
    span_free(c);
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
