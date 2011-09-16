//#include "freetdm.h"
#include "libteletone_detect.h"

int main(int argc, char *argv[])
{
	int fd, b;
	short sln[512] = {0};
	teletone_dtmf_detect_state_t dtmf_detect = {0};
	teletone_hit_type_t hit;

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
		char digit_char;
		unsigned int dur;

		teletone_dtmf_detect(&dtmf_detect, sln, b / 2);
		if ((hit = teletone_dtmf_get(&dtmf_detect, &digit_char, &dur))) {
			const char *hs = NULL;
			

			switch(hit) {
			case TT_HIT_BEGIN:
				hs = "begin";
				break;

			case TT_HIT_MIDDLE:
				hs = "middle";
				break;

			case TT_HIT_END:
				hs = "end";
				break;
			default:
				break;
			}

			printf("%s digit: %c\n", hs, digit_char);
		}
	}
	close(fd);
	return 0;
}

