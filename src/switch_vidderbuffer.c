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

#define MAX_MISSING_SEQ 20
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
	uint32_t visible_nodes;
	uint8_t visible;
	uint8_t complete;
	uint8_t mark;
	struct switch_vb_frame_s *next;
	uint16_t min_seq;
	uint16_t max_seq;
} switch_vb_frame_t;

struct switch_vb_s {
	struct switch_vb_frame_s *frame_list;
	struct switch_vb_frame_s *cur_read_frame;
	struct switch_vb_frame_s *cur_write_frame;
	uint32_t last_read_ts;
	uint32_t last_read_seq;
	uint32_t last_target_seq;
	uint32_t last_wrote_ts;
	uint32_t last_wrote_seq;
	uint16_t target_seq;
	uint16_t seq_out;
	uint32_t visible_frames;
	uint32_t total_frames;
	uint32_t complete_frames;
	uint32_t frame_len;
	uint32_t min_frame_len;
	uint32_t max_frame_len;
	uint8_t debug_level;
	switch_timer_t timer;
	int cur_errs;
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
	np->parent->visible_nodes++;

	return np;
}

static inline void add_node(switch_vb_frame_t *frame, switch_rtp_packet_t *packet, switch_size_t len)
{
	switch_vb_node_t *node = new_node(frame);
	uint16_t seq = ntohs(packet->header.seq);

	node->packet = *packet;
	node->len = len;
	memcpy(node->packet.body, packet->body, len);

	if (!frame->min_seq ||seq < ntohs(frame->min_seq)) {
		frame->min_seq = packet->header.seq;
	}

	if (seq > ntohs(frame->max_seq)) {
		frame->max_seq = packet->header.seq;
	}

	vb_debug(frame->parent, (packet->header.m ? 1 : 2), "PUT packet last_ts:%u ts:%u seq:%u%s\n", 
			 ntohl(frame->parent->last_wrote_ts), ntohl(node->packet.header.ts), ntohs(node->packet.header.seq), packet->header.m ? " <MARK>" : "");


	if (packet->header.m) {
		frame->mark = 1;
	}
	
	if ((frame->parent->last_wrote_ts && frame->parent->last_wrote_ts != node->packet.header.ts)) {
		frame->complete = 1;
		frame->parent->complete_frames++;
	}

	frame->parent->last_wrote_ts = packet->header.ts;
	frame->parent->last_wrote_seq = packet->header.seq;
}

static inline void hide_node(switch_vb_node_t *node)
{
	if (node->visible) {
		node->visible = 0;
		node->parent->visible_nodes--;
	}
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
	vb_debug(frame->parent, 2, "Hide frame ts: %u\n", ntohl(frame->ts));

	if (frame->visible) {
		frame->visible = 0;
		frame->parent->visible_frames--;
	}

	if (frame->complete) {
		frame->parent->complete_frames--;
		frame->complete = 0;
	}

	frame->min_seq = frame->max_seq = 0;

	hide_nodes(frame);
}

