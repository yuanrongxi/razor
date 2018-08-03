/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#include "rate_stat.h"

void rate_stat_init(rate_stat_t* rate, int wnd_size, float scale)
{
	rate->wnd_size = wnd_size;
	rate->scale = scale;

	rate->buckets = calloc(wnd_size, sizeof(rate_bucket_t));
	rate->accumulated_count = 0;
	rate->sample_num = 0;

	rate->oldest_index = 0;
	rate->oldest_ts = -1;
}

void rate_stat_destroy(rate_stat_t* rate)
{
	if (rate == NULL)
		return;

	if (rate->buckets != NULL){
		free(rate->buckets);
		rate->buckets = NULL;
	}
}

void rate_stat_reset(rate_stat_t* rate)
{
	int i;

	rate->accumulated_count = 0;
	rate->sample_num = 0;

	rate->oldest_index = 0;
	rate->oldest_ts = -1;

	for (i = 0; i < rate->wnd_size; ++i){
		rate->buckets[i].sum = 0;
		rate->buckets[i].sample = 0;
	}
}

/*删除过期的统计数据*/
static void rate_stat_erase(rate_stat_t* rate, int64_t now_ts)
{
	int new_oldest_ts;
	rate_bucket_t* bucket;

	if (rate->oldest_ts == -1)
		return;
	
	new_oldest_ts = (int)(now_ts - rate->wnd_size + 1);
	if (new_oldest_ts <= rate->oldest_ts)
		return;

	while (rate->sample_num > 0 && rate->oldest_ts < new_oldest_ts){
		bucket = &rate->buckets[rate->oldest_index];

		rate->sample_num -= bucket->sample;
		rate->accumulated_count -= bucket->sum;
		bucket->sum = 0;
		bucket->sample = 0;

		if (++rate->oldest_index >= rate->wnd_size)
			rate->oldest_index = 0;

		++rate->oldest_ts;
	}

	rate->oldest_ts = new_oldest_ts;
}

void rate_stat_update(rate_stat_t* rate, size_t count, int64_t now_ts)
{
	int ts_offset, index;

	if (rate->oldest_ts > now_ts)
		return; 

	rate_stat_erase(rate, now_ts);

	if (rate->oldest_ts == -1){
		rate->oldest_ts = now_ts;
	}

	ts_offset = (int)(now_ts - rate->oldest_ts);
	index = (rate->oldest_index + ts_offset) % rate->wnd_size;
	
	rate->buckets[index].sum += count;
	rate->buckets[index].sample++;

	rate->sample_num++;
	rate->accumulated_count += count;


}

/*获取统计到的码率*/
int rate_stat_rate(rate_stat_t* rate, int64_t now_ts)
{
	int ret, active_wnd_size;

	rate_stat_erase(rate, now_ts);

	active_wnd_size = (int)(now_ts - rate->oldest_ts + 1);
	if (rate->sample_num == 0 || active_wnd_size <= 1 || (active_wnd_size <= rate->wnd_size))
		return -1;

	ret = (int)(rate->accumulated_count * rate->scale / active_wnd_size + 0.5);

	return ret;
}


