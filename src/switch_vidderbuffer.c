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

struct switch_vb_s;

typedef struct switch_vb_node_s {
	struct switch_vb_s *parent;
	switch_rtp_packet_t packet;
	uint32_t len;
	uint8_t visible;
	struct switch_vb_node_s *next;
} switch_vb_node_t;

struct switch_vb_s {
	struct switch_vb_node_s *node_list;
	uint32_t last_target_seq;
	uint32_t highest_read_ts;
	uint32_t highest_read_seq;
	uint32_t highest_wrote_ts;
	uint32_t highest_wrote_seq;
	uint16_t target_seq;
	uint16_t seq_out;
	uint32_t visible_nodes;
	uint32_t total_frames;
	uint32_t complete_frames;
	uint32_t frame_len;
	uint32_t min_frame_len;
	uint32_t max_frame_len;
	uint8_t write_init;
	uint8_t read_init;
	uint8_t debug_level;
	uint16_t next_seq;
	switch_inthash_t *missing_seq_hash;
	switch_inthash_t *node_hash;
};

static inline switch_vb_node_t *new_node(switch_vb_t *vb)
{
	switch_vb_node_t *np, *last = NULL;

	for (np = vb->node_list; np; np = np->next) {
		if (!np->visible) {
			break;
		}
		last = np;
	}

	if (!np) {
		
		switch_zmalloc(np, sizeof(*np));
	
		if (last) {
			last->next = np;
		} else {
			vb->node_list = np;
		}

	}

	switch_assert(np);

	np->visible = 1;
	vb->visible_nodes++;
	np->parent = vb;

	return np;
}

static inline switch_vb_node_t *find_seq(switch_vb_t *vb, uint16_t seq)
{
	switch_vb_node_t *np;
	for (np = vb->node_list; np; np = np->next) {
		if (!np->visible) continue;
			
		if (ntohs(np->packet.header.seq) == ntohs(seq)) {
			return np;
		}
	}

	return NULL;
}

static inline void hide_node(switch_vb_node_t *node)
{
	node->visible = 0;
	node->parent->visible_nodes--;
	switch_core_inthash_delete(node->parent->node_hash, node->packet.header.seq);
}

static inline void hide_nodes(switch_vb_t *vb)
{
	switch_vb_node_t *np;

	for (np = vb->node_list; np; np = np->next) {
		hide_node(np);
	}
}

static inline void drop_ts(switch_vb_t *vb, uint32_t ts)
{
	switch_vb_node_t *np;
	int x = 0;

	for (np = vb->node_list; np; np = np->next) {
		if (!np->visible) continue;

		if (ts == np->packet.header.ts) {
			hide_node(np);
			x++;
		}
	}

	if (x) vb->complete_frames--;
}

static inline uint32_t vb_find_lowest_ts(switch_vb_t *vb)
{
	switch_vb_node_t *np, *lowest = NULL;
	
	for (np = vb->node_list; np; np = np->next) {
		if (!np->visible) continue;

		if (!lowest || ntohl(lowest->packet.header.ts) > ntohl(np->packet.header.ts)) {
			lowest = np;
		}
	}

	return lowest ? lowest->packet.header.ts : 0;
}

static inline void drop_oldest_frame(switch_vb_t *vb)
{
	uint32_t ts = vb_find_lowest_ts(vb);

	drop_ts(vb, ts);
	vb_debug(vb, 1, "Dropping oldest frame ts:%u\n", ntohl(ts));
}

