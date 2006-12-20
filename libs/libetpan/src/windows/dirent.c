#include <win_etpan.h>

DIR *opendir (const char *__name) {
#ifdef _DEBUG
	fprintf( stderr, "opendir inimplemented\n");
#endif
	return NULL;
}

int closedir (DIR *__dirp) {
#ifdef _DEBUG
	fprintf( stderr, "closedir inimplemented\n");
#endif
	return 0;
}

struct dirent *readdir (DIR *__dirp) {
#ifdef _DEBUG
	fprintf( stderr, "readdir inimplemented\n");
#endif
	return NULL;
}
