/* Copyright information is at the end of the file */
#ifndef  XMLRPC_TRANSPORT_INT_H_INCLUDED
#define  XMLRPC_TRANSPORT_INT_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

#include "pthreadx.h" /* For threading helpers. */

/*=========================================================================
**  Transport Helper Functions and declarations.
**=========================================================================
*/
typedef struct _running_thread_info
{
    struct _running_thread_info * Next;
    struct _running_thread_info * Last;

    pthread_t _thread;
} running_thread_info;


/* list of running Async callback functions. */
typedef struct _running_thread_list
{
    running_thread_info * AsyncThreadHead;
    running_thread_info * AsyncThreadTail;
} running_thread_list;

/* MRB-WARNING: Only call when you have successfully
**     acquired the Lock/Unlock mutex! */
void register_asynch_thread (running_thread_list *list, pthread_t *thread);

/* MRB-WARNING: Only call when you have successfully
**     acquired the Lock/Unlock mutex! */
void unregister_asynch_thread (running_thread_list *list, pthread_t *thread);


#ifdef __cplusplus
}
#endif
