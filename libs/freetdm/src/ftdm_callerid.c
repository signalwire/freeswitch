#include "freetdm.h"
#include "fsk.h"
#include "uart.h"



static void fsk_byte_handler (void *x, int data)
{
	ftdm_fsk_data_state_t *state = (ftdm_fsk_data_state_t *) x;
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

FT_DECLARE(ftdm_status_t) ftdm_fsk_data_init(ftdm_fsk_data_state_t *state, uint8_t *data, uint32_t datalen)
{
	memset(state, 0, sizeof(*state));
	state->buf = data;
	state->bufsize = datalen;
	state->bpos = 2;

	return FTDM_SUCCESS;
}

FT_DECLARE(ftdm_status_t) ftdm_fsk_data_add_sdmf(ftdm_fsk_data_state_t *state, const char *date, char *number)
{
	size_t dlen = strlen(date);
	size_t nlen = strlen(number);

	state->buf[0] = FTDM_CID_TYPE_SDMF;
	memcpy(&state->buf[state->bpos], date, dlen);
	state->bpos += dlen;
	memcpy(&state->buf[state->bpos], number, nlen);
	state->bpos += nlen;

	return FTDM_SUCCESS;
}

FT_DECLARE(ftdm_status_t) ftdm_fsk_data_add_mdmf(ftdm_fsk_data_state_t *state, ftdm_mdmf_type_t type, const uint8_t *data, uint32_t datalen)
{
	state->buf[0] = FTDM_CID_TYPE_MDMF;
	state->buf[state->bpos++] = type;
	state->buf[state->bpos++] = (uint8_t)datalen;
	memcpy(&state->buf[state->bpos], data, datalen);
	state->bpos += datalen;
	return FTDM_SUCCESS;
}


FT_DECLARE(ftdm_status_t) ftdm_fsk_data_add_checksum(ftdm_fsk_data_state_t *state)
{
	uint32_t i;
	uint8_t check = 0;

	state->buf[1] = (uint8_t)(state->bpos - 2);

	for (i = 0; i < state->bpos; i++) {
		check = check + state->buf[i];
	}

	state->checksum = state->buf[state->bpos] = (uint8_t)(256 - check);
	state->bpos++;

	state->dlen = state->bpos;
	state->blen = state->buf[1];

	return FTDM_SUCCESS;
}


FT_DECLARE(ftdm_status_t) ftdm_fsk_data_parse(ftdm_fsk_data_state_t *state, ftdm_size_t *type, char **data, ftdm_size_t *len)
{

	ftdm_size_t i;
	int sum = 0;
	
 top:

	if (state->checksum != 0 || state->ppos >= state->dlen - 1) {
		return FTDM_FAIL;
	}

	if (!state->ppos) {
		for(i = 0; i < state->bpos; i++) {
			sum += state->buf[i];
		}
		state->checksum = sum % 256;
		state->ppos = 2;		

		if (state->buf[0] != FTDM_CID_TYPE_MDMF && state->buf[0] != FTDM_CID_TYPE_SDMF) {
			state->checksum = -1;
		}
		goto top;
	}

	if (state->buf[0] == FTDM_CID_TYPE_SDMF) {
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
		return FTDM_SUCCESS;
	} else if (state->buf[0] == FTDM_CID_TYPE_MDMF) {
		*type = state->buf[state->ppos++];
		*len = state->buf[state->ppos++];
		*data = (char *)&state->buf[state->ppos];
		state->ppos += *len;
		return FTDM_SUCCESS;
	}

	return FTDM_FAIL;
}

FT_DECLARE(ftdm_status_t) ftdm_fsk_demod_feed(ftdm_fsk_data_state_t *state, int16_t *data, ftdm_size_t samples)
{
	uint32_t x;
	int16_t *sp = data;

	if (state->init == 3) {
		return FTDM_FAIL;
	}

	for (x = 0; x < samples; x++) {
		dsp_fsk_sample (state->fsk1200_handle, (double) *sp++ / 32767.0);
		if (state->dlen && state->bpos >= state->dlen) {
			state->init = 3;
			return FTDM_FAIL;
		}
	}

	return FTDM_SUCCESS;
}

FT_DECLARE(ftdm_status_t) ftdm_fsk_demod_destroy(ftdm_fsk_data_state_t *state)
{
	dsp_fsk_destroy(&state->fsk1200_handle);
	memset(state, 0, sizeof(*state));
	return FTDM_SUCCESS;
}

FT_DECLARE(int) ftdm_fsk_demod_init(ftdm_fsk_data_state_t *state, int rate, uint8_t *buf, ftdm_size_t bufsize)
{

	dsp_fsk_attr_t fsk1200_attr;

	if (state->fsk1200_handle) {
		dsp_fsk_destroy(&state->fsk1200_handle);
	}

	memset(state, 0, sizeof(*state));
	memset(buf, 0, bufsize);
	state->buf = buf;
	state->bufsize = bufsize;
	
	dsp_fsk_attr_init (&fsk1200_attr);
	dsp_fsk_attr_set_samplerate (&fsk1200_attr, rate);
	dsp_fsk_attr_set_bytehandler (&fsk1200_attr, fsk_byte_handler, state);
	state->fsk1200_handle = dsp_fsk_create (&fsk1200_attr);

	if (state->fsk1200_handle == NULL) {
		return FTDM_FAIL;
	}

	return FTDM_SUCCESS;
}

FT_DECLARE(ftdm_size_t) ftdm_fsk_modulator_generate_bit(ftdm_fsk_modulator_t *fsk_trans, int8_t bit, int16_t *buf, ftdm_size_t buflen)
{
	ftdm_size_t i;
		
	for(i = 0 ; i < buflen; i++) {
		fsk_trans->bit_accum += fsk_trans->bit_factor;
		if (fsk_trans->bit_accum >= FTDM_FSK_MOD_FACTOR) {
			fsk_trans->bit_accum -= (FTDM_FSK_MOD_FACTOR + fsk_trans->bit_factor);
			break;
		}

		buf[i] = teletone_dds_state_modulate_sample(&fsk_trans->dds, bit);
	}

	return i;
}


FT_DECLARE(int32_t) ftdm_fsk_modulator_generate_carrier_bits(ftdm_fsk_modulator_t *fsk_trans, uint32_t bits)
{
	uint32_t i = 0;
	ftdm_size_t r = 0;
	int8_t bit = 1;

	for (i = 0; i < bits; i++) {
		if ((r = ftdm_fsk_modulator_generate_bit(fsk_trans, bit, fsk_trans->sample_buffer, sizeof(fsk_trans->sample_buffer) / 2))) {
			if (fsk_trans->write_sample_callback(fsk_trans->sample_buffer, r, fsk_trans->user_data) != FTDM_SUCCESS) {
				break;
			}
		} else {
			break;
		}
	}

	return i;
}


FT_DECLARE(void) ftdm_fsk_modulator_generate_chan_sieze(ftdm_fsk_modulator_t *fsk_trans)
{
	uint32_t i = 0;
	ftdm_size_t r = 0;
	int8_t bit = 0;
	
	for (i = 0; i < fsk_trans->chan_sieze_bits; i++) {
		if ((r = ftdm_fsk_modulator_generate_bit(fsk_trans, bit, fsk_trans->sample_buffer, sizeof(fsk_trans->sample_buffer) / 2))) {
			if (fsk_trans->write_sample_callback(fsk_trans->sample_buffer, r, fsk_trans->user_data) != FTDM_SUCCESS) {
				break;
			}
		} else {
			break;
		}
		bit = !bit;
	}
	

}


FT_DECLARE(void) ftdm_fsk_modulator_send_data(ftdm_fsk_modulator_t *fsk_trans)
{
	ftdm_size_t r = 0;
	int8_t bit = 0;

	while((bit = ftdm_bitstream_get_bit(&fsk_trans->bs)) > -1) {
		if ((r = ftdm_fsk_modulator_generate_bit(fsk_trans, bit, fsk_trans->sample_buffer, sizeof(fsk_trans->sample_buffer) / 2))) {
			if (fsk_trans->write_sample_callback(fsk_trans->sample_buffer, r, fsk_trans->user_data) != FTDM_SUCCESS) {
				break;
			}
		} else {
			break;
		}
	}
}


FT_DECLARE(ftdm_status_t) ftdm_fsk_modulator_init(ftdm_fsk_modulator_t *fsk_trans,
									fsk_modem_types_t modem_type,
									uint32_t sample_rate,
									ftdm_fsk_data_state_t *fsk_data,
									float db_level,
									uint32_t carrier_bits_start,
									uint32_t carrier_bits_stop,
									uint32_t chan_sieze_bits,
									ftdm_fsk_write_sample_t write_sample_callback,
									void *user_data)
{
	memset(fsk_trans, 0, sizeof(*fsk_trans));
	fsk_trans->modem_type = modem_type;
	teletone_dds_state_set_tone(&fsk_trans->dds, fsk_modem_definitions[fsk_trans->modem_type].freq_space, sample_rate, 0);
	teletone_dds_state_set_tone(&fsk_trans->dds, fsk_modem_definitions[fsk_trans->modem_type].freq_mark, sample_rate, 1);
	fsk_trans->bit_factor = (uint32_t)((fsk_modem_definitions[fsk_trans->modem_type].baud_rate * FTDM_FSK_MOD_FACTOR) / (float)sample_rate);
	fsk_trans->samples_per_bit = (uint32_t) (sample_rate / fsk_modem_definitions[fsk_trans->modem_type].baud_rate);
	fsk_trans->est_bytes = (int32_t)(((fsk_data->dlen * 10) + carrier_bits_start + carrier_bits_stop + chan_sieze_bits) * ((fsk_trans->samples_per_bit + 1) * 2));
	fsk_trans->bit_accum = 0;
	fsk_trans->fsk_data = fsk_data;
	teletone_dds_state_set_tx_level(&fsk_trans->dds, db_level);
	ftdm_bitstream_init(&fsk_trans->bs, fsk_trans->fsk_data->buf, (uint32_t)fsk_trans->fsk_data->dlen, FTDM_ENDIAN_BIG, 1);
	fsk_trans->carrier_bits_start = carrier_bits_start;
	fsk_trans->carrier_bits_stop = carrier_bits_stop;
	fsk_trans->chan_sieze_bits = chan_sieze_bits;
	fsk_trans->write_sample_callback = write_sample_callback;
	fsk_trans->user_data = user_data;
	return FTDM_SUCCESS;
}

