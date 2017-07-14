/*
 * SpanDSP - a series of DSP components for telephony
 *
 * stdbool.h - A version for systems which lack their own stdbool.h
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2003 Steve Underwood
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 2.1,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*! \file */


/*
 * ISO C Standard:  7.16  Boolean type and values  <stdbool.h>
 */

#if !defined(_STDBOOL_H)
#define _STDBOOL_H

#ifdef _MSC_VER 
#pragma warning (disable: 4005)
#endif

#if !defined(__cplusplus)

#ifndef _MSC_VER 
typedef int     _Bool;
#endif
typedef int     bool;
#define false   0
#define true    (!false)

#else

typedef bool    _Bool;
#define false   false
#define true    true

#endif

#ifdef _MSC_VER 
#pragma warning (default: 4005)
#endif

/* Signal that all the definitions are present.  */
#define __bool_true_false_are_defined   1

#endif
