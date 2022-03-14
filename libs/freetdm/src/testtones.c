#include "private/ftdm_core.h"

struct ttmp {
	int fd;
};

static int teletone_handler(teletone_generation_session_t *ts, teletone_tone_map_t *map)
{
	struct ttmp *tmp = ts->user_data;
	int wrote;
	size_t len;

	wrote = teletone_mux_tones(ts, map);
	len = write(tmp->fd, ts->buffer, wrote * 2);
	
	if (!len) return -1;

	return 0;
}

#if 1
int main(int argc, char *argv[])
{
	teletone_generation_session_t ts;
	struct ttmp tmp;

	if (argc < 3) {
		fprintf(stderr, "Arg Error! <file> <tones>\n");
		exit(-1);
	}

	if ((tmp.fd = open(argv[1], O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR)) < 0) {
		fprintf(stderr, "File Error! [%s]\n", strerror(errno));
		exit(-1);
	}

	teletone_init_session(&ts, 0, teletone_handler, &tmp);
	ts.rate = 8000;
	ts.debug = 1;
	ts.debug_stream = stdout;
	teletone_run(&ts, argv[2]);
	close(tmp.fd);

	return 0;
}
#else 
int32_t main(int argc, char *argv[])
{
	int32_t j, i, fd = -1;
	int32_t rate = 8000;
	/* SIT tones and durations */
	float tones[] = { 913.8, 1370.6, 1776.7, 0 };
	int32_t durations[] = {274, 274, 380, 0};
	teletone_dds_state_t dds = {0};
	int16_t sample;
	size_t len = 1;

	if (argc < 2 || (fd = open(argv[1], O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR)) < 0) {
		fprintf(stderr, "File Error!\n", strerror(errno));
		exit(-1);
	}

	for (j = 0; tones[j] && durations[j]; j++) {

		teletone_dds_state_set_tone(&dds, tones[j], rate, -50);
		
		for(i = 0; (i < durations[j] * rate / 1000) && len != 0; i++) {
			sample = teletone_dds_modulate_sample(&dds) * 20;
			len = write(fd, &sample, sizeof(sample));
		}

	}
	
	close(fd);
}
#endif
