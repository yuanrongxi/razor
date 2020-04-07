/*-
* Copyright (c) 2017-2019 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#include "sim_internal.h"
#include "flex_fec_receiver.h"
#include <assert.h>

static void sim_receiver_fec_free_segment(sim_segment_t* seg, void* args)
{
	if (seg != NULL)
		free(seg);
}

static void sim_receiver_fec_free_fec(sim_fec_t* fec, void* args)
{
	if (fec != NULL)
		free(fec);
}

static void sim_receiver_fec_free_packet(skiplist_item_t key, skiplist_item_t val, void* args)
{
	sim_receiver_fec_free_segment(val.ptr, args);
}

static void sim_receiver_free_flex(skiplist_item_t key, skiplist_item_t val, void* args)
{
	if (val.ptr != NULL)
		flex_fec_receiver_desotry(val.ptr);
}

sim_receiver_fec_t* sim_fec_create(sim_session_t* s)
{
	sim_receiver_fec_t* f;

	f = calloc(1, sizeof(sim_receiver_fec_t));
	f->flexes = skiplist_create(idu16_compare, sim_receiver_free_flex, NULL);
	f->segs_cache = skiplist_create(idu32_compare, sim_receiver_fec_free_packet, NULL);
	f->recover_packets = skiplist_create(idu32_compare, sim_receiver_fec_free_packet, NULL);
	f->out = create_list();

	f->evict_ts = GET_SYS_MS();

	return f;
}

void sim_fec_destroy(sim_session_t* s, sim_receiver_fec_t* f)
{
	if (f == NULL)
		return;

	if (f->out != NULL){
		destroy_list(f->out);
		f->out = NULL;
	}

	if (f->recover_packets != NULL){
		skiplist_destroy(f->recover_packets);
		f->recover_packets = NULL;
	}
	
	if (f->flexes != NULL){
		skiplist_destroy(f->flexes);
		f->flexes = NULL;
	}

	if (f->segs_cache != NULL){
		skiplist_destroy(f->segs_cache);
		f->segs_cache = NULL;
	}

	free(f);
}

void sim_fec_reset(sim_session_t* s, sim_receiver_fec_t* f)
{
	f->base_id = 0;
	f->max_ts = 0;

	skiplist_clear(f->segs_cache);
	skiplist_clear(f->recover_packets);
	skiplist_clear(f->flexes);
	list_clear(f->out);
}

void sim_fec_active(sim_session_t* s, sim_receiver_fec_t* f)
{
}

static void sim_fec_packet_add_recover(sim_session_t* s, sim_receiver_fec_t *f, sim_segment_t* seg)
{
	skiplist_item_t key, val;

	if (seg == NULL)
		return;

	key.u32 = seg->packet_id;
	if (skiplist_search(f->recover_packets, key) == NULL){
		key.u32 = seg->packet_id;
		val.ptr = seg;
		skiplist_insert(f->recover_packets, key, val);
	}
	else
		free(seg);
}

static void sim_fec_add_segment_to_flex(sim_session_t* s, sim_receiver_fec_t* f, flex_fec_receiver_t* flex, sim_fec_t* fec)
{
	uint32_t i;
	skiplist_iter_t* iter;
	skiplist_item_t key;

	for (i = 0; i < fec->count; i++){
		key.u32 = fec->base_id + i;
		iter = skiplist_search(f->segs_cache, key);
		if (iter != NULL){
			list_clear(f->out);
			flex_fec_receiver_on_segment(flex, iter->val.ptr, f->out);
			assert(list_size(f->out) == 0);
			while (list_size(f->out) > 0)
				sim_fec_packet_add_recover(s, f, list_pop(f->out));
		}
	}
}

void sim_fec_put_fec_packet(sim_session_t* s, sim_receiver_fec_t* f, sim_fec_t* fec)
{
	skiplist_iter_t* iter;
	skiplist_item_t key, val;
	flex_fec_receiver_t* flex;

	if (fec->base_id + fec->count < f->base_id + 1){
		sim_receiver_fec_free_fec(fec, NULL);
		return;
	}

	key.u16 = fec->fec_id;
	iter = skiplist_search(f->flexes, key);
	if (iter == NULL){
		flex = flex_fec_receiver_create(NULL, sim_receiver_fec_free_fec, NULL);
		val.ptr = flex;
		skiplist_insert(f->flexes, key, val);

		flex->fec_ts = GET_SYS_MS();
		flex_fec_receiver_active(flex, fec->fec_id, fec->col, fec->row, fec->base_id, fec->count);

		/*将已经在segs中的segments放入FLEX中*/
		sim_fec_add_segment_to_flex(s, f, flex, fec);
	}
	else
		flex = iter->val.ptr;

	sim_fec_packet_add_recover(s, f, flex_fec_receiver_on_fec(flex, fec));
}

void sim_fec_put_segment(sim_session_t* s, sim_receiver_fec_t* f, sim_segment_t* seg)
{
	sim_segment_t* in_seg;
	skiplist_iter_t* iter;
	skiplist_item_t key, val;

	if (seg->packet_id <= f->base_id)
		return;

	key.u32 = seg->packet_id;
	if (skiplist_search(f->segs_cache, key) != NULL)
		return;

	in_seg = malloc(sizeof(sim_segment_t));
	*in_seg = *seg;
	val.ptr = in_seg;
	skiplist_insert(f->segs_cache, key, val);

	key.u16 = seg->fec_id;
	iter = skiplist_search(f->flexes, key);
	if (iter == NULL || iter->val.ptr == NULL)
		return;

	list_clear(f->out);
	flex_fec_receiver_on_segment(iter->val.ptr, in_seg, f->out);
	while (list_size(f->out) > 0)
		sim_fec_packet_add_recover(s, f, list_pop(f->out));
}

#define EVICT_FEC_DELAY 3000
void sim_fec_evict(sim_session_t* s, sim_receiver_fec_t* f, int64_t now_ts)
{
	flex_fec_receiver_t* flex;
	sim_segment_t* seg;

	skiplist_iter_t* iter;

	if (f->evict_ts + 300 > now_ts)
		return;

	f->evict_ts = now_ts;

	while (skiplist_size(f->flexes) > 0){
		iter = skiplist_first(f->flexes);
		flex = iter->val.ptr;
		if (flex->fec_ts + EVICT_FEC_DELAY <= now_ts){
			if (flex->base_id + flex->count > f->base_id + 1)
				f->base_id = flex->base_id + flex->count - 1;

			skiplist_remove(f->flexes, iter->key);
		}
		else
			break;
	}

	while (skiplist_size(f->segs_cache) > 0){
		iter = skiplist_first(f->segs_cache);
		seg = iter->val.ptr;
		if (seg->packet_id <= f->base_id)
			skiplist_remove(f->segs_cache, iter->key);
		else
			break;
	}
}


