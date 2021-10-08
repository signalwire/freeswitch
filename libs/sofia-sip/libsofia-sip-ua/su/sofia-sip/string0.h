#ifndef STRING0_H
/** Defined when <sofia-sip/string0.h> is included. */
#define STRING0_H

/**@file sofia-sip/string0.h
 *
 * @brief Extra string functions.
 *
 * String comparison functions accepting NULL pointers: str0cmp(),
 * str0ncmp(), str0casecmp(), str0ncasecmp(). Also includes span functions
 * testing at most @a n bytes: strncspn(), strnspn().
 *
 * @deprecated Use functions from <sofia-sip/su_string.h> instead.
 */

#include <sofia-sip/su_string.h>

su_inline int str0cmp(char const *a, char const *b)
{
  return su_strcmp(a, b);
}

su_inline int str0ncmp(char const *a, char const *b, size_t n)
{
  return su_strncmp(a, b, n);
}

su_inline int str0casecmp(char const *a, char const *b)
{
  return su_strcasecmp(a, b);
}

su_inline int str0ncasecmp(char const *a, char const *b, size_t n)
{
  return su_strncasecmp(a, b, n);
}

su_inline size_t strnspn(char const *s, size_t ssize, char const *term)
{
  return su_strnspn(s, ssize, term);
}

su_inline size_t strncspn(char const *s, size_t ssize, char const *term)
{
  return su_strncspn(s, ssize, term);
}
#endif
