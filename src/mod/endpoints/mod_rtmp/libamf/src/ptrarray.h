#ifndef __PTRARRAY_H__
#define __PTRARRAY_H__

#include <stdlib.h>

typedef void (*data_free_proc)(void *);

typedef struct __ptrarray {
    size_t capacity;
    size_t size;
    void ** data;
    data_free_proc data_free;
} ptrarray;

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void   ptrarray_init(ptrarray * array, size_t initial_capacity, data_free_proc free_proc);

/*size_t ptrarray_capacity(ptrarray * array);*/
#define ptrarray_capacity(a) ((a)->capacity)

/*size_t ptrarray_size(ptrarray * array);*/
#define ptrarray_size(a) ((a)->size)

/*int    ptrarray_empty(ptrarray * array);*/
#define ptrarray_empty(a) (!((a)->size))

void   ptrarray_reserve(ptrarray * array, size_t new_capacity);
void   ptrarray_compact(ptrarray * array);

void   ptrarray_push(ptrarray * array, void * data);
void * ptrarray_pop(ptrarray * array);
void   ptrarray_insert(ptrarray * array, size_t position, void * data);
void   ptrarray_prepend(ptrarray * array, void * data);
void * ptrarray_replace(ptrarray * array, size_t position, void * data);

void * ptrarray_remove(ptrarray * array, size_t position);
void   ptrarray_clear(ptrarray * array);

void * ptrarray_first(ptrarray * array);
void * ptrarray_last(ptrarray * array);
void * ptrarray_get(size_t position);

void   ptrarray_destroy(ptrarray * array);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __PTRARRAY_H__ */
