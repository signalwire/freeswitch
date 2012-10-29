/*
 * Generic table defines...
 *
 * Copyright 2000 by Gray Watson.
 *
 * This file is part of the table package.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose and without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies,
 * and that the name of Gray Watson not be used in advertising or
 * publicity pertaining to distribution of the document or software
 * without specific, written prior permission.
 *
 * Gray Watson makes no representations about the suitability of the
 * software described herein for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * The author may be reached via http://256.com/gray/
 *
 * $Id: table.h,v 1.11 2000/03/09 03:30:42 gray Exp $
 */

#ifndef __TABLE_H__
#define __TABLE_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*
 * To build a "key" in any of the below routines, pass in a pointer to
 * the key and its size [i.e. sizeof(int), etc].  With any of the
 * "key" or "data" arguments, if their size is < 0, it will do an
 * internal strlen of the item and add 1 for the \0.
 *
 * If you are using firstkey() and nextkey() functions, be careful if,
 * after starting your firstkey loop, you use delete or insert, it
 * will not crash but may produce interesting results.  If you are
 * deleting from firstkey to NULL it will work fine.
 */

/* return types for table functions */
#define TABLE_ERROR_NONE	1	/* no error from function */
#define TABLE_ERROR_PNT		2	/* bad table pointer */
#define TABLE_ERROR_ARG_NULL	3	/* buffer args were null */
#define TABLE_ERROR_SIZE	4	/* size of data was bad */
#define TABLE_ERROR_OVERWRITE	5	/* key exists and we cant overwrite */
#define TABLE_ERROR_NOT_FOUND	6	/* key does not exist */
#define TABLE_ERROR_ALLOC	7	/* memory allocation error */
#define TABLE_ERROR_LINEAR	8	/* no linear access started */
#define TABLE_ERROR_OPEN	9	/* could not open file */
#define TABLE_ERROR_SEEK	10	/* could not seek to pos in file */
#define TABLE_ERROR_READ	11	/* could not read from file */
#define TABLE_ERROR_WRITE	12	/* could not write to file */
#define TABLE_ERROR_MMAP_NONE	13	/* no mmap support */
#define TABLE_ERROR_MMAP	14	/* could not mmap file */
#define TABLE_ERROR_MMAP_OP	15	/* can't perform operation on mmap */
#define TABLE_ERROR_EMPTY	16	/* table is empty */
#define TABLE_ERROR_NOT_EMPTY	17	/* table contains data */
#define TABLE_ERROR_ALIGNMENT	18	/* invalid alignment value */
#define TABLE_ERROR_COMPARE	19	/* problems with internal comparison */
#define TABLE_ERROR_FREE	20	/* memory free error */

/*
 * Table flags set with table_attr.
 */

/*
 * Automatically adjust the number of table buckets on the fly.
 * Whenever the number of entries gets above some threshold, the
 * number of buckets is realloced to a new size and each entry is
 * re-hashed.  Although this may take some time when it re-hashes, the
 * table will perform better over time.
 */
#define TABLE_FLAG_AUTO_ADJUST	(1<<0)	

/*
 * If the above auto-adjust flag is set, also adjust the number of
 * table buckets down as we delete entries.
 */
#define TABLE_FLAG_ADJUST_DOWN	(1<<1)

/* structure to walk through the fields in a linear order */
typedef struct {
  unsigned int	tl_magic;	/* magic structure to ensure correct init */
  unsigned int	tl_bucket_c;	/* where in the table buck array we are */
  unsigned int	tl_entry_c;	/* in the bucket, which entry we are on */
} table_linear_t;

/*
 * int (*table_compare_t)
 *
 * DESCRIPTION
 *
 * Comparison function which compares two key/data pairs for table
 * order.
 *
 * RETURNS:
 *
 * -1, 0, or 1 if key1 is <, ==, or > than key2.
 *
 * ARGUMENTS:
 *
 * key1 - Pointer to the first key entry.
 *
 * key1_size - Pointer to the size of the first key entry.
 *
 * data1 - Pointer to the first data entry.
 *
 * data1_size - Pointer to the size of the first data entry.
 *
 * key2 - Pointer to the second key entry.
 *
 * key2_size - Pointer to the size of the second key entry.
 *
 * data2 - Pointer to the second data entry.
 *
 * data2_size - Pointer to the size of the second data entry.
 */
typedef int (*table_compare_t)(const void *key1, const int key1_size,
			       const void *data1, const int data1_size,
			       const void *key2, const int key2_size,
			       const void *data2, const int data2_size);

/*
 * int (*table_mem_alloc_t)
 *
 * DESCRIPTION
 *
 * Function to override the table's allocation function.
 *
 * RETURNS:
 *
 * Success - Newly allocated pointer.
 *
 * Failure - NULL
 *
 * ARGUMENTS:
 *
 * pool_p <-> Pointer to our memory pool.  If no pool is set then this
 * will be NULL.
 *
 * size -> Number of bytes that needs to be allocated.
 */
typedef void	*(*table_mem_alloc_t)(void *pool_p, const unsigned long size);

