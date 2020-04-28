/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#include "cc_loss_stat.h"
#include "razor_log.h"

#define k_loss_statistics_window_ms 4000
#define k_max_statistics_window_number 200

void loss_statistics_init(cc_loss_statistics_t* loss_stat)
{
	loss_stat->max_id = -1;
	loss_stat->stat_ts = -1;

	init_unwrapper16(&loss_stat->wrapper);

	loss_stat->list = skiplist_create(id64_compare, NULL, NULL);
}

void loss_statistics_destroy(cc_loss_statistics_t* loss_stat)
{
	if (loss_stat->list){
		skiplist_destroy(loss_stat->list);
		loss_stat->list = NULL;
	}
}

static void loss_evict_oldest(cc_loss_statistics_t* loss_stat, int64_t now_ts)
{
	skiplist_iter_t* iter;

	if (skiplist_size(loss_stat->list) > 0){
		iter = skiplist_first(loss_stat->list);
		while (skiplist_size(loss_stat->list) > k_max_statistics_window_number 
			|| (iter != NULL && iter->val.i64 + k_loss_statistics_window_ms < now_ts)){
			skiplist_remove(loss_stat->list, iter->key);
			iter = skiplist_first(loss_stat->list);
		}
	}
}

int loss_statistics_calculate(cc_loss_statistics_t* loss_stat, int64_t now_ts, uint8_t* fraction_loss, int* num)
{
	skiplist_iter_t* iter;
	int disance, count;
	int64_t oldest;
	
	*fraction_loss = 0;
	*num = 0;

	loss_evict_oldest(loss_stat, now_ts);

	if (loss_stat->max_id == -1 || skiplist_size(loss_stat->list) < 100)
		return -1;

	iter = skiplist_first(loss_stat->list);
	if (iter == NULL)
		return -1;

	oldest = iter->key.i64;

	disance = (int32_t)(loss_stat->max_id - oldest + 1);
	if (disance <= 0)
		return -1;
	
	count = skiplist_size(loss_stat->list);
	if (disance <= count)
		*fraction_loss = 0;
	else{
		*fraction_loss = (disance - count) * 255 / disance;
	}
	*num = disance;

	loss_stat->stat_ts = now_ts;

	return 0;
}

void loss_statistics_incoming(cc_loss_statistics_t* loss_stat, uint16_t seq, int64_t now_ts)
{
	skiplist_item_t key, val;
	int64_t id;
	id = wrap_uint16(&loss_stat->wrapper, seq);

	if (loss_stat->max_id < id)
		loss_stat->max_id = id;

	key.i64 = id;
	val.i64 = now_ts;

	skiplist_insert(loss_stat->list, key, val);

	loss_evict_oldest(loss_stat, val.i64);
}



