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

const char *TOKEN_1 = "ONE";
const char *TOKEN_2 = "TWO";

struct switch_vb_s;

typedef struct switch_vb_node_s {
	struct switch_vb_s *parent;
	switch_rtp_packet_t packet;
	uint32_t len;
	uint8_t visible;
	struct switch_vb_node_s *prev;
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
	uint32_t visible_nodes;
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
	switch_mutex_t *mutex;
	switch_mutex_t *list_mutex;
	switch_memory_pool_t *pool;
	int free_pool;
	switch_vb_flag_t flags;
};

static inline switch_vb_node_t *new_node(switch_vb_t *vb)
{
	switch_vb_node_t *np;

	switch_mutex_lock(vb->list_mutex);

	for (np = vb->node_list; np; np = np->next) {
		if (!np->visible) {
			break;
		}
	}

	if (!np) {
		
		np = switch_core_alloc(vb->pool, sizeof(*np));
		
		np->next = vb->node_list;
		if (np->next) {
			np->next->prev = np;
		}
		vb->node_list = np;
		
	}

	switch_assert(np);

	np->visible = 1;
	vb->visible_nodes++;
	np->parent = vb;

	switch_mutex_unlock(vb->list_mutex);

	return np;
}

static inline void push_to_top(switch_vb_t *vb, switch_vb_node_t *node)
{
	if (node == vb->node_list) {
		vb->node_list = node->next;
	} else if (node->prev) {
		node->prev->next = node->next;
	}
			
	if (node->next) {
		node->next->prev = node->prev;
	}

	node->next = vb->node_list;
	node->prev = NULL;

	if (node->next) {
		node->next->prev = node;
	}

	vb->node_list = node;

	switch_assert(node->next != node);
	switch_assert(node->prev != node);
}

static inline void hide_node(switch_vb_node_t *node, switch_bool_t pop)
{
	switch_vb_t *vb = node->parent;

	switch_mutex_lock(vb->list_mutex);

	if (node->visible) {
		node->visible = 0;
		vb->visible_nodes--;

		if (pop) {
			push_to_top(vb, node);
		}
	}

	switch_core_inthash_delete(vb->node_hash, node->packet.header.seq);

	switch_mutex_unlock(vb->list_mutex);
}

static inline void sort_free_nodes(switch_vb_t *vb)
{
	switch_vb_node_t *np, *this_np;
	int start = 0;

	switch_mutex_lock(vb->list_mutex);
	np = vb->node_list;

	while(np) {
		this_np = np;
		np = np->next;

		if (this_np->visible) {
			start++;
		}

		if (start && !this_np->visible) {
			push_to_top(vb, this_np);
		}		
	}

	switch_mutex_unlock(vb->list_mutex);
}

static inline void hide_nodes(switch_vb_t *vb)
{
	switch_vb_node_t *np;

	switch_mutex_lock(vb->list_mutex);
	for (np = vb->node_list; np; np = np->next) {
		hide_node(np, SWITCH_FALSE);
	}
	switch_mutex_unlock(vb->list_mutex);
}

static inline void drop_ts(switch_vb_t *vb, uint32_t ts)
{
	switch_vb_node_t *np;
	int x = 0;

	switch_mutex_lock(vb->list_mutex);
	for (np = vb->node_list; np; np = np->next) {
		if (!np->visible) continue;

		if (ts == np->packet.header.ts) {
			hide_node(np, SWITCH_FALSE);
			x++;
		}
	}

	if (x) {
		sort_free_nodes(vb);
	}

	switch_mutex_unlock(vb->list_mutex);
	
	if (x) vb->complete_frames--;
}