/*
 * int (*table_mem_resize_t)
 *
 * DESCRIPTION
 *
 * Function to override the table's memory resize function.  The
 * difference between this and realloc is that this provides the
 * previous allocation size.  You can specify NULL for this function
 * in which cause the library will allocate, copy, and free itself.
 *
 * RETURNS:
 *
 * Success - Newly allocated pointer.
 *
 * Failure - NULL
 *
 * ARGUMENTS:
 *
 * pool_p <-> Pointer to our memory pool.  If no pool is set then this
 * will be NULL.
 *
 * old_addr -> Previously allocated address.
 *
 * old_size -> Size of the old address.  Since the system is
 * lightweight, it does not store size information on the pointer.
 *
 * new_size -> New size of the allocation.
 */
typedef void	*(*table_mem_resize_t)(void *pool_p, void *old_addr,
				       const unsigned long old_size,
				       const unsigned long new_size);

/*
 * int (*table_mem_free_t)
 *
 * DESCRIPTION
 *
 * Function to override the table's free function.
 *
 * RETURNS:
 *
 * Success - 1
 *
 * Failure - 0
 *
 * ARGUMENTS:
 *
 * pool_p <-> Pointer to our memory pool.  If no pool is set then this
 * will be NULL.
 *
 * addr -> Address that we are freeing.
 *
 * min_size -> Minimum size of the address being freed or 0 if not
 * known.  This can also be the exact size if known.
 */
typedef int	(*table_mem_free_t)(void *pool_p, void *addr,
				    const unsigned long min_size);

#ifdef TABLE_MAIN

#include "table_loc.h"

#else

/* generic table type */
typedef	void	table_t;

/* generic table entry type */
typedef void	table_entry_t;

#endif

/*<<<<<<<<<<  The below prototypes are auto-generated by fillproto */

/*
 * table_t *table_alloc
 *
 * DESCRIPTION:
 *
 * Allocate a new table structure.
 *
 * RETURNS:
 *
 * A pointer to the new table structure which must be passed to
 * table_free to be deallocated.  On error a NULL is returned.
 *
 * ARGUMENTS:
 *
 * bucket_n - Number of buckets for the hash table.  Our current hash
 * value works best with base two numbers.  Set to 0 to take the
 * library default of 1024.
 *
 * error_p - Pointer to an integer which, if not NULL, will contain a
 * table error code.
 */
extern
table_t		*table_alloc(const unsigned int bucket_n, int *error_p);

/*
 * table_t *table_alloc_in_pool
 *
 * DESCRIPTION:
 *
 * Allocate a new table structure in a memory pool or using
 * alternative allocation and free functions.
 *
 * RETURNS:
 *
 * A pointer to the new table structure which must be passed to
 * table_free to be deallocated.  On error a NULL is returned.
 *
 * ARGUMENTS:
 *
 * bucket_n - Number of buckets for the hash table.  Our current hash
 * value works best with base two numbers.  Set to 0 to take the
 * library default of 1024.
 *
 * mem_pool <-> Memory pool to associate with the table.  Can be NULL.
 *
 * alloc_func -> Allocate function we are overriding malloc() with.
 *
 * resize_func -> Resize function we are overriding the standard
 * memory resize/realloc with.  This can be NULL in which cause the
 * library will allocate, copy, and free itself.
 *
 * free_func -> Free function we are overriding free() with.
 *
 * error_p - Pointer to an integer which, if not NULL, will contain a
 * table error code.
 */
extern
table_t		*table_alloc_in_pool(const unsigned int bucket_n,
				     void *mem_pool,
				     table_mem_alloc_t alloc_func,
				     table_mem_resize_t resize_func,
				     table_mem_free_t free_func, int *error_p);

/*
 * int table_attr
 *
 * DESCRIPTION:
 *
 * Set the attributes for the table.  The available attributes are
 * specified at the top of table.h.
 *
 * RETURNS:
 *
 * Success - TABLE_ERROR_NONE
 *
 * Failure - Table error code.
 *
 * ARGUMENTS:
 *
 * table_p - Pointer to a table structure which we will be altering.
 *
 * attr - Attribute(s) that we will be applying to the table.
 */
extern
int	table_attr(table_t *table_p, const int attr);

/*
 * int table_set_data_alignment
 *
 * DESCRIPTION:
 *
 * Set the alignment for the data in the table.  This is used when you
 * want to store binary data types and refer to them directly out of
 * the table storage.  For instance if you are storing integers as
 * data in the table and want to be able to retrieve the location of
 * the interger and then increment it as (*loc_p)++.  Otherwise you
 * would have to memcpy it out to an integer, increment it, and memcpy
 * it back.  If you are storing character data, no alignment is
 * necessary.
 *
 * For most data elements, sizeof(long) is recommended unless you use
 * smaller data types exclusively.
 *
 * WARNING: If necessary, you must set the data alignment before any
 * data gets put into the table.  Otherwise a TABLE_ERROR_NOT_EMPTY
 * error will be returned.
 *
 * NOTE: there is no way to set the key data alignment although it
 * should automatically be long aligned.
 *
 * RETURNS:
 *
 * Success - TABLE_ERROR_NONE
 *
 * Failure - Table error code.
 *
 * ARGUMENTS:
 *
 * table_p - Pointer to a table structure which we will be altering.
 *
 * alignment - Alignment requested for the data.  Must be a power of
 * 2.  Set to 0 for none.
 */
