/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2020, Anthony Minessale II <anthm@freeswitch.org>
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
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Seven Du <dujinfang@gmail.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Seven Du <dujinfang@gmail.com>
 *
 * switch_packetizer H264 packetizer
 *
 */

#ifndef SWITCH_PACKETIZER_H
#define SWITCH_PACKETIZER_H

typedef void switch_packetizer_t;

typedef enum {
    SPT_H264_BITSTREAM, // with separator 0 0 0 1 or 0 0 1
    SPT_H264_SIZED_BITSTREAM,
    SPT_H264_SIGNALE_NALU,
    SPT_VP8_BITSTREAM,
    SPT_VP9_BITSTREAM,

    // no more beyond this line
    SPT_INVALID_STREAM
} switch_packetizer_bitstream_t;

/*

    create a packetizer and feed data, to avoid data copy, data MUST be valid before the next feed, or before close.

 */

SWITCH_DECLARE(switch_packetizer_t *) switch_packetizer_create(switch_packetizer_bitstream_t type, uint32_t slice_size);
SWITCH_DECLARE(switch_status_t) switch_packetizer_feed(switch_packetizer_t *packetizer, void *data, uint32_t size);
SWITCH_DECLARE(switch_status_t) switch_packetizer_feed_extradata(switch_packetizer_t *packetizer, void *data, uint32_t size);
SWITCH_DECLARE(switch_status_t) switch_packetizer_read(switch_packetizer_t *packetizer, switch_frame_t *frame);
SWITCH_DECLARE(void) switch_packetizer_close(switch_packetizer_t **packetizer);

#endif