static inline uint32_t vb_find_lowest_ts(switch_vb_t *vb)
{
	switch_vb_node_t *np, *lowest = NULL;
	
	switch_mutex_lock(vb->list_mutex);
	for (np = vb->node_list; np; np = np->next) {
		if (!np->visible) continue;

		if (!lowest || ntohl(lowest->packet.header.ts) > ntohl(np->packet.header.ts)) {
			lowest = np;
		}
	}
	switch_mutex_unlock(vb->list_mutex);

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





	if (vb->write_init && ((abs(((int)htons(packet->header.seq) - htons(vb->highest_wrote_seq))) > 16) || 
						   (abs((int)((int64_t)ntohl(node->packet.header.ts) - (int64_t)ntohl(vb->highest_wrote_ts))) > 900000))) {
		vb_debug(vb, 2, "%s", "CHANGE DETECTED, PUNT\n");
		switch_vb_reset(vb);
	}
 
	 
	 

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
	
	switch_mutex_lock(vb->list_mutex);
	for (np = vb->node_list; np; np = np->next) {
		if (!np->visible) continue;

		if (!lowest || ntohs(lowest->packet.header.seq) > ntohs(np->packet.header.seq)) {
			lowest = np;
		}
	}
	switch_mutex_unlock(vb->list_mutex);

	return lowest;
}

static inline switch_status_t vb_next_packet(switch_vb_t *vb, switch_vb_node_t **nodep)
{
	switch_vb_node_t *node = NULL;

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
	switch_mutex_lock(vb->list_mutex);
	vb->node_list = NULL;
	switch_mutex_unlock(vb->list_mutex);
}

SWITCH_DECLARE(void) switch_vb_set_flag(switch_vb_t *vb, switch_vb_flag_t flag)
{
	switch_set_flag(vb, flag);
}

