/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2014, Anthony Minessale II <anthm@freeswitch.org>
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
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthm@freeswitch.org>
 *
 * switch_vidderbuffer.c -- Video Buffer
 *
 */
#include <switch.h>
#include <switch_vidderbuffer.h>

#define vb_debug(_vb, _level, _format, ...) if (_vb->debug_level >= _level) switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ALERT, "VB:%p level:%d line:%d ->" _format, (void *) _vb, _level, __LINE__,  __VA_ARGS__)

typedef struct switch_vb_node_s {
	struct switch_vb_frame_s *parent;
	switch_rtp_packet_t packet;
	uint32_t len;
	uint8_t visible;
	struct switch_vb_node_s *next;
} switch_vb_node_t;

typedef struct switch_vb_frame_s {
	struct switch_vb_s *parent;
	struct switch_vb_node_s *node_list;
	uint32_t ts;
	uint8_t visible;
	uint8_t complete;
	struct switch_vb_frame_s *next;
	uint16_t min_seq;
	uint16_t max_seq;
} switch_vb_frame_t;

struct switch_vb_s {
	struct switch_vb_frame_s *frame_list;
	struct switch_vb_frame_s *cur_read_frame;
	uint32_t last_read_ts;
	uint32_t last_read_seq;
	uint16_t target_seq;
	uint32_t visible_frames;
	uint32_t total_frames;
	uint32_t complete_frames;
	uint32_t frame_len;
	uint32_t min_frame_len;
	uint32_t max_frame_len;
	uint8_t debug_level;
};

static inline switch_vb_node_t *new_node(switch_vb_frame_t *frame)
{
	switch_vb_node_t *np, *last = NULL;

	for (np = frame->node_list; np; np = np->next) {
		if (!np->visible) {
			break;
		}
		last = np;
	}

	if (!np) {
		
		switch_zmalloc(np, sizeof(*np));
		np->parent = frame;
	
		if (last) {
			last->next = np;
		} else {
			frame->node_list = np;
		}

	}

	switch_assert(np);

	np->visible = 1;
	
	return np;
}

static inline void add_node(switch_vb_frame_t *frame, switch_rtp_packet_t *packet, switch_size_t len)
{
	switch_vb_node_t *node = new_node(frame);
	uint16_t seq = ntohs(packet->header.seq);

	node->packet = *packet;
	node->len = len;
	memcpy(node->packet.body, packet->body, len);

	if (seq < ntohs(frame->min_seq)) {
		frame->min_seq = packet->header.seq;
	} else if (seq > ntohs(frame->max_seq)) {
		frame->max_seq = packet->header.seq;
	}

	vb_debug(frame->parent, (packet->header.m ? 1 : 2), "PUT packet ts:%u seq:%u %s\n", 
			 ntohl(node->packet.header.ts), ntohs(node->packet.header.seq), packet->header.m ? "FINAL" : "PARTIAL");

	if (packet->header.m) {
		frame->complete = 1;
		frame->parent->complete_frames++;
	}
}

static inline void hide_node(switch_vb_node_t *node)
{
	node->visible = 0;
}

static inline void hide_nodes(switch_vb_frame_t *frame)
{
	switch_vb_node_t *np;

	for (np = frame->node_list; np; np = np->next) {
		hide_node(np);
	}
}

static inline void hide_frame(switch_vb_frame_t *frame)
{
	frame->visible = 0;
	frame->min_seq = frame->max_seq = 0;
	frame->parent->visible_frames--;

	if (frame->complete) {
		frame->parent->complete_frames--;
	}

	hide_nodes(frame);
}

static inline switch_vb_frame_t *new_frame(switch_vb_t *vb, switch_rtp_packet_t *packet)
{
	switch_vb_frame_t *fp, *last = NULL;

	for (fp = vb->frame_list; fp; fp = fp->next) {
		if (fp->ts == packet->header.ts) {
			if (fp->complete || !fp->visible) {
				return NULL;
			} else {
				break;
			}
		}
	}

	if (!fp) {
		for (fp = vb->frame_list; fp; fp = fp->next) {
			if (!fp->visible) {
				break;
			}
			last = fp;
		}
	}

	if (!fp) {
		switch_zmalloc(fp, sizeof(*fp));
		fp->parent = vb;
		vb->total_frames++;

		if (last) {
			last->next = fp;
		} else {
			vb->frame_list = fp;
		}
	}

	switch_assert(fp);

	vb->visible_frames++;
	fp->visible = 1;
	fp->complete = 0;
	fp->ts = packet->header.ts;
	fp->min_seq = fp->max_seq = 0;

	return fp;

}

static inline int frame_contains_seq(switch_vb_frame_t *frame, uint16_t target_seq)
{
	int16_t seq = ntohs(target_seq);

	if (frame->min_seq && frame->max_seq && seq >= ntohs(frame->min_seq) && seq <= ntohs(frame->max_seq)) {
		return 1;
	}

	return 0;
}

static inline switch_vb_frame_t *next_frame(switch_vb_t *vb)
{
	switch_vb_frame_t *fp = NULL, *oldest = NULL, *frame_containing_seq = NULL;

	for (fp = vb->frame_list; fp; fp = fp->next) {

		if (!fp->visible || !fp->complete) {
			continue;
		}

		if (vb->target_seq) {
			if (frame_contains_seq(fp, vb->target_seq)) {
				vb_debug(fp->parent, 2, "FOUND FRAME CONTAINING SEQ %d\n", ntohs(vb->target_seq));
				frame_containing_seq = fp;
				break;
			}
		}

		if ((!oldest || htonl(oldest->ts) > htonl(fp->ts))) {
			oldest = fp;
		}
	}


	if (frame_containing_seq) {
		return frame_containing_seq;
	}

	return oldest;
}