static inline switch_vb_frame_t *new_frame(switch_vb_t *vb, switch_rtp_packet_t *packet)
{
	switch_vb_frame_t *fp = NULL, *last = NULL;
	int new = 1;

	if (vb->cur_write_frame) {
		if (!vb->cur_write_frame->visible) {
			vb->cur_write_frame = NULL;
			return NULL;
		} else if (vb->cur_write_frame->ts == packet->header.ts) {
			fp = vb->cur_write_frame;
			new = 0;
		}
	}

	if (!fp) {
		for (fp = vb->frame_list; fp; fp = fp->next) {
			if (fp->ts == packet->header.ts) {
				if (!fp->visible) {
					return NULL;
				} else {
					new = 0;
					break;
				}
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

	if (new) {
		vb->visible_frames++;
		fp->visible = 1;
		fp->complete = 0;
		fp->ts = packet->header.ts;
		fp->min_seq = fp->max_seq = 0;
		fp->mark = 0;
	}

	vb->cur_write_frame = fp;
	
	return fp;

}

static inline int frame_contains_seq(switch_vb_frame_t *frame, uint16_t target_seq, switch_vb_node_t **nodep)
{
	uint16_t seq = ntohs(target_seq);
	switch_vb_node_t *np;
		
	for (np = frame->node_list; np; np = np->next) {
		if (!np->visible) {
			continue;
		}
		//vb_debug(frame->parent, 10, "    CMP %u %u/%u\n", ntohl(frame->ts), ntohs(np->packet.header.seq), seq);
		if (ntohs(np->packet.header.seq) == seq) {
			//vb_debug(frame->parent, 10, "      MATCH %u %u v:%d\n", ntohs(np->packet.header.seq), seq, np->visible);
			if (nodep) {
				*nodep = np;
			}
			return 1;
		}
	}

	return 0;
}

static inline void increment_seq(switch_vb_t *vb)
{
	vb->target_seq = htons((ntohs(vb->target_seq) + 1));
}

static inline void set_read_seq(switch_vb_t *vb, uint16_t seq)
{
	vb->last_target_seq = seq;
	vb->target_seq = htons((ntohs(vb->last_target_seq) + 1));
}

static inline switch_status_t next_frame(switch_vb_t *vb, switch_vb_node_t **nodep)
{
	switch_vb_frame_t *fp = NULL, *oldest = NULL, *frame_containing_seq = NULL;

	if ((fp = vb->cur_read_frame)) {
		if (fp->visible_nodes == 0) {
			hide_frame(fp);
			vb->cur_read_frame = NULL;
		}
	}


	if ((fp = vb->cur_read_frame)) {
		int ok = 1;

		if (!fp->visible || fp->visible_nodes == 0) {
			ok = 0;
		} else {
			if (vb->target_seq) {
				if (frame_contains_seq(fp, vb->target_seq, nodep)) {
					vb_debug(vb, 2, "CUR FRAME %u CONTAINS REQUESTED SEQ %d\n", ntohl(fp->ts), ntohs(vb->target_seq));
					frame_containing_seq = fp;
					goto end;
				} else {
					ok = 0;
				}
			}
		}

		if (!ok) {
			vb_debug(vb, 2, "DONE WITH CUR FRAME %u v: %d c: %d\n", ntohl(fp->ts), fp->visible, fp->complete);
			vb->cur_read_frame = NULL;
		}
	}

	do {
		*nodep = NULL;

		for (fp = vb->frame_list; fp; fp = fp->next) {
			if (!fp->visible || !fp->complete) {
				continue;
			}

			if (vb->target_seq) {
				if (frame_contains_seq(fp, vb->target_seq, nodep)) {
					vb_debug(vb, 2, "FOUND FRAME %u CONTAINING SEQ %d\n", ntohl(fp->ts), ntohs(vb->target_seq));
					frame_containing_seq = fp;
					goto end;
				}
			}
			
			if ((!oldest || htonl(oldest->ts) > htonl(fp->ts))) {
				oldest = fp;
			}
		}

		if (!frame_containing_seq && vb->target_seq) {
			if (ntohs(vb->target_seq) - ntohs(vb->last_target_seq) > MAX_MISSING_SEQ) {
				vb_debug(vb, 1, "FOUND NO FRAMES CONTAINING SEQ %d. Too many failures....\n", ntohs(vb->target_seq));
				switch_vb_reset(vb, SWITCH_FALSE);
			} else {
				vb_debug(vb, 2, "FOUND NO FRAMES CONTAINING SEQ %d. Try next one\n", ntohs(vb->target_seq));
				increment_seq(vb);
				vb->cur_errs++;
			}
		}
	} while (!frame_containing_seq && vb->target_seq);
	
 end:

	if (frame_containing_seq) {
		vb->cur_read_frame = frame_containing_seq;
		if (nodep && *nodep) {
			hide_node(*nodep);
			set_read_seq(vb, (*nodep)->packet.header.seq);
		}
	} else if (oldest) {
		vb->cur_read_frame = oldest;
	} else {
		vb->cur_read_frame = NULL;
	}

	if (vb->cur_read_frame) {
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_NOTFOUND;
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

static inline switch_status_t next_frame_packet(switch_vb_t *vb, switch_vb_node_t **nodep)
{
	switch_vb_node_t *node = NULL;
	switch_status_t status;

	if ((status = next_frame(vb, &node) != SWITCH_STATUS_SUCCESS)) {
		return status;
	}
	
	if (!node) {
		if (vb->target_seq) {
			vb_debug(vb, 2, "Search for next packet %u cur ts: %u\n", htons(vb->target_seq), htonl(vb->cur_read_frame->ts));
			node = frame_find_next_seq(vb->cur_read_frame);
		} else {
			node = frame_find_lowest_seq(vb->cur_read_frame);
			vb_debug(vb, 2, "Find lowest seq frame ts: %u seq: %u\n", ntohl(vb->cur_read_frame->ts), ntohs(node->packet.header.seq));
		}
	}

	*nodep = node;
	
	if (node) {
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_NOTFOUND;
	
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

SWITCH_DECLARE(int) switch_vb_poll(switch_vb_t *vb)
{
	return (vb->complete_frames >= vb->frame_len);
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
	vb_debug(vb, 2, "RESET BUFFER flush: %d\n", (int)flush);


	if (vb->cur_read_frame) {
		vb->cur_read_frame = NULL;
	}

	vb->last_read_ts = 0;
	vb->last_target_seq = 0;
	vb->target_seq = 0;

	if (flush) {
		do_flush(vb);
	}
}

SWITCH_DECLARE(switch_status_t) switch_vb_create(switch_vb_t **vbp, uint32_t min_frame_len, uint32_t max_frame_len, switch_bool_t timer_compensation)
{
	switch_vb_t *vb;
	switch_zmalloc(vb, sizeof(*vb));
	
	vb->min_frame_len = vb->frame_len = min_frame_len;
	vb->max_frame_len = max_frame_len;
	//vb->seq_out = (uint16_t) rand();

	if (timer_compensation) { /* rewrite timestamps and seq as they are read to hide packet loss */
		switch_core_timer_init(&vb->timer, "soft", 1, 90, NULL);
	}

	*vbp = vb;

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_vb_destroy(switch_vb_t **vbp)
{
	switch_vb_t *vb = *vbp;
	*vbp = NULL;

	if (vb->timer.timer_interface) {
		switch_core_timer_destroy(&vb->timer);
	}

	free_frames(vb);
	free(vb);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_vb_put_packet(switch_vb_t *vb, switch_rtp_packet_t *packet, switch_size_t len)
{
	switch_vb_frame_t *frame;
	
#ifdef VB_PLOSS
	int r = (rand() % 10000) + 1;
	if (r <= 200) {
		vb_debug(vb, 1, "Simulate dropped packet ......... ts: %u seq: %u\n", ntohl(packet->header.ts), ntohs(packet->header.seq));
		return SWITCH_STATUS_SUCCESS;
	}
#endif
	
	if ((frame = new_frame(vb, packet))) {
		add_node(frame, packet, len);
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_IGNORE;
}

SWITCH_DECLARE(switch_status_t) switch_vb_get_packet(switch_vb_t *vb, switch_rtp_packet_t *packet, switch_size_t *len)
{
	switch_vb_node_t *node = NULL;
	switch_status_t status;
	
	vb->cur_errs = 0;

	if (vb->complete_frames < vb->frame_len) {
		vb_debug(vb, 2, "BUFFERING %u/%u\n", vb->complete_frames , vb->frame_len);
		return SWITCH_STATUS_MORE_DATA;
	}

	if ((status = next_frame_packet(vb, &node)) == SWITCH_STATUS_SUCCESS) {
		vb_debug(vb, 2, "Found next frame cur ts: %u seq: %u\n", htonl(vb->cur_read_frame->ts), htons(node->packet.header.seq));
	} else {
		switch_vb_reset(vb, SWITCH_FALSE);

		switch(status) {
		case SWITCH_STATUS_RESTART:
			vb_debug(vb, 2, "%s", "Error encountered ask for new keyframe\n");
			return SWITCH_STATUS_RESTART;
		case SWITCH_STATUS_NOTFOUND:
		default:
			vb_debug(vb, 2, "%s", "No frames found wait for more\n");
			return SWITCH_STATUS_MORE_DATA;
		}
	}
	
	if (node) {
		status = SWITCH_STATUS_SUCCESS;
		
		*packet = node->packet;
		*len = node->len;
		memcpy(packet->body, node->packet.body, node->len);
		
		if (vb->cur_errs) {
			vb_debug(vb, 1, "One or more Missing SEQ TS %u\n", ntohl(packet->header.ts));
			status = SWITCH_STATUS_BREAK;
		}

		vb->last_read_ts = packet->header.ts;
		vb->last_read_seq = packet->header.seq;

		if (vb->timer.timer_interface) {
			if (packet->header.m || !vb->timer.samplecount) {
				switch_core_timer_sync(&vb->timer);
			}
		}

		if (vb->cur_read_frame && vb->cur_read_frame->visible_nodes == 0 && !packet->header.m) {
			/* force mark bit */
			vb_debug(vb, 1, "LAST PACKET %u WITH NO MARK BIT, ADDIONG MARK BIT\n", ntohl(packet->header.ts));
			packet->header.m = 1;
			status = SWITCH_STATUS_BREAK;
		}

		vb_debug(vb, 1, "GET packet ts:%u seq:%u~%u%s\n", ntohl(packet->header.ts), ntohs(packet->header.seq), vb->seq_out, packet->header.m ? " <MARK>" : "");
		//packet->header.seq = htons(vb->seq_out++);

		if (vb->timer.timer_interface) {
			packet->header.ts = htonl(vb->timer.samplecount);
		}
		
		return status;
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