extern
int	table_set_data_alignment(table_t *table_p, const int alignment);

/*
 * int table_clear
 *
 * DESCRIPTION:
 *
 * Clear out and free all elements in a table structure.
 *
 * RETURNS:
 *
 * Success - TABLE_ERROR_NONE
 *
 * Failure - Table error code.
 *
 * ARGUMENTS:
 *
 * table_p - Table structure pointer that we will be clearing.
 */
extern
int	table_clear(table_t *table_p);

/*
 * int table_free
 *
 * DESCRIPTION:
 *
 * Deallocates a table structure.
 *
 * RETURNS:
 *
 * Success - TABLE_ERROR_NONE
 *
 * Failure - Table error code.
 *
 * ARGUMENTS:
 *
 * table_p - Table structure pointer that we will be freeing.
 */
extern
int	table_free(table_t *table_p);

/*
 * int table_insert_kd
 *
 * DESCRIPTION:
 *
 * Like table_insert except it passes back a pointer to the key and
 * the data buffers after they have been inserted into the table
 * structure.
 *
 * This routine adds a key/data pair both of which are made up of a
 * buffer of bytes and an associated size.  Both the key and the data
 * will be copied into buffers allocated inside the table.  If the key
 * exists already, the associated data will be replaced if the
 * overwrite flag is set, otherwise an error is returned.
 *
 * NOTE: be very careful changing the values since the table library
 * provides the pointers to its memory.  The key can _never_ be
 * changed otherwise you will not find it again.  The data can be
 * changed but its length can never be altered unless you delete and
 * re-insert it into the table.
 *
 * WARNING: The pointers to the key and data are not in any specific
 * alignment.  Accessing the key and/or data as an short, integer, or
 * long pointer directly can cause problems.
 *
 * WARNING: Replacing a data cell (not inserting) will cause the table
 * linked list to be temporarily invalid.  Care must be taken with
 * multiple threaded programs which are relying on the first/next
 * linked list to be always valid.
 *
 * RETURNS:
 *
 * Success - TABLE_ERROR_NONE
 *
 * Failure - Table error code.
 *
 * ARGUMENTS:
 *
 * table_p - Table structure pointer into which we will be inserting a
 * new key/data pair.
 *
 * key_buf - Buffer of bytes of the key that we are inserting.  If you
 * are storing an (int) as the key (for example) then key_buf should
 * be a (int *).
 *
 * key_size - Size of the key_buf buffer.  If set to < 0 then the
 * library will do a strlen of key_buf and add 1 for the '\0'.  If you
 * are storing an (int) as the key (for example) then key_size should
 * be sizeof(int).
 *
 * data_buf - Buffer of bytes of the data that we are inserting.  If
 * it is NULL then the library will allocate space for the data in the
 * table without copying in any information.  If data_buf is NULL and
 * data_size is 0 then the library will associate a NULL data pointer
 * with the key.  If you are storing a (long) as the data (for
 * example) then data_buf should be a (long *).
 *
 * data_size - Size of the data_buf buffer.  If set to < 0 then the
 * library will do a strlen of data_buf and add 1 for the '\0'.  If
 * you are storing an (long) as the key (for example) then key_size
 * should be sizeof(long).
 *
 * key_buf_p - Pointer which, if not NULL, will be set to the address
 * of the key storage that was allocated in the table.  If you are
 * storing an (int) as the key (for example) then key_buf_p should be
 * (int **) i.e. the address of a (int *).
 *
 * data_buf_p - Pointer which, if not NULL, will be set to the address
 * of the data storage that was allocated in the table.  If you are
 * storing an (long) as the data (for example) then data_buf_p should
 * be (long **) i.e. the address of a (long *).
 *
 * overwrite - Flag which, if set to 1, will allow the overwriting of
 * the data in the table with the new data if the key already exists
 * in the table.
 */
extern
int	table_insert_kd(table_t *table_p,
			const void *key_buf, const int key_size,
			const void *data_buf, const int data_size,
			void **key_buf_p, void **data_buf_p,
			const char overwrite_b);

/*
 * int table_insert
 *
 * DESCRIPTION:
 *
 * Exactly the same as table_insert_kd except it does not pass back a
 * pointer to the key after they have been inserted into the table
 * structure.  This is still here for backwards compatibility.
 *
 * See table_insert_kd for more information.
 *
 * RETURNS:
 *
 * Success - TABLE_ERROR_NONE
 *
 * Failure - Table error code.
 *
 * ARGUMENTS:
 *
 * table_p - Table structure pointer into which we will be inserting a
 * new key/data pair.
 *
 * key_buf - Buffer of bytes of the key that we are inserting.  If you
 * are storing an (int) as the key (for example) then key_buf should
 * be a (int *).
 *
 * key_size - Size of the key_buf buffer.  If set to < 0 then the
 * library will do a strlen of key_buf and add 1 for the '\0'.  If you
 * are storing an (int) as the key (for example) then key_size should
 * be sizeof(int).
 *
 * data_buf - Buffer of bytes of the data that we are inserting.  If
 * it is NULL then the library will allocate space for the data in the
 * table without copying in any information.  If data_buf is NULL and
 * data_size is 0 then the library will associate a NULL data pointer
 * with the key.  If you are storing a (long) as the data (for
 * example) then data_buf should be a (long *).
 *
 * data_size - Size of the data_buf buffer.  If set to < 0 then the
 * library will do a strlen of data_buf and add 1 for the '\0'.  If
 * you are storing an (long) as the key (for example) then key_size
 * should be sizeof(long).
 *
 * data_buf_p - Pointer which, if not NULL, will be set to the address
 * of the data storage that was allocated in the table.  If you are
 * storing an (long) as the data (for example) then data_buf_p should
 * be (long **) i.e. the address of a (long *).
 *
 * overwrite - Flag which, if set to 1, will allow the overwriting of
 * the data in the table with the new data if the key already exists
 * in the table.
 */
