/*
 * mod_rayo for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2013, Grasshopper
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mod_rayo for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is Grasshopper
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Chris Rienzo <chris.rienzo@grasshopper.com>
 *
 * rayo_elements.c -- Rayo XML element definition
 *
 */
#include "rayo_elements.h"

/**
 * <input> component validation
 */
ELEMENT(RAYO_INPUT)
	STRING_ATTRIB(mode, any, "any,dtmf,voice")
	OPTIONAL_ATTRIB(terminator,, dtmf_digit)
	ATTRIB(recognizer,, any)
	ATTRIB(language, en-US, any)
	ATTRIB(initial-timeout, -1, positive_or_neg_one)
	ATTRIB(inter-digit-timeout, -1, positive_or_neg_one)
	ATTRIB(sensitivity, 0.5, decimal_between_zero_and_one)
	ATTRIB(min-confidence, 0, decimal_between_zero_and_one)
	ATTRIB(max-silence, -1, positive_or_neg_one)
	/* for now, only NLSML */
	STRING_ATTRIB(match-content-type, application/nlsml+xml, "application/nlsml+xml")
	/* internal attribs for prompt support */
	ATTRIB(barge-event, false, bool)
	ATTRIB(start-timers, true, bool)
ELEMENT_END

/**
 * <output> component validation
 */
ELEMENT(RAYO_OUTPUT)
	ATTRIB(start-offset, 0, not_negative)
	ATTRIB(start-paused, false, bool)
	ATTRIB(repeat-interval, 0, not_negative)
	ATTRIB(repeat-times, 1, not_negative)
	ATTRIB(max-time, -1, positive_or_neg_one)
	ATTRIB(renderer,, any)
	ATTRIB(voice,, any)
ELEMENT_END

/**
 * <output><seek> validation
 */
ELEMENT(RAYO_OUTPUT_SEEK)
	STRING_ATTRIB(direction,, "forward,back")
	ATTRIB(amount,-1, positive)
ELEMENT_END

/**
 * <prompt> component validation
 */
ELEMENT(RAYO_PROMPT)
	ATTRIB(barge-in, true, bool)
ELEMENT_END

/**
 * <record> component validation
 */
ELEMENT(RAYO_RECORD)
	ATTRIB(format, wav, any)
	ATTRIB(start-beep, false, bool)
	ATTRIB(stop-beep, false, bool)
	ATTRIB(start-paused, false, bool)
	ATTRIB(max-duration, -1, positive_or_neg_one)
	ATTRIB(initial-timeout, -1, positive_or_neg_one)
	ATTRIB(final-timeout, -1, positive_or_neg_one)
	STRING_ATTRIB(direction, duplex, "duplex,send,recv")
	ATTRIB(mix, false, bool)
ELEMENT_END

/**
 * <join> command validation
 */
ELEMENT(RAYO_JOIN)
	STRING_ATTRIB(direction, duplex, "send,recv,duplex")
	STRING_ATTRIB(media, bridge, "bridge,direct")
	ATTRIB(call-uri,, any)
	ATTRIB(mixer-name,, any)
ELEMENT_END


/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet
 */

