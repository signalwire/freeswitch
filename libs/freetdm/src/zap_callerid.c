#include "openzap.h"
#include "fsk.h"
#include "uart.h"


static void fsk_byte_handler (void *x, int data)
{
	zap_fsk_data_state_t *state = (zap_fsk_data_state_t *) x;
	uint8_t byte = (uint8_t)data;

 top:

	if (state->init == 3) {
		return;
	}

	if (state->dlen) {
		goto add_byte;
	}

	if (state->bpos == 1) {
		state->blen = byte;

		if ((uint32_t)(state->dlen = state->bpos + byte + 2) > state->bufsize) {
			state->dlen = state->bufsize;
		}
		goto top;
	}

 add_byte:

	if (state->bpos <= state->dlen) {
		state->buf[state->bpos++] = byte;
	} else {
		state->init = 3;
	}
}

zap_status_t zap_fsk_data_parse(zap_fsk_data_state_t *state, zap_size_t *type, char **data, zap_size_t *len)
{

	zap_size_t i;
	int sum = 0;
	
 top:

	if (state->checksum != 0 || state->ppos >= state->dlen - 1) {
		return ZAP_FAIL;
	}

	if (!state->ppos) {
		for(i = 0; i < state->bpos; i++) {
			sum += state->buf[i];
		}
		state->checksum = sum % 256;
		state->ppos = 2;		
		if (state->buf[0] != ZAP_CID_TYPE_MDMF && state->buf[0] != ZAP_CID_TYPE_SDMF) {
			state->checksum = -1;
		}
		goto top;
	}

	if (state->buf[0] == ZAP_CID_TYPE_SDMF) {
		/* convert sdmf to mdmf so we don't need 2 parsers */
		if (state->ppos == 2) {
			*type = MDMF_DATETIME;
			*len = 8;
		} else {
			if (state->buf[state->ppos] == 'P' || state->buf[state->ppos] == 'O') {
				*type = MDMF_NO_NUM;
				*len = 1;
			} else {
				*type = MDMF_PHONE_NUM;
				*len = state->blen - 8;
			}
		}
		*data = (char *)&state->buf[state->ppos];
		state->ppos += *len;		
		return ZAP_SUCCESS;
	} else if (state->buf[0] == ZAP_CID_TYPE_MDMF) {
		*type = state->buf[state->ppos++];
		*len = state->buf[state->ppos++];
		*data = (char *)&state->buf[state->ppos];
		state->ppos += *len;
		return ZAP_SUCCESS;
	}

	return ZAP_FAIL;
}

zap_status_t zap_fsk_demod_feed(zap_fsk_data_state_t *state, int16_t *data, zap_size_t samples)
{
	uint32_t x;
	int16_t *sp = data;

	if (state->init == 3) {
		return ZAP_FAIL;
	}

	for (x = 0; x < samples; x++) {
		dsp_fsk_sample (state->fsk1200_handle, (double) *sp++ / 32767.0);
		if (state->dlen && state->bpos >= state->dlen) {
			state->init = 3;
			return ZAP_FAIL;
		}
	}

	return ZAP_SUCCESS;
}

zap_status_t zap_fsk_demod_destroy(zap_fsk_data_state_t *state)
{
	dsp_fsk_destroy(&state->fsk1200_handle);
	memset(state, 0, sizeof(*state));
	return ZAP_SUCCESS;
}

int zap_fsk_demod_init(zap_fsk_data_state_t *state, int rate, uint8_t *buf, zap_size_t bufsize)
{

	dsp_fsk_attr_t fsk1200_attr;

	if (state->fsk1200_handle) {
		dsp_fsk_destroy(&state->fsk1200_handle);
	}

	state->buf = buf;
	state->bufsize = bufsize;
	
	dsp_fsk_attr_init (&fsk1200_attr);
	dsp_fsk_attr_set_samplerate (&fsk1200_attr, rate);
	dsp_fsk_attr_set_bytehandler (&fsk1200_attr, fsk_byte_handler, state);
	state->fsk1200_handle = dsp_fsk_create (&fsk1200_attr);

	if (state->fsk1200_handle == NULL) {
		return ZAP_FAIL;
	}

	return ZAP_SUCCESS;
}


