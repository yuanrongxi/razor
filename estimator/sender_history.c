/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#include "sender_history.h"

static void free_packet_feedback(skiplist_item_t key, skiplist_item_t val, void* args)
{
	packet_feedback_t* packet = val.ptr;
	if (packet != NULL)
		free(packet);
}

sender_history_t* sender_history_create(uint32_t limited_ms)
{
	sender_history_t* hist = calloc(1, sizeof(sender_history_t));
	
	hist->limited_ms = limited_ms;
	hist->l = skiplist_create(id64_compare, free_packet_feedback, NULL);
	init_unwrapper16(&hist->wrapper);

	return hist;
}

void sender_history_destroy(sender_history_t* hist)
{
	if (hist == NULL)
		return;

	if (hist->l != NULL){
		skiplist_destroy(hist->l);
		hist->l = NULL;
	}

	free(hist);
}

void sender_history_add(sender_history_t* hist, packet_feedback_t* packet)
{
	int64_t now_ts = GET_SYS_MS();
	packet_feedback_t* p;
	skiplist_iter_t* it;
	skiplist_item_t key, val;

	/*去除过期的发送记录*/
	while (skiplist_size(hist->l) > 0){
		it = skiplist_first(hist->l);
		p = it->val.ptr;
		if (p->create_ts + hist->limited_ms < now_ts)
			skiplist_remove(hist->l, it->key);
		else
			break;
	}

	p = malloc(sizeof(packet_feedback_t));
	*p = *packet;

	key.i64 = wrap_uint16(&hist->wrapper, packet->sequence_number);
	val.ptr = p;
	skiplist_insert(hist->l, key, val);
}

int sender_history_get(sender_history_t* hist, uint16_t seq, packet_feedback_t* packet)
{
	skiplist_iter_t* it;
	skiplist_item_t key;
	packet_feedback_t* p;

	key.i64 = wrap_uint16(&hist->wrapper, seq);

	hist->last_ack_seq_num = SU_MAX(key.i64, hist->last_ack_seq_num);

	it = skiplist_search(hist->l, key);
	if (it == NULL)
		return -1;

	p = it->val.ptr;
	*packet = *p;

	skiplist_remove(hist->l, key);

	return 0;
}

size_t sender_history_outstanding_bytes(sender_history_t* hist)
{
	skiplist_iter_t* it;
	packet_feedback_t* p;
	size_t ret;

	ret = 0;
	SKIPLIST_FOREACH(hist->l, it){
		p = it->val.ptr;
		if (it->key.i64 > hist->last_ack_seq_num)
			ret += p->payload_size;
	}

	return ret;
}