static inline void set_read_seq(switch_vb_t *vb, uint16_t seq)
{
	vb->last_read_seq = seq;
	vb->target_seq = htons((ntohs(vb->last_read_seq) + 1));
}

static inline switch_vb_node_t *frame_find_next_seq(switch_vb_frame_t *frame)
{
	switch_vb_node_t *np;
	
	for (np = frame->node_list; np; np = np->next) {
		if (!np->visible) continue;

		if (ntohs(np->packet.header.seq) == ntohs(frame->parent->target_seq)) {
			hide_node(np);
			set_read_seq(frame->parent, np->packet.header.seq);
			return np;
		}
	}

	return NULL;
}


static inline switch_vb_node_t *frame_find_lowest_seq(switch_vb_frame_t *frame)
{
	switch_vb_node_t *np, *lowest = NULL;
	
	for (np = frame->node_list; np; np = np->next) {
		if (!np->visible) continue;

		if (!lowest || ntohs(lowest->packet.header.seq) > ntohs(np->packet.header.seq)) {
			hide_node(np);
			lowest = np;
		}
	}

	if (lowest) {
		set_read_seq(frame->parent, lowest->packet.header.seq);
	}

	return lowest;
}

static inline switch_vb_node_t *next_frame_packet(switch_vb_t *vb)
{
	switch_vb_node_t *node;

	if (vb->last_read_seq) {
		node = frame_find_next_seq(vb->cur_read_frame);
	} else {
		printf("WTF LOWEST ==============\n");
		node = frame_find_lowest_seq(vb->cur_read_frame);
	}

	return node;
}

static inline void free_nodes(switch_vb_frame_t *frame)
{
	switch_vb_node_t *np = frame->node_list, *cur;

	while(np) {
		cur = np;
		np = np->next;
		free(cur);
	}
	
	frame->node_list = NULL;
}

static inline void free_frames(switch_vb_t *vb)
{
	switch_vb_frame_t *fp = vb->frame_list, *cur = NULL;

	while(fp) {
		cur = fp;
		fp = fp->next;
		free_nodes(cur);
		free(cur);
	}

	vb->frame_list = NULL;
}

static inline void do_flush(switch_vb_t *vb)
{
	switch_vb_frame_t *fp = vb->frame_list;

	while(fp) {
		hide_frame(fp);
		fp = fp->next;
	}
}


SWITCH_DECLARE(int) switch_vb_frame_count(switch_vb_t *vb)
{
	return vb->complete_frames;
}

SWITCH_DECLARE(void) switch_vb_debug_level(switch_vb_t *vb, uint8_t level)
{
	vb->debug_level = level;
}

SWITCH_DECLARE(void) switch_vb_reset(switch_vb_t *vb, switch_bool_t flush)
{
	if (vb->cur_read_frame) {
		hide_frame(vb->cur_read_frame);
		vb->cur_read_frame = NULL;
	}

	vb->last_read_ts = 0;
	vb->last_read_seq = 0;
	vb->target_seq = 0;

	if (flush) {
		do_flush(vb);
	}
}

SWITCH_DECLARE(switch_status_t) switch_vb_create(switch_vb_t **vbp, uint32_t min_frame_len, uint32_t max_frame_len)
{
	switch_vb_t *vb;
	switch_zmalloc(vb, sizeof(*vb));
	
	vb->min_frame_len = vb->frame_len = min_frame_len;
	vb->max_frame_len = max_frame_len;

	*vbp = vb;

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_vb_destroy(switch_vb_t **vbp)
{
	switch_vb_t *vb = *vbp;
	*vbp = NULL;

	free_frames(vb);
	free(vb);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_vb_put_packet(switch_vb_t *vb, switch_rtp_packet_t *packet, switch_size_t len)
{
	switch_vb_frame_t *frame;

	if ((frame = new_frame(vb, packet))) {
		add_node(frame, packet, len);
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_IGNORE;
}

SWITCH_DECLARE(switch_status_t) switch_vb_get_packet(switch_vb_t *vb, switch_rtp_packet_t *packet, switch_size_t *len)
{
	switch_vb_node_t *node = NULL;
	int fail = 0;

	if (vb->complete_frames < vb->frame_len) {
		vb_debug(vb, 2, "BUFFERING %u/%u\n", vb->complete_frames , vb->frame_len);
		return SWITCH_STATUS_MORE_DATA;
	}

	do {
		if (vb->cur_read_frame) {
			vb_debug(vb, 2, "Search for next frame cur ts: %u\n", htonl(vb->cur_read_frame->ts));
			if (!(node = next_frame_packet(vb))) {
				vb_debug(vb, 1, "Cannot find frame cur ts %u ... RESET!\n", htonl(vb->cur_read_frame->ts));
				switch_vb_reset(vb, SWITCH_FALSE);
				fail++;
			}
		} else {
			if (!(vb->cur_read_frame = next_frame(vb))) {
				break;
			}
		}
	} while (!node && fail < 2);


	if (node) {
		*packet = node->packet;
		*len = node->len;
		memcpy(packet->body, node->packet.body, node->len);
		
		vb->last_read_ts = node->packet.header.ts;

		vb_debug(vb, 1, "GET packet ts:%u seq:%u\n", ntohl(packet->header.ts), ntohs(packet->header.seq));

		if (vb->cur_read_frame && node->packet.header.m) {
			hide_frame(vb->cur_read_frame);
			vb->cur_read_frame = NULL;
		}

		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_MORE_DATA;

}


/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