extern
int	table_insert(table_t *table_p,
		     const void *key_buf, const int key_size,
		     const void *data_buf, const int data_size,
		     void **data_buf_p, const char overwrite_b);

/*
 * int table_retrieve
 *
 * DESCRIPTION:
 *
 * This routine looks up a key made up of a buffer of bytes and an
 * associated size in the table.  If found then it returns the
 * associated data information.
 *
 * RETURNS:
 *
 * Success - TABLE_ERROR_NONE
 *
 * Failure - Table error code.
 *
 * ARGUMENTS:
 *
 * table_p - Table structure pointer into which we will be searching
 * for the key.
 *
 * key_buf - Buffer of bytes of the key that we are searching for.  If
 * you are looking for an (int) as the key (for example) then key_buf
 * should be a (int *).
 *
 * key_size - Size of the key_buf buffer.  If set to < 0 then the
 * library will do a strlen of key_buf and add 1 for the '\0'.  If you
 * are looking for an (int) as the key (for example) then key_size
 * should be sizeof(int).
 *
 * data_buf_p - Pointer which, if not NULL, will be set to the address
 * of the data storage that was allocated in the table and that is
 * associated with the key.  If a (long) was stored as the data (for
 * example) then data_buf_p should be (long **) i.e. the address of a
 * (long *).
 *
 * data_size_p - Pointer to an integer which, if not NULL, will be set
 * to the size of the data stored in the table that is associated with
 * the key.
 */
extern
int	table_retrieve(table_t *table_p,
		       const void *key_buf, const int key_size,
		       void **data_buf_p, int *data_size_p);

/*
 * int table_delete
 *
 * DESCRIPTION:
 *
 * This routine looks up a key made up of a buffer of bytes and an
 * associated size in the table.  If found then it will be removed
 * from the table.  The associated data can be passed back to the user
 * if requested.
 *
 * RETURNS:
 *
 * Success - TABLE_ERROR_NONE
 *
 * Failure - Table error code.
 *
 * NOTE: this could be an allocation error if the library is to return
 * the data to the user.
 *
 * ARGUMENTS:
 *
 * table_p - Table structure pointer from which we will be deleteing
 * the key.
 *
 * key_buf - Buffer of bytes of the key that we are searching for to
 * delete.  If you are deleting an (int) key (for example) then
 * key_buf should be a (int *).
 *
 * key_size - Size of the key_buf buffer.  If set to < 0 then the
 * library will do a strlen of key_buf and add 1 for the '\0'.  If you
 * are deleting an (int) key (for example) then key_size should be
 * sizeof(int).
 *
 * data_buf_p - Pointer which, if not NULL, will be set to the address
 * of the data storage that was allocated in the table and that was
 * associated with the key.  If a (long) was stored as the data (for
 * example) then data_buf_p should be (long **) i.e. the address of a
 * (long *).  If a pointer is passed in, the caller is responsible for
 * freeing it after use.  If data_buf_p is NULL then the library will
 * free up the data allocation itself.
 *
 * data_size_p - Pointer to an integer which, if not NULL, will be set
 * to the size of the data that was stored in the table and that was
 * associated with the key.
 */
extern
int	table_delete(table_t *table_p,
		     const void *key_buf, const int key_size,
		     void **data_buf_p, int *data_size_p);

/*
 * int table_delete_first
 *
 * DESCRIPTION:
 *
 * This is like the table_delete routines except it deletes the first
 * key/data pair in the table instead of an entry corresponding to a
 * particular key.  The associated key and data information can be
 * passed back to the user if requested.  This routines is handy to
 * clear out a table.
 *
 * RETURNS:
 *
 * Success - TABLE_ERROR_NONE
 *
 * Failure - Table error code.
 *
 * NOTE: this could be an allocation error if the library is to return
 * the data to the user.
 *
 * ARGUMENTS:
 *
 * table_p - Table structure pointer from which we will be deleteing
 * the first key.
 *
 * key_buf_p - Pointer which, if not NULL, will be set to the address
 * of the storage of the first key that was allocated in the table.
 * If an (int) was stored as the first key (for example) then
 * key_buf_p should be (int **) i.e. the address of a (int *).  If a
 * pointer is passed in, the caller is responsible for freeing it
 * after use.  If key_buf_p is NULL then the library will free up the
 * key allocation itself.
 *
 * key_size_p - Pointer to an integer which, if not NULL, will be set
 * to the size of the key that was stored in the table and that was
 * associated with the key.
 *
 * data_buf_p - Pointer which, if not NULL, will be set to the address
 * of the data storage that was allocated in the table and that was
 * associated with the key.  If a (long) was stored as the data (for
 * example) then data_buf_p should be (long **) i.e. the address of a
 * (long *).  If a pointer is passed in, the caller is responsible for
 * freeing it after use.  If data_buf_p is NULL then the library will
 * free up the data allocation itself.
 *
 * data_size_p - Pointer to an integer which, if not NULL, will be set
 * to the size of the data that was stored in the table and that was
 * associated with the key.
 */
