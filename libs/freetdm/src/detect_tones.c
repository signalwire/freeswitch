//#include "openzap.h"
#include "libteletone_detect.h"

int main(int argc, char *argv[])
{
	teletone_multi_tone_t mt = {0};
	teletone_tone_map_t map = {{0}};

	int fd, b;
	short sln[512] = {0};

	if (argc < 2) {
		fprintf(stderr, "Arg Error!\n");
		exit(-1);
	}

	map.freqs[0] = atof("350");
	map.freqs[1] = atof("440");
	teletone_multi_tone_init(&mt, &map);

	if ((fd = open(argv[1], O_RDONLY)) < 0) {
		fprintf(stderr, "File Error! [%s]\n", strerror(errno));
		exit(-1);
	}

	while((b = read(fd, sln, 320)) > 0) {
		printf("TEST %d %d\n", b, teletone_multi_tone_detect(&mt, sln, b / 2));
	}
	close(fd);
	return 0;
}

