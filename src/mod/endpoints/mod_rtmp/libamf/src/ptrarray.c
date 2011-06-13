#include "ptrarray.h"
#include <string.h>

/**
    Customize default capacity
*/
#ifndef PTRARRAY_DEFAULT_CAPACITY
# define PTRARRAY_DEFAULT_CAPACITY 5
#endif

/**
    Enable array security checking
*/
/* #define PTRARRAY_SECURITY_CHECKS */

/**
    Customize memory allocation routines
*/
#ifndef PTRARRAY_MALLOC
# define PTRARRAY_MALLOC  malloc
#endif

#ifndef PTRARRAY_FREE
# define PTRARRAY_FREE    free
#endif

#ifndef PTRARRAY_REALLOC
# define PTRARRAY_REALLOC realloc
#endif

/**
    This function doubles the current capacity
    of the given dynamic array.
 */
static int ptrarray_grow(ptrarray * array) {
    void * new_mem;
    size_t new_capacity;
#ifdef PTRARRAY_SECURITY_CHECKS
    if (array == NULL)
        return;
#endif
    new_capacity = array->capacity * 2;
    new_mem = PTRARRAY_REALLOC(array->data, new_capacity * sizeof(void*));
    if (new_mem != NULL) {
        array->data = new_mem;
        array->capacity = new_capacity;
        return 1;
    }
    else {
        return 0;
    }
}

void ptrarray_init(ptrarray * array, size_t initial_capacity, data_free_proc free_proc) {
    if (free_proc == NULL) {
        free_proc = PTRARRAY_FREE;
    }
    array->data_free = free_proc;
    if (initial_capacity <= 0) {
        initial_capacity = PTRARRAY_DEFAULT_CAPACITY;
    }
    array->capacity = initial_capacity;
    array->data = PTRARRAY_MALLOC(initial_capacity * sizeof(void*));
    array->size = 0;
}

/*size_t ptrarray_capacity(ptrarray * array) {
    return array->capacity;
}*/

/*size_t ptrarray_size(ptrarray * array) {
    return array->size;
}*/

/*int ptrarray_empty(ptrarray * array) {
    return !((array)->size);
}*/

void ptrarray_reserve(ptrarray * array, size_t new_capacity) {
    void * new_mem;
#ifdef PTRARRAY_SECURITY_CHECKS
    if (array == NULL)
        return;
#endif
    if (new_capacity > array->capacity) {
        new_mem = PTRARRAY_REALLOC(array->data, new_capacity * sizeof(void*));
        if (new_mem != NULL) {
            array->data = new_mem;
            array->capacity = new_capacity;
        }
    }
    else if (new_capacity < array->capacity) {
        new_capacity = (new_capacity < array->size) ? array->size : new_capacity;
        new_capacity = (new_capacity < PTRARRAY_DEFAULT_CAPACITY) ? PTRARRAY_DEFAULT_CAPACITY : new_capacity;
        new_mem = PTRARRAY_REALLOC(array->data, new_capacity * sizeof(void*));
        if (new_mem != NULL) {
            array->data = new_mem;
            array->capacity = new_capacity;
        }
    }
}

void ptrarray_compact(ptrarray * array) {
    size_t new_capacity;
    void * new_mem;
#ifdef PTRARRAY_SECURITY_CHECKS
    if (array == NULL)
        return;
#endif
    new_capacity = (array->size < PTRARRAY_DEFAULT_CAPACITY) ? PTRARRAY_DEFAULT_CAPACITY : array->size;
    new_mem = PTRARRAY_REALLOC(array->data, new_capacity * sizeof(void*));
    if (new_mem != NULL) {
        array->data = new_mem;
        array->capacity = new_capacity;
    }
}

void ptrarray_push(ptrarray * array, void * data) {
#ifdef PTRARRAY_SECURITY_CHECKS
    if (array == NULL)
        return;
#endif
    if (array->size == array->capacity) {
        if (!ptrarray_grow(array)) {
            return;
        }
    }
    array->data[array->size++] = data;
}

void * ptrarray_pop(ptrarray * array) {
#ifdef PTRARRAY_SECURITY_CHECKS
    if (array == NULL)
        return NULL;
#endif
    if (ptrarray_empty(array)) {
        return NULL;
    }
    return array->data[array->size--];
}

void ptrarray_insert(ptrarray * array, size_t position, void * data) {
    void ** src_pos;

#ifdef PTRARRAY_SECURITY_CHECKS
    if (array == NULL)
        return;
#endif
    if (array->size > position) {
        if (array->size == array->capacity) {
            if (!ptrarray_grow(array)) {
                return;
            }
        }
        src_pos = array->data + position;
        memmove(src_pos + 1, src_pos, array->size - position);
        *src_pos = data;
    }
}

void ptrarray_prepend(ptrarray * array, void * data);
void * ptrarray_replace(ptrarray * array, size_t position, void * data);

void * ptrarray_remove(ptrarray * array, size_t position);
void ptrarray_clear(ptrarray * array);

void * ptrarray_first(ptrarray * array);
void * ptrarray_last(ptrarray * array);
void * ptrarray_get(size_t position);

void ptrarray_destroy(ptrarray * array) {
}
