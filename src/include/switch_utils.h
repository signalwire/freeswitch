/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005/2006, Anthony Minessale II <anthmct@yahoo.com>
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Anthony Minessale II <anthmct@yahoo.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthmct@yahoo.com>
 *
 *
 * switch_utils.h -- Compatability and Helper Code
 *
 */
/*! \file switch_utils.h
    \brief Compatability and Helper Code

	Just a miscelaneaous set of general utility/helper functions.

*/
#ifndef SWITCH_UTILS_H
#define SWITCH_UTILS_H

#include <switch.h>

BEGIN_EXTERN_C

#ifndef snprintf
#define snprintf apr_snprintf
#endif
#ifndef vsnprintf
#define vsnprintf apr_vsnprintf
#endif


/*!
  \brief Evaluate the truthfullness of a string expression
  \param expr a string expression
  \return true or false 
*/
#define switch_true(expr)\
(expr && ( !strcasecmp(expr, "yes") ||\
!strcasecmp(expr, "on") ||\
!strcasecmp(expr, "true") ||\
atoi(expr))) ? SWITCH_TRUE : SWITCH_FALSE


SWITCH_DECLARE(switch_status_t) switch_socket_recvfrom(switch_sockaddr_t *from, switch_socket_t *sock, int32_t flags, char *buf, switch_size_t *len);

/*!
  \brief Return a printable name of a switch_priority_t
  \param priority the priority to get the name of
  \return the printable form of the priority
*/
SWITCH_DECLARE(char *) switch_priority_name(switch_priority_t priority);

/*!
  \brief Return the RFC2833 character based on an event id
  \param event the event id to convert
  \return the character represented by the event or null for an invalid event
*/
SWITCH_DECLARE(char) switch_rfc2833_to_char(int event);

/*!
  \brief Return the RFC2833 event based on an key character
  \param key the charecter to encode
  \return the event id for the specified character or -1 on an invalid input
*/
SWITCH_DECLARE(unsigned char) switch_char_to_rfc2833(char key);

/*!
  \brief determine if a character is a valid DTMF key
  \param key the key to test
  \return TRUE or FALSE
 */
#define is_dtmf(key)  ((key > 47 && key < 58) || (key > 64 && key < 69) || (key > 96 && key < 101) || key == 35 || key == 42)

/*!
  \brief Duplicate a string 
*/
#define switch_copy_string apr_cpystrn

/*!
  \brief Test for the existance of a flag on an arbitary object
  \param obj the object to test
  \param flag the or'd list of flags to test
  \return true value if the object has the flags defined
*/
#define switch_test_flag(obj, flag) ((obj)->flags & flag)

/*!
  \brief Set a flag on an arbitrary object
  \param obj the object to set the flags on
  \param flag the or'd list of flags to set
*/
#define switch_set_flag(obj, flag) (obj)->flags |= (flag)

/*!
  \brief Clear a flag on an arbitrary object
  \param obj the object to test
  \param flag the or'd list of flags to clear
*/
#define switch_clear_flag(obj, flag) (obj)->flags &= ~(flag)

/*!
  \brief Copy flags from one arbitrary object to another
  \param dest the object to copy the flags to
  \param src the object to copy the flags from
  \param flags the flags to copy
*/
#define switch_copy_flags(dest, src, flags) (dest)->flags &= ~(flags);	(dest)->flags |= ((src)->flags & (flags))


/*!
  \brief Free a pointer and set it to NULL unles it already is NULL
  \param it the pointer
*/
#define switch_safe_free(it) if (it) {free(it);it=NULL;}

/*!
  \brief Test for NULL or zero length string
  \param s the string to test
  \return true value if the string is NULL or zero length
*/
#define switch_strlen_zero(s) (s && *s != '\0') ? 0 : 1

/*!
  \brief Wait a desired number of microseconds and yield the CPU
*/
#define switch_yield(ms) apr_sleep(ms * 10); apr_thread_yield();

/*!
  \brief Declares a function designed to set a dymaic global string
  \param fname the function name to declare
  \param vname the name of the global pointer to modify with the new function
*/
#define SWITCH_DECLARE_GLOBAL_STRING_FUNC(fname, vname) static void fname(char *string) { if (vname) {free(vname); vname = NULL;}vname = strdup(string);}

/*!
  \brief Separate a string into an array based on a character delimeter
  \param buf the string to parse
  \param delim the character delimeter
  \param array the array to split the values into
  \param arraylen the max number of elements in the array
  \return the number of elements added to the array
*/
SWITCH_DECLARE(unsigned int) switch_separate_string(char *buf, char delim, char **array, int arraylen);

/*!
  \brief Create a set of file descriptors to poll
  \param poll the polfd to create
  \param sock the socket to add
  \param flags the flags to modify the behaviour
  \param pool the memory pool to use
  \return SWITCH_STATUS_SUCCESS when successful
*/
SWITCH_DECLARE(switch_status_t) switch_socket_create_pollfd(switch_pollfd_t *poll, switch_socket_t *sock, switch_int16_t flags, switch_memory_pool_t *pool);

/*!
  \brief Wait for a socket
  \param poll the pollfd to wait on
  \param ms the number of milliseconds to wait
  \return the requested condition
*/
SWITCH_DECLARE(int) switch_socket_waitfor(switch_pollfd_t *poll, int ms);

/*!
  \brief Create a pointer to the file name in a given file path eliminating the directory name
  \return the pointer to the next character after the final / or \\ characters
*/
SWITCH_DECLARE(char *) switch_cut_path(char *in);

SWITCH_DECLARE(char *) switch_string_replace(const char *string, const char *search, const char *replace);
SWITCH_DECLARE(switch_status_t) switch_string_match(const char *string, size_t string_len, const char *search, size_t search_len);

#define SWITCH_READ_ACCEPTABLE(status) status == SWITCH_STATUS_SUCCESS || status == SWITCH_STATUS_BREAK
SWITCH_DECLARE(size_t) switch_url_encode(char *url, char *buf, size_t len);
SWITCH_DECLARE(char *) switch_url_decode(char *s);
END_EXTERN_C

#endif
