/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#include "cc_loss_stat.h"
#include "razor_log.h"

#define k_loss_statistics_window_ms 500

void loss_statistics_init(cc_loss_statistics_t* loss_stat)
{
	loss_stat->count = 0;
	loss_stat->max_id = -1;
	loss_stat->prev_max_id = -1;
	loss_stat->stat_ts = -1;
	
	init_unwrapper16(&loss_stat->wrapper);
}

void loss_statistics_destroy(cc_loss_statistics_t* loss_stat)
{

}

int loss_statistics_calculate(cc_loss_statistics_t* loss_stat, int64_t now_ts, uint8_t* fraction_loss, int* num)
{
	int disance;
	
	*fraction_loss = 0;
	*num = 0;

	if (loss_stat->max_id == -1 || loss_stat->prev_max_id == -1 
		|| (now_ts < loss_stat->stat_ts + k_loss_statistics_window_ms || loss_stat->max_id < loss_stat->prev_max_id + 20))
		return -1;

	disance = loss_stat->max_id - loss_stat->prev_max_id;
	if (disance <= 0)
		return -1;
	
	if (disance <= loss_stat->count)
		*fraction_loss = 0;
	else{
		*fraction_loss = (disance - loss_stat->count) * 255 / disance;
		if (*fraction_loss > 12) /*%5ртио*/
			razor_info("loss!! fraction = %u\n", *fraction_loss);
	}
	*num = disance;

	loss_stat->prev_max_id = loss_stat->max_id;
	loss_stat->count = 0;
	loss_stat->stat_ts = now_ts;

	return 0;
}

void loss_statistics_incoming(cc_loss_statistics_t* loss_stat, uint16_t seq)
{
	int64_t id;
	id = wrap_uint16(&loss_stat->wrapper, seq);

	if (loss_stat->prev_max_id == -1 && id > 0)
		loss_stat->prev_max_id = id - 1;

	if (loss_stat->max_id < id)
		loss_stat->max_id = id;

	++loss_stat->count;
}