extern
int	table_delete_first(table_t *table_p,
			   void **key_buf_p, int *key_size_p,
			   void **data_buf_p, int *data_size_p);

/*
 * int table_info
 *
 * DESCRIPTION:
 *
 * Get some information about a table_p structure.
 *
 * RETURNS:
 *
 * Success - TABLE_ERROR_NONE
 *
 * Failure - Table error code.
 *
 * ARGUMENTS:
 *
 * table_p - Table structure pointer from which we are getting
 * information.
 *
 * num_buckets_p - Pointer to an integer which, if not NULL, will
 * contain the number of buckets in the table.
 *
 * num_entries_p - Pointer to an integer which, if not NULL, will
 * contain the number of entries stored in the table.
 */
extern
int	table_info(table_t *table_p, int *num_buckets_p, int *num_entries_p);

/*
 * int table_adjust
 *
 * DESCRIPTION:
 *
 * Set the number of buckets in a table to a certain value.
 *
 * RETURNS:
 *
 * Success - TABLE_ERROR_NONE
 *
 * Failure - Table error code.
 *
 * ARGUMENTS:
 *
 * table_p - Table structure pointer of which we are adjusting.
 *
 * bucket_n - Number buckets to adjust the table to.  Set to 0 to
 * adjust the table to its number of entries.
 */
extern
int	table_adjust(table_t *table_p, const int bucket_n);

/*
 * int table_type_size
 *
 * DESCRIPTION:
 *
 * Return the size of the internal table type.
 *
 * RETURNS:
 *
 * The size of the table_t type.
 *
 * ARGUMENTS:
 *
 * None.
 */
extern
int	table_type_size(void);

/*
 * int table_first
 *
 * DESCRIPTION:
 *
 * Find first element in a table and pass back information about the
 * key/data pair.  If any of the key/data pointers are NULL then they
 * are ignored.
 *
 * NOTE: This function is not reentrant.  More than one thread cannot
 * be doing a first and next on the same table at the same time.  Use
 * the table_first_r version below for this.
 *
 * RETURNS:
 *
 * Success - TABLE_ERROR_NONE
 *
 * Failure - Table error code.
 *
 * ARGUMENTS:
 *
 * table_p - Table structure pointer from which we are getting the
 * first element.
 *
 * key_buf_p - Pointer which, if not NULL, will be set to the address
 * of the storage of the first key that is allocated in the table.  If
 * an (int) is stored as the first key (for example) then key_buf_p
 * should be (int **) i.e. the address of a (int *).
 *
 * key_size_p - Pointer to an integer which, if not NULL, will be set
 * to the size of the key that is stored in the table and that is
 * associated with the first key.
 *
 * data_buf_p - Pointer which, if not NULL, will be set to the address
 * of the data storage that is allocated in the table and that is
 * associated with the first key.  If a (long) is stored as the data
 * (for example) then data_buf_p should be (long **) i.e. the address
 * of a (long *).
 *
 * data_size_p - Pointer to an integer which, if not NULL, will be set
 * to the size of the data that is stored in the table and that is
 * associated with the first key.
 */
extern
int	table_first(table_t *table_p,
		    void **key_buf_p, int *key_size_p,
		    void **data_buf_p, int *data_size_p);

/*
 * int table_next
 *
 * DESCRIPTION:
 *
 * Find the next element in a table and pass back information about
 * the key/data pair.  If any of the key/data pointers are NULL then
 * they are ignored.
 *
 * NOTE: This function is not reentrant.  More than one thread cannot
 * be doing a first and next on the same table at the same time.  Use
 * the table_next_r version below for this.
 *
 * RETURNS:
 *
 * Success - TABLE_ERROR_NONE
 *
 * Failure - Table error code.
 *
 * ARGUMENTS:
 *
 * table_p - Table structure pointer from which we are getting the
 * next element.
 *
 * key_buf_p - Pointer which, if not NULL, will be set to the address
 * of the storage of the next key that is allocated in the table.  If
 * an (int) is stored as the next key (for example) then key_buf_p
 * should be (int **) i.e. the address of a (int *).
 *
 * key_size_p - Pointer to an integer which, if not NULL, will be set
 * to the size of the key that is stored in the table and that is
 * associated with the next key.
 *
 * data_buf_p - Pointer which, if not NULL, will be set to the address
 * of the data storage that is allocated in the table and that is
 * associated with the next key.  If a (long) is stored as the data
 * (for example) then data_buf_p should be (long **) i.e. the address
 * of a (long *).
 *
 * data_size_p - Pointer to an integer which, if not NULL, will be set
 * to the size of the data that is stored in the table and that is
 * associated with the next key.
 */
