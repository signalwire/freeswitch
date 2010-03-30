#include "freetdm.h"
ftdm_status_t my_write_sample(int16_t *buf, ftdm_size_t buflen, void *user_data);

struct helper {
	int fd;
	int wrote;
};

ftdm_status_t my_write_sample(int16_t *buf, ftdm_size_t buflen, void *user_data)
{
	struct helper *foo = (struct helper *) user_data;
        size_t len;
	len = write(foo->fd, buf, buflen * 2);
        if (!len) return FTDM_FAIL;
	foo->wrote += buflen * 2;
	return FTDM_SUCCESS;
}

int main(int argc, char *argv[])
{
	struct ftdm_fsk_modulator fsk_trans;
	ftdm_fsk_data_state_t fsk_data = {0};
	int fd = -1;
	int16_t buf[160] = {0};
	ssize_t len = 0;
	size_t type, mlen;
	char *sp;
	char str[128] = "";
	char fbuf[256];
	uint8_t databuf[1024] = "";
	struct helper foo = {0};
	//	int x, bytes, start_bits = 180, stop_bits = 5, sbits = 300;
	char time_str[9];
	struct tm tm;
	time_t now;
	
	if (argc < 2) {
		int x;
		const char *url = "sip:cool@rad.com";

		if ((fd = open("tone.raw", O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR)) < 0) {
			fprintf(stderr, "File Error! [%s]\n", strerror(errno));
			exit(-1);
		}


		time(&now);
		localtime_r(&now, &tm);
		strftime(time_str, sizeof(time_str), "%m%d%H%M", &tm);

		ftdm_fsk_data_init(&fsk_data, databuf, sizeof(databuf));
#if 1
		
		ftdm_fsk_data_add_mdmf(&fsk_data, MDMF_DATETIME, (uint8_t *)time_str, strlen(time_str));
		//ftdm_fsk_data_add_mdmf(&fsk_data, MDMF_DATETIME, "06091213", 8);
		ftdm_fsk_data_add_mdmf(&fsk_data, MDMF_PHONE_NUM, (uint8_t *)"14149361212", 7);
		ftdm_fsk_data_add_mdmf(&fsk_data, MDMF_PHONE_NAME, (uint8_t *)"Fred Smith", 10);
		for(x = 0; x < 0; x++)
			ftdm_fsk_data_add_mdmf(&fsk_data, MDMF_ALT_ROUTE, (uint8_t *)url, strlen(url));
#else
		ftdm_fsk_data_add_sdmf(&fsk_data, "06061234", "0");
		//ftdm_fsk_data_add_sdmf(&state, "06061234", "5551212");
#endif
		ftdm_fsk_data_add_checksum(&fsk_data);

		foo.fd = fd;


		ftdm_fsk_modulator_init(&fsk_trans, FSK_BELL202, 8000, &fsk_data, -14, 180, 5, 300, my_write_sample, &foo);
		ftdm_fsk_modulator_send_all((&fsk_trans));

		printf("%u %d %d\n", (unsigned) fsk_data.dlen, foo.wrote, fsk_trans.est_bytes);

		if (fd > -1) {
			close (fd);
		}

		return 0;
	}

	if (ftdm_fsk_demod_init(&fsk_data, 8000, (uint8_t *)fbuf, sizeof(fbuf))) {
		printf("wtf\n");
		return 0;
	}

	if ((fd = open(argv[1], O_RDONLY)) < 0) {
		fprintf(stderr, "cant open file %s\n", argv[1]);
		exit (-1);
	}

	while((len = read(fd, buf, sizeof(buf))) > 0) {
		if (ftdm_fsk_demod_feed(&fsk_data, buf, len / 2) != FTDM_SUCCESS) {
			break;
		}
	}

	while(ftdm_fsk_data_parse(&fsk_data, &type, &sp, &mlen) == FTDM_SUCCESS) {
		ftdm_copy_string(str, sp, mlen+1);
		*(str+mlen) = '\0';
		ftdm_clean_string(str);
		printf("TYPE %u (%s) LEN %u VAL [%s]\n", (unsigned)type, ftdm_mdmf_type2str(type), (unsigned)mlen, str);
	}

	ftdm_fsk_demod_destroy(&fsk_data);

	close(fd);
	return 0;
}
