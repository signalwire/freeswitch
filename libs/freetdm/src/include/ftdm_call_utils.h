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

#ifndef __FTDM_CALL_UTILS_H__
#define __FTDM_CALL_UTILS_H__

/*! 
 * \brief Set the Numbering Plan Identification from a string
 *
 * \param npi_string string value
 * \param target the target to set value to
 *
 * \retval FTDM_SUCCESS success
 * \retval FTDM_FAIL failure
 */
FT_DECLARE(ftdm_status_t) ftdm_set_npi(const char *npi_string, uint8_t *target);


/*! 
 * \brief Set the Type of number from a string
 *
 * \param ton_string string value
 * \param target the target to set value to
 *
 * \retval FTDM_SUCCESS success
 * \retval FTDM_FAIL failure
 */
FT_DECLARE(ftdm_status_t) ftdm_set_ton(const char *ton_string, uint8_t *target);

/*! 
 * \brief Set the Bearer Capability from a string
 *
 * \param bc_string string value
 * \param target the target to set value to
 *
 * \retval FTDM_SUCCESS success
 * \retval FTDM_FAIL failure
 */
FT_DECLARE(ftdm_status_t) ftdm_set_bearer_capability(const char *bc_string, uint8_t *target);

/*! 
 * \brief Set the Bearer Capability - Layer 1 from a string
 *
 * \param bc_string string value
 * \param target the target to set value to
 *
 * \retval FTDM_SUCCESS success
 * \retval FTDM_FAIL failure
 */
FT_DECLARE(ftdm_status_t) ftdm_set_bearer_layer1(const char *bc_string, uint8_t *target);

/*! 
 * \brief Set the Screening Ind from a string
 *
 * \param screen_string string value
 * \param target the target to set value to
 *
 * \retval FTDM_SUCCESS success
 * \retval FTDM_FAIL failure
 */
FT_DECLARE(ftdm_status_t) ftdm_set_screening_ind(const char *string, uint8_t *target);


/*! 
 * \brief Set the Presentation Ind from an enum
 *
 * \param screen_string string value
 * \param target the target to set value to
 *
 * \retval FTDM_SUCCESS success
 * \retval FTDM_FAIL failure
 */
FT_DECLARE(ftdm_status_t) ftdm_set_presentation_ind(const char *string, uint8_t *target);


/*! 
 * \brief Checks whether a string contains only numbers
 *
 * \param number string value
 *
 * \retval FTDM_SUCCESS success
 * \retval FTDM_FAIL failure
 */
FT_DECLARE(ftdm_status_t) ftdm_is_number(const char *number);

/*! 
 * \brief Set the Calling Party Category from an enum
 *
 * \param cpc_string string value
 * \param target the target to set value to
 *
 * \retval FTDM_SUCCESS success
 * \retval FTDM_FAIL failure
 */
FT_DECLARE(ftdm_status_t) ftdm_set_calling_party_category(const char *string, uint8_t *target);

/*! 
 * \brief URL encode a buffer
 *
 * \param url buffer to convert
 * \param buf target to save converted string to
 * \param len size of buffer
 *
 * \retval pointer to converted string
 */
FT_DECLARE(char *) ftdm_url_encode(const char *url, char *buf, ftdm_size_t len);

/*! 
 * \param s buffer to convert
 * \param len size of buffer
 *	
 * \retval pointer to converted string
 */
FT_DECLARE(char *) ftdm_url_decode(char *s, ftdm_size_t *len);

#endif /* __FTDM_CALL_UTILS_H__ */

