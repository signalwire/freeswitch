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
 *
 * Contributors: 
 *
 * Moises Silva <moy@sangoma.com>
 * Ricardo Barroetaveña <rbarroetavena@anura.com.ar>
 *
 */

#include "private/ftdm_core.h"
#include <ctype.h>


FT_DECLARE(ftdm_status_t) ftdm_set_npi(const char *string, uint8_t *target)
{
	uint8_t val;
	ftdm_status_t status = FTDM_SUCCESS;

	val = ftdm_str2ftdm_npi(string);
	if (val == FTDM_NPI_INVALID) {
		ftdm_log(FTDM_LOG_WARNING, "Invalid NPI string (%s)\n", string);
		status = FTDM_FAIL;
		val = FTDM_NPI_UNKNOWN;
	}
	*target = val;
	return status;
}

FT_DECLARE(ftdm_status_t) ftdm_set_ton(const char *string, uint8_t *target)
{
	uint8_t val;
	ftdm_status_t status = FTDM_SUCCESS;

	val = ftdm_str2ftdm_ton(string);
	if (val == FTDM_TON_INVALID) {
		ftdm_log(FTDM_LOG_WARNING, "Invalid TON string (%s)\n", string);
		status = FTDM_FAIL;
		val = FTDM_TON_UNKNOWN;
	}
	*target = val;
	return status;
}

FT_DECLARE(ftdm_status_t) ftdm_set_bearer_capability(const char *string, uint8_t *target)
{
	uint8_t val;
	ftdm_status_t status = FTDM_SUCCESS;

	val = ftdm_str2ftdm_bearer_cap(string);
	if (val == FTDM_NPI_INVALID) {
		ftdm_log(FTDM_LOG_WARNING, "Invalid Bearer-Capability string (%s)\n", string);
		status = FTDM_FAIL;
		val = FTDM_BEARER_CAP_SPEECH;
	}

	*target = val;
	return status;
}

FT_DECLARE(ftdm_status_t) ftdm_set_bearer_layer1(const char *string, uint8_t *target)
{
	uint8_t val;
	ftdm_status_t status = FTDM_SUCCESS;

	val = ftdm_str2ftdm_usr_layer1_prot(string);
	if (val == FTDM_USER_LAYER1_PROT_INVALID) {
		ftdm_log(FTDM_LOG_WARNING, "Invalid Bearer Layer 1 Protocol string (%s)\n", string);
		status = FTDM_FAIL;
		val = FTDM_USER_LAYER1_PROT_ULAW;
	}

	*target = val;
	return status;
}

FT_DECLARE(ftdm_status_t) ftdm_set_screening_ind(const char *string, uint8_t *target)
{
	uint8_t val;
	ftdm_status_t status = FTDM_SUCCESS;

	val = ftdm_str2ftdm_screening(string);
	if (val == FTDM_SCREENING_INVALID) {
		ftdm_log(FTDM_LOG_WARNING, "Invalid screening indicator string (%s)\n", string);
		status = FTDM_FAIL;
		val = FTDM_SCREENING_NOT_SCREENED;
	}

	*target = val;
	return status;
}

FT_DECLARE(ftdm_status_t) ftdm_set_presentation_ind(const char *string, uint8_t *target)
{
	uint8_t val;
	ftdm_status_t status = FTDM_SUCCESS;

	val = ftdm_str2ftdm_presentation(string);
	if (val == FTDM_PRES_INVALID) {
		ftdm_log(FTDM_LOG_WARNING, "Invalid presentation string (%s)\n", string);
		status = FTDM_FAIL;
		val = FTDM_PRES_ALLOWED;
	}

	*target = val;
	return status;
}

FT_DECLARE(ftdm_status_t) ftdm_is_number(const char *number)
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


FT_DECLARE(ftdm_status_t) ftdm_set_calling_party_category(const char *string, uint8_t *target)
{
	uint8_t val;
	ftdm_status_t status = FTDM_SUCCESS;

	val = ftdm_str2ftdm_calling_party_category(string);
	if (val == FTDM_CPC_INVALID) {
		ftdm_log(FTDM_LOG_WARNING, "Invalid category string (%s)\n", string);
		val = FTDM_CPC_ORDINARY;
		status = FTDM_FAIL;
	}

	*target = val;
	return status;
}