extern
int	table_next(table_t *table_p,
		   void **key_buf_p, int *key_size_p,
		   void **data_buf_p, int *data_size_p);

/*
 * int table_this
 *
 * DESCRIPTION:
 *
 * Find the current element in a table and pass back information about
 * the key/data pair.  If any of the key/data pointers are NULL then
 * they are ignored.
 *
 * NOTE: This function is not reentrant.  Use the table_current_r
 * version below.
 *
 * RETURNS:
 *
 * Success - TABLE_ERROR_NONE
 *
 * Failure - Table error code.
 *
 * ARGUMENTS:
 *
 * table_p - Table structure pointer from which we are getting the
 * current element.
 *
 * key_buf_p - Pointer which, if not NULL, will be set to the address
 * of the storage of the current key that is allocated in the table.
 * If an (int) is stored as the current key (for example) then
 * key_buf_p should be (int **) i.e. the address of a (int *).
 *
 * key_size_p - Pointer to an integer which, if not NULL, will be set
 * to the size of the key that is stored in the table and that is
 * associated with the current key.
 *
 * data_buf_p - Pointer which, if not NULL, will be set to the address
 * of the data storage that is allocated in the table and that is
 * associated with the current key.  If a (long) is stored as the data
 * (for example) then data_buf_p should be (long **) i.e. the address
 * of a (long *).
 *
 * data_size_p - Pointer to an integer which, if not NULL, will be set
 * to the size of the data that is stored in the table and that is
 * associated with the current key.
 */
extern
int	table_this(table_t *table_p,
		   void **key_buf_p, int *key_size_p,
		   void **data_buf_p, int *data_size_p);

/*
 * int table_first_r
 *
 * DESCRIPTION:
 *
 * Reetrant version of the table_first routine above.  Find first
 * element in a table and pass back information about the key/data
 * pair.  If any of the key/data pointers are NULL then they are
 * ignored.
 *
 * RETURNS:
 *
 * Success - TABLE_ERROR_NONE
 *
 * Failure - Table error code.
 *
 * ARGUMENTS:
 *
 * table_p - Table structure pointer from which we are getting the
 * first element.
 *
 * linear_p - Pointer to a table linear structure which is initialized
 * here.  The same pointer should then be passed to table_next_r
 * below.
 *
 * key_buf_p - Pointer which, if not NULL, will be set to the address
 * of the storage of the first key that is allocated in the table.  If
 * an (int) is stored as the first key (for example) then key_buf_p
 * should be (int **) i.e. the address of a (int *).
 *
 * key_size_p - Pointer to an integer which, if not NULL, will be set
 * to the size of the key that is stored in the table and that is
 * associated with the first key.
 *
 * data_buf_p - Pointer which, if not NULL, will be set to the address
 * of the data storage that is allocated in the table and that is
 * associated with the first key.  If a (long) is stored as the data
 * (for example) then data_buf_p should be (long **) i.e. the address
 * of a (long *).
 *
 * data_size_p - Pointer to an integer which, if not NULL, will be set
 * to the size of the data that is stored in the table and that is
 * associated with the first key.
 */
extern
int	table_first_r(table_t *table_p, table_linear_t *linear_p,
		      void **key_buf_p, int *key_size_p,
		      void **data_buf_p, int *data_size_p);

/*
 * int table_next_r
 *
 * DESCRIPTION:
 *
 * Reetrant version of the table_next routine above.  Find next
 * element in a table and pass back information about the key/data
 * pair.  If any of the key/data pointers are NULL then they are
 * ignored.
 *
 * RETURNS:
 *
 * Success - TABLE_ERROR_NONE
 *
 * Failure - Table error code.
 *
 * ARGUMENTS:
 *
 * table_p - Table structure pointer from which we are getting the
 * next element.
 *
 * linear_p - Pointer to a table linear structure which is incremented
 * here.  The same pointer must have been passed to table_first_r
 * first so that it can be initialized.
 *
 * key_buf_p - Pointer which, if not NULL, will be set to the address
 * of the storage of the next key that is allocated in the table.  If
 * an (int) is stored as the next key (for example) then key_buf_p
 * should be (int **) i.e. the address of a (int *).
 *
 * key_size_p - Pointer to an integer which, if not NULL will be set
 * to the size of the key that is stored in the table and that is
 * associated with the next key.
 *
 * data_buf_p - Pointer which, if not NULL, will be set to the address
 * of the data storage that is allocated in the table and that is
 * associated with the next key.  If a (long) is stored as the data
 * (for example) then data_buf_p should be (long **) i.e. the address
 * of a (long *).
 *
 * data_size_p - Pointer to an integer which, if not NULL, will be set
 * to the size of the data that is stored in the table and that is
 * associated with the next key.
 */
extern
int	table_next_r(table_t *table_p, table_linear_t *linear_p,
		     void **key_buf_p, int *key_size_p,
		     void **data_buf_p, int *data_size_p);

