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
	packet_feedback_t* p;
	skiplist_iter_t* it;
	skiplist_item_t key, val;

	/*去除过期的发送记录*/
	while (skiplist_size(hist->l) > 0){
		it = skiplist_first(hist->l);
		p = it->val.ptr;
		if (p->create_ts + hist->limited_ms < packet->send_ts){
			if (it->key.i64 > hist->last_ack_seq_num){
				hist->outstanding_bytes = SU_MAX(hist->outstanding_bytes - p->payload_size, 0);
				hist->last_ack_seq_num = it->key.i64;
			}

			skiplist_remove(hist->l, it->key);
		}
		else
			break;
	}

	p = malloc(sizeof(packet_feedback_t));
	*p = *packet;

	key.i64 = wrap_uint16(&hist->wrapper, packet->sequence_number);
	val.ptr = p;
	skiplist_insert(hist->l, key, val);

	hist->outstanding_bytes += packet->payload_size;

	if (hist->last_ack_seq_num == 0)
		hist->last_ack_seq_num = SU_MAX(hist->last_ack_seq_num, packet->sequence_number - 1);
}

int sender_history_get(sender_history_t* hist, uint16_t seq, packet_feedback_t* packet, int remove_flag)
{
	skiplist_iter_t* it;
	skiplist_item_t key;
	packet_feedback_t* p;

	int64_t i, seqnumber;

	seqnumber = wrap_uint16(&hist->wrapper, seq);
	for (i = hist->last_ack_seq_num + 1; i <= seqnumber; i++){
		key.i64 = i;
		it = skiplist_search(hist->l, key);
		if (it != NULL){
			p = it->val.ptr;
			hist->outstanding_bytes = SU_MAX(hist->outstanding_bytes - p->payload_size, 0);
		}
	}

	hist->last_ack_seq_num = SU_MAX(seqnumber, hist->last_ack_seq_num);

	key.i64 = seqnumber;
	it = skiplist_search(hist->l, key);
	if (it == NULL)
		return -1;

	p = it->val.ptr;
	*packet = *p;

	if (remove_flag == 1)
		skiplist_remove(hist->l, key);

	return 0;
}

size_t sender_history_outstanding_bytes(sender_history_t* hist)
{
	return hist->outstanding_bytes;
}
