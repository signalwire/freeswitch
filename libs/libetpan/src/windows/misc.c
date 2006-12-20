#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include <sys/types.h>
#include <sys/stat.h>

int mkstemp (char *tmp_template) {
	int fd;
	char * res;

#ifdef _DEBUG
	printf("mkstemp:%s\n", tmp_template);
#endif
	fd = -1;
	res = mktemp( tmp_template);
	if (res && *res) {
		fd = open( res, _O_BINARY | _O_CREAT | _O_TEMPORARY | _O_EXCL, _S_IREAD | _S_IWRITE);
	}

	return fd;
}