static inline void add_node(switch_vb_t *vb, switch_rtp_packet_t *packet, switch_size_t len)
{
	switch_vb_node_t *node = new_node(vb);

	node->packet = *packet;
	node->len = len;
	memcpy(node->packet.body, packet->body, len);

	switch_core_inthash_insert(vb->node_hash, node->packet.header.seq, node);

	vb_debug(vb, (packet->header.m ? 1 : 2), "PUT packet last_ts:%u ts:%u seq:%u%s\n", 
			 ntohl(vb->highest_wrote_ts), ntohl(node->packet.header.ts), ntohs(node->packet.header.seq), packet->header.m ? " <MARK>" : "");

	if (!vb->write_init || ntohs(packet->header.seq) > ntohs(vb->highest_wrote_seq) || 
		(ntohs(vb->highest_wrote_seq) > USHRT_MAX - 10 && ntohs(packet->header.seq) <= 10) ) {
		vb->highest_wrote_seq = packet->header.seq;
	}

	if (vb->write_init && htons(packet->header.seq) >= htons(vb->highest_wrote_seq) && (ntohl(node->packet.header.ts) > ntohl(vb->highest_wrote_ts))) {
		vb->complete_frames++;
		vb_debug(vb, 2, "WRITE frame ts: %u complete=%u/%u n:%u\n", ntohl(node->packet.header.ts), vb->complete_frames , vb->frame_len, vb->visible_nodes);
		vb->highest_wrote_ts = packet->header.ts;
	} else if (!vb->write_init) {
		vb->highest_wrote_ts = packet->header.ts;
	}
	
	if (!vb->write_init) vb->write_init = 1;

	if (vb->complete_frames > vb->max_frame_len) {
		drop_oldest_frame(vb);
	}
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

static inline switch_vb_node_t *vb_find_lowest_seq(switch_vb_t *vb)
{
	switch_vb_node_t *np, *lowest = NULL;
	
	for (np = vb->node_list; np; np = np->next) {
		if (!np->visible) continue;

		if (!lowest || ntohs(lowest->packet.header.seq) > ntohs(np->packet.header.seq)) {
			lowest = np;
		}
	}

	return lowest;
}

static inline switch_status_t vb_next_packet(switch_vb_t *vb, switch_vb_node_t **nodep)
{
	switch_vb_node_t *np = NULL, *node = NULL;
	switch_status_t status;

	if (np) status = 0, status++;

	if (!vb->target_seq) {
		if ((node = vb_find_lowest_seq(vb))) {
			vb_debug(vb, 2, "No target seq using seq: %u as a starting point\n", ntohs(node->packet.header.seq));
		} else {
			vb_debug(vb, 1, "%s", "No nodes available....\n");
		}
	} else if ((node = switch_core_inthash_find(vb->node_hash, vb->target_seq))) {
		vb_debug(vb, 2, "FOUND desired seq: %u\n", ntohs(vb->target_seq));
	} else {
		int x;

		vb_debug(vb, 2, "MISSING desired seq: %u\n", ntohs(vb->target_seq));

		for (x = 0; x < 10; x++) {
			increment_seq(vb);
			if ((node = switch_core_inthash_find(vb->node_hash, vb->target_seq))) {
				vb_debug(vb, 2, "FOUND incremental seq: %u\n", ntohs(vb->target_seq));
				break;
			} else {
				vb_debug(vb, 2, "MISSING incremental seq: %u\n", ntohs(vb->target_seq));
			}
		}
	}

	*nodep = node;
	
	if (node) {
		set_read_seq(vb, node->packet.header.seq);
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_NOTFOUND;
	
}

static inline void free_nodes(switch_vb_t *vb)
{
	switch_vb_node_t *np = vb->node_list, *cur;

	while(np) {
		cur = np;
		np = np->next;
		free(cur);
	}
	
	vb->node_list = NULL;
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

	vb->last_target_seq = 0;
	vb->target_seq = 0;

	if (flush) {
		//do_flush(vb);
	}
}

SWITCH_DECLARE(switch_status_t) switch_vb_create(switch_vb_t **vbp, uint32_t min_frame_len, uint32_t max_frame_len)
{
	switch_vb_t *vb;
	switch_zmalloc(vb, sizeof(*vb));
	
	vb->min_frame_len = vb->frame_len = min_frame_len;
	vb->max_frame_len = max_frame_len;
	//vb->seq_out = (uint16_t) rand();
	switch_core_inthash_init(&vb->missing_seq_hash);
	switch_core_inthash_init(&vb->node_hash);

	*vbp = vb;

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_vb_destroy(switch_vb_t **vbp)
{
	switch_vb_t *vb = *vbp;
	*vbp = NULL;
	
	switch_core_inthash_destroy(&vb->missing_seq_hash);
	switch_core_inthash_destroy(&vb->node_hash);

	free_nodes(vb);
	free(vb);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(uint32_t) switch_vb_pop_nack(switch_vb_t *vb)
{
	switch_hash_index_t *hi = NULL;
	uint32_t nack = 0;
	uint16_t least = 0;
	int i = 0;

	void *val;
	const void *var;

	for (hi = switch_core_hash_first(vb->missing_seq_hash); hi; hi = switch_core_hash_next(&hi)) {
		uint16_t seq;
		
		switch_core_hash_this(hi, &var, NULL, &val);
		seq = ntohs(*((uint16_t *) var));

		if (!least || seq < least) {
			least = seq;
		}
	}

	if (least && switch_core_inthash_delete(vb->missing_seq_hash, (uint32_t)htons(least))) {
		vb_debug(vb, 3, "Found smallest NACKABLE seq %u\n", least);
		nack = (uint32_t) htons(least);
	
		for (i = 1; i > 17; i++) {
			if (switch_core_inthash_delete(vb->missing_seq_hash, (uint32_t)htons(least + i))) {
				vb_debug(vb, 3, "Found addtl NACKABLE seq %u\n", least + i);
				nack |= (1 << (16 + i));
			} else {
				break;
			}
		}
	}

	return nack;
}

SWITCH_DECLARE(switch_status_t) switch_vb_push_packet(switch_vb_t *vb, switch_rtp_packet_t *packet, switch_size_t len)
{
	add_node(vb, packet, len);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_vb_put_packet(switch_vb_t *vb, switch_rtp_packet_t *packet, switch_size_t len)
{
	uint32_t i;
	uint16_t want = ntohs(vb->next_seq), got = ntohs(packet->header.seq);

	if (!want) want = got;
	
	if (got > want) {
		for (i = want; i < got; i++) {
			vb_debug(vb, 2, "MARK SEQ MISSING %u\n", i);
			switch_core_inthash_insert(vb->missing_seq_hash, (uint32_t)htons(i), (void *)SWITCH_TRUE);
		}
	} else {
		if (switch_core_inthash_delete(vb->missing_seq_hash, (uint32_t)htons(got))) {
			vb_debug(vb, 2, "MARK SEQ FOUND %u\n", got);
		}
	}

	if (got >= want) {
		vb->next_seq = htons(ntohs(packet->header.seq) + 1);
	}

	add_node(vb, packet, len);
	
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_vb_get_packet_by_seq(switch_vb_t *vb, uint16_t seq, switch_rtp_packet_t *packet, switch_size_t *len)
{
	switch_vb_node_t *node;
	
	if ((node = switch_core_inthash_find(vb->node_hash, seq))) {
		vb_debug(vb, 2, "Found buffered seq: %u\n", ntohs(seq));
		*packet = node->packet;
		*len = node->len;
		memcpy(packet->body, node->packet.body, node->len);
		return SWITCH_STATUS_SUCCESS;
	} else {
		vb_debug(vb, 2, "Missing buffered seq: %u\n", ntohs(seq));
	}

	return SWITCH_STATUS_NOTFOUND;
}

SWITCH_DECLARE(switch_status_t) switch_vb_get_packet(switch_vb_t *vb, switch_rtp_packet_t *packet, switch_size_t *len)
{
	switch_vb_node_t *node = NULL;
	switch_status_t status;
	
	vb_debug(vb, 2, "GET PACKET %u/%u n:%d\n", vb->complete_frames , vb->frame_len, vb->visible_nodes);

	if (vb->complete_frames < vb->frame_len) {
		vb_debug(vb, 2, "BUFFERING %u/%u\n", vb->complete_frames , vb->frame_len);
		return SWITCH_STATUS_MORE_DATA;
	}

	if ((status = vb_next_packet(vb, &node)) == SWITCH_STATUS_SUCCESS) {
		vb_debug(vb, 2, "Found next frame cur ts: %u seq: %u\n", htonl(node->packet.header.ts), htons(node->packet.header.seq));

		if (!vb->read_init || ntohs(node->packet.header.seq) > ntohs(vb->highest_read_seq) || 
			(ntohs(vb->highest_read_seq) > USHRT_MAX - 10 && ntohs(node->packet.header.seq) <= 10) ) {
			vb->highest_read_seq = node->packet.header.seq;
		}
		
		if (vb->read_init && htons(node->packet.header.seq) >= htons(vb->highest_read_seq) && (ntohl(node->packet.header.ts) > ntohl(vb->highest_read_ts))) {
			vb->complete_frames--;
			vb_debug(vb, 2, "READ frame ts: %u complete=%u/%u n:%u\n", ntohl(node->packet.header.ts), vb->complete_frames , vb->frame_len, vb->visible_nodes);
			vb->highest_read_ts = node->packet.header.ts;
		} else if (!vb->read_init) {
			vb->highest_read_ts = node->packet.header.ts;
		}
		
		if (!vb->read_init) vb->read_init = 1;


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
		hide_node(node);

		vb_debug(vb, 1, "GET packet ts:%u seq:%u~%u%s\n", ntohl(packet->header.ts), ntohs(packet->header.seq), vb->seq_out, packet->header.m ? " <MARK>" : "");
		//packet->header.seq = htons(vb->seq_out++);

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