/*
 * int table_this_r
 *
 * DESCRIPTION:
 *
 * Reetrant version of the table_this routine above.  Find current
 * element in a table and pass back information about the key/data
 * pair.  If any of the key/data pointers are NULL then they are
 * ignored.
 *
 * RETURNS:
 *
 * Success - TABLE_ERROR_NONE
 *
 * Failure - Table error code.
 *
 * ARGUMENTS:
 *
 * table_p - Table structure pointer from which we are getting the
 * current element.
 *
 * linear_p - Pointer to a table linear structure which is accessed
 * here.  The same pointer must have been passed to table_first_r
 * first so that it can be initialized.
 *
 * key_buf_p - Pointer which, if not NULL, will be set to the address
 * of the storage of the current key that is allocated in the table.
 * If an (int) is stored as the current key (for example) then
 * key_buf_p should be (int **) i.e. the address of a (int *).
 *
 * key_size_p - Pointer to an integer which, if not NULL, will be set
 * to the size of the key that is stored in the table and that is
 * associated with the current key.
 *
 * data_buf_p - Pointer which, if not NULL, will be set to the address
 * of the data storage that is allocated in the table and that is
 * associated with the current key.  If a (long) is stored as the data
 * (for example) then data_buf_p should be (long **) i.e. the address
 * of a (long *).
 *
 * data_size_p - Pointer to an integer which, if not NULL, will be set
 * to the size of the data that is stored in the table and that is
 * associated with the current key.
 */
extern
int	table_this_r(table_t *table_p, table_linear_t *linear_p,
		     void **key_buf_p, int *key_size_p,
		     void **data_buf_p, int *data_size_p);

/*
 * table_t *table_mmap
 *
 * DESCRIPTION:
 *
 * Mmap a table from a file that had been written to disk earlier via
 * table_write.
 *
 * RETURNS:
 *
 * A pointer to the new table structure which must be passed to
 * table_munmap to be deallocated.  On error a NULL is returned.
 *
 * ARGUMENTS:
 *
 * path - Table file to mmap in.
 *
 * error_p - Pointer to an integer which, if not NULL, will contain a
 * table error code.
 */
extern
table_t		*table_mmap(const char *path, int *error_p);

/*
 * int table_munmap
 *
 * DESCRIPTION:
 *
 * Unmmap a table that was previously mmapped using table_mmap.
 *
 * RETURNS:
 *
 * Returns table error codes.
 *
 * ARGUMENTS:
 *
 * table_p - Mmaped table pointer to unmap.
 */
extern
int	table_munmap(table_t *table_p);

/*
 * int table_read
 *
 * DESCRIPTION:
 *
 * Read in a table from a file that had been written to disk earlier
 * via table_write.
 *
 * RETURNS:
 *
 * Success - Pointer to the new table structure which must be passed
 * to table_free to be deallocated.
 *
 * Failure - NULL
 *
 * ARGUMENTS:
 *
 * path - Table file to read in.
 *
 * error_p - Pointer to an integer which, if not NULL, will contain a
 * table error code.
 */
extern
table_t	*table_read(const char *path, int *error_p);

/*
 * int table_write
 *
 * DESCRIPTION:
 *
 * Write a table from memory to file.
 *
 * RETURNS:
 *
 * Success - TABLE_ERROR_NONE
 *
 * Failure - Table error code.
 *
 * ARGUMENTS:
 *
 * table_p - Pointer to the table that we are writing to the file.
 *
 * path - Table file to write out to.
 *
 * mode - Mode of the file.  This argument is passed on to open when
 * the file is created.
 */
extern
int	table_write(const table_t *table_p, const char *path, const int mode);

/*
 * table_entry_t *table_order
 *
 * DESCRIPTION:
 *
 * Order a table by building an array of table entry pointers and then
 * sorting this array using the qsort function.  To retrieve the
 * sorted entries, you can then use the table_entry routine to access
 * each entry in order.
 *
 * NOTE: This routine is thread safe and makes use of an internal
 * status qsort function.
 *
 * RETURNS:
 *
 * Success - An allocated list of table-linear structures which must
 * be freed by table_order_free later.
 *
 * Failure - NULL
 *
 * ARGUMENTS:
 *
 * table_p - Pointer to the table that we are ordering.
 *
 * compare - Comparison function defined by the user.  Its definition
 * is at the top of the table.h file.  If this is NULL then it will
 * order the table my memcmp-ing the keys.
 *
 * num_entries_p - Pointer to an integer which, if not NULL, will
 * contain the number of entries in the returned entry pointer array.
 *
 * error_p - Pointer to an integer which, if not NULL, will contain a
 * table error code.
 */
extern
table_entry_t	**table_order(table_t *table_p, table_compare_t compare,
			      int *num_entries_p, int *error_p);

/*
 * int table_order_free
 *
 * DESCRIPTION:
 *
 * Free the pointer returned by the table_order or table_order_pos
 * routines.
 *
 * RETURNS:
 *
 * Success - TABLE_ERROR_NONE
 *
 * Failure - Table error code.
 *
 * ARGUMENTS:
 *
 * table_p - Pointer to the table.
 *
 * table_entries - Allocated list of entry pointers returned by
 * table_order.
 *
 * entry_n - Number of entries in the array as passed back by
 * table_order or table_order_pos in num_entries_p.
 */
extern
int	table_order_free(table_t *table_p, table_entry_t **table_entries,
			 const int entry_n);