SWITCH_DECLARE(void) switch_vb_clear_flag(switch_vb_t *vb, switch_vb_flag_t flag)
{
	switch_clear_flag(vb, flag);
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

SWITCH_DECLARE(void) switch_vb_reset(switch_vb_t *vb)
{

	switch_mutex_lock(vb->mutex);
	switch_core_inthash_destroy(&vb->missing_seq_hash);
	switch_core_inthash_init(&vb->missing_seq_hash);
	switch_mutex_unlock(vb->mutex);

	vb_debug(vb, 2, "%s", "RESET BUFFER\n");


	vb->last_target_seq = 0;
	vb->target_seq = 0;
	vb->write_init = 0;
	vb->highest_wrote_seq = 0;
	vb->highest_wrote_ts = 0;
	vb->next_seq = 0;
	vb->highest_read_ts = 0;
	vb->highest_read_seq = 0;
	vb->complete_frames = 0;
	vb->read_init = 0;
	vb->next_seq = 0;
	vb->complete_frames = 0;

	switch_mutex_lock(vb->mutex);
	hide_nodes(vb);
	switch_mutex_unlock(vb->mutex);
}

SWITCH_DECLARE(switch_status_t) switch_vb_set_frames(switch_vb_t *vb, uint32_t min_frame_len, uint32_t max_frame_len)
{
	switch_mutex_lock(vb->mutex);
	vb->min_frame_len = vb->frame_len = min_frame_len;
	vb->max_frame_len = max_frame_len;
	switch_mutex_unlock(vb->mutex);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_vb_create(switch_vb_t **vbp, uint32_t min_frame_len, uint32_t max_frame_len, switch_memory_pool_t *pool)
{
	switch_vb_t *vb;
	int free_pool = 0;

	if (!pool) {
		switch_core_new_memory_pool(&pool);
		free_pool = 1;
	}

	vb = switch_core_alloc(pool, sizeof(*vb));
	vb->free_pool = free_pool;
	vb->min_frame_len = vb->frame_len = min_frame_len;
	vb->max_frame_len = max_frame_len;
	vb->pool = pool;

	switch_core_inthash_init(&vb->missing_seq_hash);
	switch_core_inthash_init(&vb->node_hash);
	switch_mutex_init(&vb->mutex, SWITCH_MUTEX_NESTED, pool);
	switch_mutex_init(&vb->list_mutex, SWITCH_MUTEX_NESTED, pool);

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

	if (vb->free_pool) {
		switch_core_destroy_memory_pool(&vb->pool);
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(uint32_t) switch_vb_pop_nack(switch_vb_t *vb)
{
	switch_hash_index_t *hi = NULL;
	uint32_t nack = 0;
	uint16_t blp = 0;
	uint16_t least = 0;
	int i = 0;

	void *val;
	const void *var;

	switch_mutex_lock(vb->mutex);

	for (hi = switch_core_hash_first(vb->missing_seq_hash); hi; hi = switch_core_hash_next(&hi)) {
		uint16_t seq;
		const char *token;

		switch_core_hash_this(hi, &var, NULL, &val);
		token = (const char *) val;

		if (token == TOKEN_2) {
			//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "SKIP %u %s\n", ntohs(*((uint16_t *) var)), token);
			continue;
		}
		seq = ntohs(*((uint16_t *) var));
			                                             
		if (!least || seq < least) {
			least = seq;
		}
	}

	if (least && switch_core_inthash_delete(vb->missing_seq_hash, (uint32_t)htons(least))) {
		vb_debug(vb, 3, "Found smallest NACKABLE seq %u\n", least);
		nack = (uint32_t) htons(least);

		switch_core_inthash_insert(vb->missing_seq_hash, nack, (void *) TOKEN_2);

		for(i = 0; i < 16; i++) {
			if (switch_core_inthash_delete(vb->missing_seq_hash, (uint32_t)htons(least + i + 1))) {
				switch_core_inthash_insert(vb->missing_seq_hash, (uint32_t)htons(least + i + 1), (void *) TOKEN_2);
				vb_debug(vb, 3, "Found addtl NACKABLE seq %u\n", least + i + 1);
				blp |= (1 << i);
			}
		}

		blp = htons(blp);
		nack |= (uint32_t) blp << 16;
	}
	
	switch_mutex_unlock(vb->mutex);


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
	int missing = 0;

	switch_mutex_lock(vb->mutex);

	if (!want) want = got;

	if (switch_test_flag(vb, SVB_QUEUE_ONLY)) {
		vb->next_seq = htons(got + 1);
	} else {
		//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "WTF %u\n", got);

		if (switch_core_inthash_delete(vb->missing_seq_hash, (uint32_t)htons(got))) {
			//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "POPPED RESEND %u\n", got);	
			missing = 1;
		} 

		if (!missing || want == got) {
			if (got > want) {
				//vb_debug(vb, 2, "GOT %u WANTED %u; MARK SEQS MISSING %u - %u\n", got, want, want, got - 1);
				//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "XXXXXXXXXXXXXXXXXX   WTF GOT %u WANTED %u; MARK SEQS MISSING %u - %u\n", got, want, want, got - 1);
				for (i = want; i < got; i++) {
					//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "MISSING %u\n", i);
					switch_core_inthash_insert(vb->missing_seq_hash, (uint32_t)htons(i), (void *)TOKEN_1);
				}
			
			}

			if (got >= want || (want - got) > 1000) {
				vb->next_seq = htons(got + 1);
			}
		}
	}

	add_node(vb, packet, len);
	
	switch_mutex_unlock(vb->mutex);

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
	
	switch_mutex_lock(vb->mutex);
	vb_debug(vb, 2, "GET PACKET %u/%u n:%d\n", vb->complete_frames , vb->frame_len, vb->visible_nodes);

	if (vb->complete_frames < vb->frame_len) {
		vb_debug(vb, 2, "BUFFERING %u/%u\n", vb->complete_frames , vb->frame_len);
		switch_goto_status(SWITCH_STATUS_MORE_DATA, end);
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
		switch_vb_reset(vb);

		switch(status) {
		case SWITCH_STATUS_RESTART:
			vb_debug(vb, 2, "%s", "Error encountered ask for new keyframe\n");
			switch_goto_status(SWITCH_STATUS_RESTART, end);
		case SWITCH_STATUS_NOTFOUND:
		default:
			vb_debug(vb, 2, "%s", "No frames found wait for more\n");
			switch_goto_status(SWITCH_STATUS_MORE_DATA, end);
		}
	}
	
	if (node) {
		status = SWITCH_STATUS_SUCCESS;
		
		*packet = node->packet;
		*len = node->len;
		memcpy(packet->body, node->packet.body, node->len);
		hide_node(node, SWITCH_TRUE);

		vb_debug(vb, 1, "GET packet ts:%u seq:%u %s\n", ntohl(packet->header.ts), ntohs(packet->header.seq), packet->header.m ? " <MARK>" : "");

	} else {
		status = SWITCH_STATUS_MORE_DATA;
	}

 end:

	switch_mutex_unlock(vb->mutex);

	return status;

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
