/* Coverity Scan model
 *
 * This is a modelling file for Coverity Scan. Modelling helps to avoid false
 * positives.
 *
 * - A model file can't import any header files.
 * - Therefore only some built-in primitives like int, char and void are
 *   available but not NULL etc.
 * - Modelling doesn't need full structs and typedefs. Rudimentary structs
 *   and similar types are sufficient.
 * - An uninitialised local pointer is not an error. It signifies that the
 *   variable could be either NULL or have some data.
 *
 * Coverity Scan doesn't pick up modifications automatically. The model file
 * must be uploaded by an admin in the analysis.
 *
 * Based on:
 *     http://hg.python.org/cpython/file/tip/Misc/coverity_model.c
 * Copyright (c) 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
 * 2011, 2012, 2013 Python Software Foundation; All Rights Reserved
 *
 */

/*
 * Useful references:
 *   https://scan.coverity.com/models
 */

typedef unsigned int switch_status_t;

struct pthread_mutex_t {};

struct switch_mutex
{
    struct pthread_mutex_t lock;
};
typedef struct switch_mutex switch_mutex_t;

switch_status_t switch_mutex_lock(switch_mutex_t *lock)
{
    __coverity_recursive_lock_acquire__(&lock->lock);
}

switch_status_t switch_mutex_unlock(switch_mutex_t *lock)
{
    __coverity_recursive_lock_release__(&lock->lock);
}
