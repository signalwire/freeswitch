/*
 * Copyright (c) 1995  Colin Plumb.  All rights reserved.
 * For licensing and other legal details, see the file legal.c.
 *
 * We want the copyright string to be accessable to the unix strings command 
 * in the final linked binary, and we don't want the linker to remove it if 
 * it's not referenced, so we do that by using the volatile qualifier.
 * 
 * ANSI C standard, section 3.5.3: "An object that has volatile-qualified
 * type may be modified in ways unknown to the implementation or have
 * other unknown side effects."  Yes, we can't expect a compiler to
 * understand law...
 */
extern volatile const char bnCopyright[];
