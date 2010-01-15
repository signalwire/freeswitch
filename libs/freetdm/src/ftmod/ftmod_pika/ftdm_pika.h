/*
 * Copyright (c) 2007, Anthony Minessale II
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

#ifndef FTDM_PIKA_H
#define FTDM_PIKA_H
#include "freetdm.h"
#include "pikahmpapi.h"



#define PIKA_STR2ENUM_P(_FUNC1, _FUNC2, _TYPE) _TYPE _FUNC1 (const char *name); const char * _FUNC2 (_TYPE type);
#define PIKA_STR2ENUM(_FUNC1, _FUNC2, _TYPE, _STRINGS, _MAX)	\
	_TYPE _FUNC1 (const char *name)								\
	{														\
		int i;												\
		_TYPE t = _MAX ;									\
															\
		for (i = 0; i < _MAX ; i++) {						\
			if (!strcasecmp(name, _STRINGS[i])) {			\
				t = (_TYPE) i;								\
				break;										\
			}												\
		}													\
															\
		return t;											\
	}														\
	const char * _FUNC2 (_TYPE type)						\
	{														\
		if (type > _MAX) {									\
			type = _MAX;									\
		}													\
		return _STRINGS[(int)type];							\
	}


typedef enum {
	PIKA_SPAN_FRAMING_T1_D4,
	PIKA_SPAN_FRAMING_T1_ESF,
	PIKA_SPAN_FRAMING_E1_BASIC,
	PIKA_SPAN_FRAMING_E1_CRC4,
	PIKA_SPAN_INVALID
} PIKA_TSpanFraming;
#define PIKA_SPAN_STRINGS "T1_D4", "T1_ESF", "E1_BASIC", "E1_CRC4"
PIKA_STR2ENUM_P(pika_str2span, pika_span2str, PIKA_TSpanFraming)

typedef enum {
	PIKA_SPAN_ENCODING_T1_AMI_ZS_NONE,
	PIKA_SPAN_ENCODING_T1_AMI_ZS_GTE,
	PIKA_SPAN_ENCODING_T1_AMI_ZS_BELL,
	PIKA_SPAN_ENCODING_T1_AMI_ZS_JAM8,
	PIKA_SPAN_ENCODING_T1_B8ZS,
	PIKA_SPAN_ENCODING_E1_AMI,
	PIKA_SPAN_ENCODING_E1_HDB3,
	PIKA_SPAN_ENCODING_INVALID
} PIKA_TSpanEncoding;
#define PIKA_SPAN_ENCODING_STRINGS "T1_AMI_ZS_NONE", "T1_AMI_ZS_GTE", "T1_AMI_ZS_BELL", "T1_AMI_ZS_JAM8", "T1_B8ZS", "E1_AMI", "E1_HDB3"
PIKA_STR2ENUM_P(pika_str2span_encoding, pika_span_encoding2str, PIKA_TSpanEncoding)

typedef enum {
	PIKA_SPAN_LOOP_LENGTH_SHORT_HAUL,
	PIKA_SPAN_LOOP_LENGTH_LONG_HAUL,
	PIKA_SPAN_LOOP_INVALID
} PIKA_TSpanLoopLength;
#define PIKA_LL_STRINGS "SHORT_HAUL", "LONG_HAUL"
PIKA_STR2ENUM_P(pika_str2loop_length, pika_loop_length2str, PIKA_TSpanLoopLength)

typedef enum {
	PIKA_SPAN_LBO_T1_LONG_0_DB,
	PIKA_SPAN_LBO_T1_LONG_7_DB,
	PIKA_SPAN_LBO_T1_LONG_15_DB,
	PIKA_SPAN_LBO_T1_LONG_22_DB,
	PIKA_SPAN_LBO_T1_SHORT_133_FT,
	PIKA_SPAN_LBO_T1_SHORT_266_FT,
	PIKA_SPAN_LBO_T1_SHORT_399_FT,
	PIKA_SPAN_LBO_T1_SHORT_533_FT,
	PIKA_SPAN_LBO_T1_SHORT_655_FT,
	PIKA_SPAN_LBO_E1_WAVEFORM_120_OHM,
	PIKA_SPAN_LBO_INVALID
} PIKA_TSpanBuildOut;
#define PIKA_LBO_STRINGS "T1_LONG_0_DB", "T1_LONG_7_DB", "T1_LONG_15_DB", "T1_LONG_22_DB", "T1_SHORT_133_FT", "T1_SHORT_266_FT", "T1_SHORT_399_FT", "T1_SHORT_533_FT", "T1_SHORT_655_FT", "E1_WAVEFORM_120_OHM"
PIKA_STR2ENUM_P(pika_str2lbo, pika_lbo2str, PIKA_TSpanBuildOut)

typedef enum {
	PIKA_SPAN_COMPAND_MODE_MU_LAW = 1,
	PIKA_SPAN_COMPAND_MODE_A_LAW,
	PIKA_SPAN_COMPAND_MODE_INVALID
} PIKA_TSpanCompandMode;
#define PIKA_SPAN_COMPAND_MODE_STRINGS "MU_LAW", "A_LAW"
PIKA_STR2ENUM_P(pika_str2compand_mode, pika_compand_mode2str, PIKA_TSpanCompandMode)

#endif

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