/*
 * int table_entry
 *
 * DESCRIPTION:
 *
 * Get information about an element.  The element is one from the
 * array returned by the table_order function.  If any of the key/data
 * pointers are NULL then they are ignored.
 *
 * RETURNS:
 *
 * Success - TABLE_ERROR_NONE
 *
 * Failure - Table error code.
 *
 * ARGUMENTS:
 *
 * table_p - Table structure pointer from which we are getting the
 * element.
 *
 * entry_p - Pointer to a table entry from the array returned by the
 * table_order function.
 *
 * key_buf_p - Pointer which, if not NULL, will be set to the address
 * of the storage of this entry that is allocated in the table.  If an
 * (int) is stored as this entry (for example) then key_buf_p should
 * be (int **) i.e. the address of a (int *).
 *
 * key_size_p - Pointer to an integer which, if not NULL, will be set
 * to the size of the key that is stored in the table.
 *
 * data_buf_p - Pointer which, if not NULL, will be set to the address
 * of the data storage of this entry that is allocated in the table.
 * If a (long) is stored as this entry data (for example) then
 * data_buf_p should be (long **) i.e. the address of a (long *).
 *
 * data_size_p - Pointer to an integer which, if not NULL, will be set
 * to the size of the data that is stored in the table.
 */
extern
int	table_entry(table_t *table_p, table_entry_t *entry_p,
		    void **key_buf_p, int *key_size_p,
		    void **data_buf_p, int *data_size_p);

/*
 * table_linear_t *table_order_pos
 *
 * DESCRIPTION:
 *
 * Order a table by building an array of table linear structures and
 * then sorting this array using the qsort function.  To retrieve the
 * sorted entries, you can then use the table_entry_pos routine to
 * access each entry in order.
 *
 * NOTE: This routine is thread safe and makes use of an internal
 * status qsort function.
 *
 * RETURNS:
 *
 * Success - An allocated list of table-linear structures which must
 * be freed by table_order_pos_free later.
 *
 * Failure - NULL
 *
 * ARGUMENTS:
 *
 * table_p - Pointer to the table that we are ordering.
 *
 * compare - Comparison function defined by the user.  Its definition
 * is at the top of the table.h file.  If this is NULL then it will
 * order the table my memcmp-ing the keys.
 *
 * num_entries_p - Pointer to an integer which, if not NULL, will
 * contain the number of entries in the returned entry pointer array.
 *
 * error_p - Pointer to an integer which, if not NULL, will contain a
 * table error code.
 */
extern
table_linear_t	*table_order_pos(table_t *table_p, table_compare_t compare,
				 int *num_entries_p, int *error_p);

/*
 * int table_order_pos_free
 *
 * DESCRIPTION:
 *
 * Free the pointer returned by the table_order or table_order_pos
 * routines.
 *
 * RETURNS:
 *
 * Success - TABLE_ERROR_NONE
 *
 * Failure - Table error code.
 *
 * ARGUMENTS:
 *
 * table_p - Pointer to the table.
 *
 * table_entries - Allocated list of entry pointers returned by
 * table_order_pos.
 *
 * entry_n - Number of entries in the array as passed back by
 * table_order or table_order_pos in num_entries_p.
 */
extern
int	table_order_pos_free(table_t *table_p, table_linear_t *table_entries,
			     const int entry_n);

/*
 * int table_entry_pos
 *
 * DESCRIPTION:
 *
 * Get information about an element.  The element is one from the
 * array returned by the table_order function.  If any of the key/data
 * pointers are NULL then they are ignored.
 *
 * RETURNS:
 *
 * Success - TABLE_ERROR_NONE
 *
 * Failure - Table error code.
 *
 * ARGUMENTS:
 *
 * table_p - Table structure pointer from which we are getting the
 * element.
 *
 * linear_p - Pointer to a table linear structure from the array
 * returned by the table_order function.
 *
 * key_buf_p - Pointer which, if not NULL, will be set to the address
 * of the storage of this entry that is allocated in the table.  If an
 * (int) is stored as this entry (for example) then key_buf_p should
 * be (int **) i.e. the address of a (int *).
 *
 * key_size_p - Pointer to an integer which, if not NULL, will be set
 * to the size of the key that is stored in the table.
 *
 * data_buf_p - Pointer which, if not NULL, will be set to the address
 * of the data storage of this entry that is allocated in the table.
 * If a (long) is stored as this entry data (for example) then
 * data_buf_p should be (long **) i.e. the address of a (long *).
 *
 * data_size_p - Pointer to an integer which, if not NULL, will be set
 * to the size of the data that is stored in the table.
 */
extern
int	table_entry_pos(table_t *table_p, table_linear_t *linear_p,
			void **key_buf_p, int *key_size_p,
			void **data_buf_p, int *data_size_p);

/*
 * const char *table_strerror
 *
 * DESCRIPTION:
 *
 * Return the corresponding string for the error number.
 *
 * RETURNS:
 *
 * Success - String equivalient of the error.
 *
 * Failure - String "invalid error code"
 *
 * ARGUMENTS:
 *
 * error - Error number that we are converting.
 */
extern
const char	*table_strerror(const int error);

/*<<<<<<<<<<   This is end of the auto-generated output from fillproto. */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* ! __TABLE_H__ */
