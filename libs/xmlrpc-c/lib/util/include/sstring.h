#ifndef SSTRING_H_INCLUDED
#define SSTRING_H_INCLUDED

/* This file contains string functions that are cognizant of the
   declared size of the destination data structure.
*/


/* Copy string pointed by B to array A with size checking.  */
#define SSTRCPY(A,B) \
	(strncpy((A), (B), sizeof(A)), *((A)+sizeof(A)-1) = '\0')
#define SSTRCMP(A,B) \
	(strncmp((A), (B), sizeof(A)))

#endif
