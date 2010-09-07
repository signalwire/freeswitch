/*
 * Copyright (c) 2010, Sangoma Technologies
 * David Yat Sin <dyatsin@sangoma.com>
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * * Neither the name of the original author; nor the names of any contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 * 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "private/ftdm_core.h"
#include <ctype.h>


FT_DECLARE(ftdm_status_t) ftdm_span_set_npi(const char *npi_string, uint8_t *target)
{
	if (!strcasecmp(npi_string, "isdn") || !strcasecmp(npi_string, "e164")) {
		*target = FTDM_NPI_ISDN;
	} else if (!strcasecmp(npi_string, "data")) {
		*target = FTDM_NPI_DATA;
	} else if (!strcasecmp(npi_string, "telex")) {
		*target = FTDM_NPI_TELEX;
	} else if (!strcasecmp(npi_string, "national")) {
		*target = FTDM_NPI_NATIONAL;
	} else if (!strcasecmp(npi_string, "private")) {
		*target = FTDM_NPI_PRIVATE;
	} else if (!strcasecmp(npi_string, "reserved")) {
		*target = FTDM_NPI_RESERVED;
	} else if (!strcasecmp(npi_string, "unknown")) {
		*target = FTDM_NPI_UNKNOWN;
	} else {
		ftdm_log(FTDM_LOG_WARNING, "Invalid NPI value (%s)\n", npi_string);
		*target = FTDM_NPI_UNKNOWN;
		return FTDM_FAIL;
	}
	return FTDM_SUCCESS;
}

FT_DECLARE(ftdm_status_t) ftdm_span_set_ton(const char *ton_string, uint8_t *target)
{
	if (!strcasecmp(ton_string, "national")) {
		*target = FTDM_TON_NATIONAL;
	} else if (!strcasecmp(ton_string, "international")) {
		*target = FTDM_TON_INTERNATIONAL;
	} else if (!strcasecmp(ton_string, "local")) {
		*target = FTDM_TON_SUBSCRIBER_NUMBER;
	} else if (!strcasecmp(ton_string, "unknown")) {
		*target = FTDM_TON_UNKNOWN;
	} else {
		ftdm_log(FTDM_LOG_WARNING, "Invalid TON value (%s)\n", ton_string);
		*target = FTDM_TON_UNKNOWN;
		return FTDM_FAIL;
	}
	return FTDM_SUCCESS;
}

FT_DECLARE(ftdm_status_t) ftdm_span_set_bearer_capability(const char *bc_string, ftdm_bearer_cap_t *target)
{
	if (!strcasecmp(bc_string, "speech")) {
		*target = FTDM_BEARER_CAP_SPEECH;
	} else if (!strcasecmp(bc_string, "unrestricted-digital")) {
		*target = FTDM_BEARER_CAP_64K_UNRESTRICTED;
	} else if (!strcasecmp(bc_string, "3.1Khz")) {
		*target = FTDM_BEARER_CAP_3_1KHZ_AUDIO;
	} else {
		ftdm_log(FTDM_LOG_WARNING, "Unsupported Bearer Capability value (%s)\n", bc_string);
		return FTDM_FAIL;
	}
	return FTDM_SUCCESS;
}

FT_DECLARE(ftdm_status_t) ftdm_span_set_bearer_layer1(const char *bc_string, ftdm_user_layer1_prot_t *target)
{
	if (!strcasecmp(bc_string, "v110")) {
		*target = FTDM_USER_LAYER1_PROT_V110;
	} else if (!strcasecmp(bc_string, "ulaw")) {
		*target = FTDM_USER_LAYER1_PROT_ULAW;
	} else if (!strcasecmp(bc_string, "alaw")) {
		*target =FTDM_USER_LAYER1_PROT_ALAW ;
	} else {
		ftdm_log(FTDM_LOG_WARNING, "Unsupported Bearer Layer1 Prot value (%s)\n", bc_string);
		return FTDM_FAIL;
	}
	return FTDM_SUCCESS;
}


FT_DECLARE(ftdm_status_t) ftdm_is_number(char *number)
{
	if (!number) {
		return FTDM_FAIL;
	}

	for ( ; *number; number++) {
		if (!isdigit(*number)) {
			return FTDM_FAIL;
		}
	}
	return FTDM_SUCCESS;
}

