/*
 * libZRTP SDK library, implements the ZRTP secure VoIP protocol.
 * Copyright (c) 2006-2009 Philip R. Zimmermann.  All rights reserved.
 * Contact: http://philzimmermann.com
 * For licensing and other legal details, see the file zrtp_legal.c.
 * 
 * Viktor Krykun <v.krikun at zfoneproject.com> 
 */


/**
 * \file zrtp_iface_system.h
 * \brief libzrtp platform-dependent routine
 */

#ifndef __ZRTP_IFACE_SYSTEM_H__ 
#define __ZRTP_IFACE_SYSTEM_H__

#include "zrtp_config.h"
#include "zrtp_types.h"

#if defined(__cplusplus)
extern "C"
{
#endif


/*============================================================================*/
/*     System wide functions                                                  */
/*============================================================================*/

/**
 * \defgroup zrtp_iface Library Interfaces Overview
 * 
 * This section describes the requirements for the implementation of each interface function.
 * Descriptions are divided into groups by function
 */
 
/**
 * \defgroup zrtp_iface_base Basic platform-dependent routine
 * \ingroup zrtp_iface
 * \{
 */

/**
 * \brief Time in miliseconds
 *
 * libzrtp uses a unix-like time calculation scheme: time since 1/1/1970.
 */
typedef uint64_t	zrtp_time_t;


/**
 * \brief Allocates memory of a defined size
 *
 * Allocates \c size bytes and returns a pointer to the allocated memory Allocated memory is not 
 * cleared.
 *
 * \param size - number of bytes for allocation
 * \return 
 *  - pointer to the allocated memory if successful.
 *  - NULL if the memory allocation failed.
 */
extern void* zrtp_sys_alloc(unsigned int size);

/**
 * \brief release memory
 *
 * Release the memory space pointed to by \c obj, which was returned by a previous zrtp_sys_alloc() 
 * call. If \c obj is NULL, no operation is performed.
 *
 * \param obj - pointer to the released memory
 */
extern void  zrtp_sys_free(void* obj);

/**
 * \brief Memory copying function.
 *
 * This function copies \c length bytes from memory area \c src to memory area \c dest. The memory 
 * areas should not overlap.
 *
 * \param dest - pointer to the destination buffer
 * \param src - pointer to the source buffer;
 * \param length - number of bytes to be copied.
 * \return
 *  - pointer to the destination buffer (dest)
 */
extern void* zrtp_memcpy(void* dest, const void* src, unsigned int length);	

/**
 * \brief Write a byte to a byte string
 *
 * The zrtp_memset() function writes \c n bytes of value \c c (converted to an unsigned char) to the 
 * string \c s.
 * \return 
 *	- first argument
 */
extern void *zrtp_memset(void *s, int c, unsigned int n);

/**
 * \brief Returns current date and time
 *
 * This function should return current unix-like date and time: number of microseconds since 
 * 1.1.1970.
 */
extern zrtp_time_t zrtp_time_now();

/** \} */

/*============================================================================*/
/*    Mutex related interfaces                                                */
/*============================================================================*/
	
/**
 * \defgroup zrtp_iface_mutex Synchronization related functions
 * \ingroup zrtp_iface
 * \{
 */

/**
 * \brief Initializing the mutex structure 
 *
 * This function allocates and initializes the mutex referenced by \c mutex with default attributes. 
 * Upon  successful initialization, the state of the mutex becomes initialized and unlocked. This 
 * function should create a NON RECURSIVE mutex. (Attempting to relock the mutex causes deadlock)
 *
 * \param mutex - out parameter, mutex structure for allocation and initialization
 * \return:
 *  - zrtp_status_ok if initialization successful;
 *  - zrtp_status_fail if an error occurred.
 * \sa zrtp_mutex_destroy()
 */
extern zrtp_status_t zrtp_mutex_init(zrtp_mutex_t** mutex);
	
/**
 * \brief Deinitializing the mutex structure 
 *
 * This function destroys the mutex object previously allocated by zrtp_mutex_init().
 *
 * \param mutex - mutex structure for deinitialization.
 * \return:
 *  - zrtp_status_ok if deinitialization successful;
 *  - zrtp_status_fail if an error occurred.
 * \sa zrtp_mutex_init()
 */
 extern zrtp_status_t zrtp_mutex_destroy(zrtp_mutex_t* mutex);

/**
 * \brief Mutex locking
 *
 * This function locks the mutex object referenced by \c mutex.  If the mutex is already locked, the 
 * thread that called it is blocked until the mutex becomes available.  This operation returns the 
 * mutex object referenced by the mutex in the locked state with the calling thread as its owner.
 *
 * \param mutex - mutex for locking;
 * \return:
 *  - zrtp_status_ok if successful;
 *  - zrtp_status_fail if an error occurred.
 */
extern zrtp_status_t zrtp_mutex_lock(zrtp_mutex_t* mutex);

/**
 * \brief Mutex releasing
 *
 * This function releases the mutex object referenced by mutex. The way a mutex is released depends 
 * on the mutex's type attribute. If there are threads blocked on the mutex object referenced by 
 * mutex when zrtp_mutex_unlock() is called and the mutex becomes available, the scheduling policy 
 * determines which thread acquires the mutex.
 *
 * \param mutex - mutex to release
 * \return:
 *  - zrtp_status_ok if successful;
 *  - zrtp_status_fail if an error occurred.
 */
extern zrtp_status_t zrtp_mutex_unlock(zrtp_mutex_t* mutex);

/*! \} */

#if defined(__cplusplus)
}
#endif

#endif /* __ZRTP_IFACE_SYSTEM_H__ */
