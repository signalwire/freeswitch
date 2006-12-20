/* MS C++ Compiler includes */
#ifndef __win_etpan_h
#define __win_etpan_h

#include <direct.h>
#include <process.h>
#include <stdlib.h>
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif

/* for compatibility */
#define S_IRUSR		_S_IREAD
#define S_IWUSR		_S_IWRITE

/* compatibility is allowed with MULTITHREAD DLL */
extern struct tm *gmtime_r (const time_t *timep, struct tm *result);
extern struct tm *localtime_r (const time_t *timep, struct tm *result);

/* stat */
typedef unsigned short mode_t;
#define S_ISREG(x) ((x &  _S_IFMT) == _S_IFREG)
#define S_ISDIR(x) ((x &  _S_IFMT) == _S_IFDIR)

/* same properties ? */
#define random() rand()

/* same function */
#define ftruncate( x, y) chsize( x, y)

/* create and open */
extern int mkstemp (char *tmp_template);

/* why is this missing ? */
#define sleep(x) _sleep(x)

#define link(x, y) (errno = EPERM)

/* ------------------------------------------------- */
/* dirent */

struct dirent
  {
    unsigned short int d_reclen;
    unsigned char d_type;
    char d_name[256];       /* We must not include limits.h! */
  };

/* This is the data type of directory stream objects.
   The actual structure is opaque to users.  */
typedef struct __dirstream DIR;

/* Open a directory stream on NAME.
   Return a DIR stream on the directory, or NULL if it could not be opened.  */
extern DIR *opendir (const char *__name);

/* Close the directory stream DIRP.
   Return 0 if successful, -1 if not.  */
extern int closedir (DIR *__dirp);

/* Read a directory entry from DIRP.  Return a pointer to a `struct
   dirent' describing the entry, or NULL for EOF or error.  The
   storage returned may be overwritten by a later readdir call on the
   same DIR stream.*/
extern struct dirent *readdir (DIR *__dirp);
/* ------------------------------------------------- */

/* ------------------------------------------------- */
/* sys/mman */

/* Protections are chosen from these bits, OR'd together.  The
   implementation does not necessarily support PROT_EXEC or PROT_WRITE
   without PROT_READ.  The only guarantees are that no writing will be
   allowed without PROT_WRITE and no access will be allowed for PROT_NONE. */

#define PROT_READ   0x1     /* Page can be read.  */
#define PROT_WRITE  0x2     /* Page can be written.  */
#define PROT_EXEC   0x4     /* Page can be executed.  */
#define PROT_NONE   0x0     /* Page can not be accessed.  */

/* Sharing types (must choose one and only one of these).  */
#define MAP_SHARED  0x01        /* Share changes.  */
#define MAP_PRIVATE 0x02        /* Changes are private.  */


/* Return value of `mmap' in case of an error.  */
#define MAP_FAILED  ((void *) -1)

/* Map addresses starting near ADDR and extending for LEN bytes.  from
   OFFSET into the file FD describes according to PROT and FLAGS.  If ADDR
   is nonzero, it is the desired mapping address.  If the MAP_FIXED bit is
   set in FLAGS, the mapping will be at ADDR exactly (which must be
   page-aligned); otherwise the system chooses a convenient nearby address.
   The return value is the actual mapping address chosen or MAP_FAILED
   for errors (in which case `errno' is set).  A successful `mmap' call
   deallocates any previous mapping for the affected region.  */
extern void *mmap (void *__addr, size_t __len, int __prot,
           int __flags, int __fd, size_t __offset);

/* Deallocate any mapping for the region starting at ADDR and extending LEN
   bytes.  Returns 0 if successful, -1 for errors (and sets errno).  */
extern int munmap (void *__addr, size_t __len);

/* Synchronize the region starting at ADDR and extending LEN bytes with the
   file it maps.  Filesystem operations on a file being mapped are
   unpredictable before this is done.  Flags are from the MS_* set.  */
extern int msync (void *__addr, size_t __len, int __flags);

/* Flags to `msync'.  */
/* Sync memory asynchronously.  */
#if 0 /* unsupported */
#define MS_ASYNC    1
#endif
#define MS_SYNC     4       /* Synchronous memory sync.  */
/* Invalidate the caches.  */
#if 0 /* unsupported */
#define MS_INVALIDATE   2
#endif

/* ------------------------------------------------- */

#ifdef __cplusplus
}
#endif
#endif /* __win_etpan_h */
