//#include "freetdm.h"
#include "libteletone_detect.h"

int main(int argc, char *argv[])
{
	int fd, b;
	short sln[512] = {0};
	teletone_dtmf_detect_state_t dtmf_detect = {0};
	char digit_str[128] = "";

	if (argc < 2) {
		fprintf(stderr, "Arg Error!\n");
		exit(-1);
	}

	teletone_dtmf_detect_init (&dtmf_detect, 8000);

	if ((fd = open(argv[1], O_RDONLY)) < 0) {
		fprintf(stderr, "File Error! [%s]\n", strerror(errno));
		exit(-1);
	}

	while((b = read(fd, sln, 320)) > 0) {
		teletone_dtmf_detect(&dtmf_detect, sln, b / 2);
		teletone_dtmf_get(&dtmf_detect, digit_str, sizeof(digit_str));
		if (*digit_str) {
			printf("digit: %s\n", digit_str);
		}
	}
	close(fd);
	return 0;
}

