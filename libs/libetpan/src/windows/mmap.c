#include "win_etpan.h"


void *mmap (void *__addr, size_t __len, int __prot,
				   int __flags, int __fd, size_t __offset) {
#ifdef _DEBUG
	fprintf( stderr, "mmap inimplemented\n");
#endif
   return MAP_FAILED;
}

int munmap (void *__addr, size_t __len) {
#ifdef _DEBUG
	fprintf( stderr, "mmap inimplemented\n");
#endif
	return -1;
}

int msync (void *__addr, size_t __len, int __flags) {
#ifdef _DEBUG
	fprintf( stderr, "mmap inimplemented\n");
#endif
	return -1;
}
