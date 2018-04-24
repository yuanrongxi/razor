/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#include "alr_detector.h"

alr_detector_t* alr_detector_create()
{
	alr_detector_t* alr = calloc(1, sizeof(alr_detector_t));
	alr->alr_started_ts = -1;
	init_interval_budget(&alr->budget, 0, 0);

	return alr;
}

void alr_detector_destroy(alr_detector_t* alr)
{
	if (alr != NULL){
		free(alr);
	}
}

void alr_detector_bytes_sent(alr_detector_t* alr, size_t bytes, int64_t delta_ts)
{
	int percent;
	use_budget(&alr->budget, bytes);
	increase_budget(&alr->budget, (int)delta_ts);

	percent = budget_level_precent(&alr->budget);
	/*进行alr started ts判定*/
	if (percent > k_alr_start_buget_percent && alr->alr_started_ts == -1){ /*发送budget还有很多空闲*/
		alr->alr_started_ts = GET_SYS_MS();
	}
	else if (percent < k_alr_stop_buget_percent){
		alr->alr_started_ts = -1;
	}
}

void alr_detector_set_bitrate(alr_detector_t* alr, int bitrate_bps)
{
	int target_bitrate_kbps;

	target_bitrate_kbps = bitrate_bps * k_alr_banwidth_useage_percent / (1000 * 100);
	set_target_rate_kbps(&alr->budget, target_bitrate_kbps);
}

int64_t alr_get_app_limited_started_ts(alr_detector_t* alr)
{
	return alr->alr_started_ts;
}


