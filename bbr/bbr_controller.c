/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#include "bbr_controller.h"

#ifndef false
#define false 0
#endif

#ifndef true
#define true 1
#endif

static inline void bbr_set_default_config(bbr_config_t* config)
{
	config->probe_bw_pacing_gain_offset = 0.25;
	config->encoder_rate_gain = 1;
	config->encoder_rate_gain_in_probe_rtt = 1;
	config->exit_startup_rtt_threshold_ms = 0;
	config->probe_rtt_congestion_window_gain = 0.75;
	config->exit_startup_on_loss = true;
	config->num_startup_rtts = 3;
	config->rate_based_recovery = false;
	config->max_aggregation_bytes_multiplier = 0;
	config->slower_startup = false;
	config->rate_based_startup = false;
	config->fully_drain_queue = false;
	config->initial_conservation_in_startup = CONSERVATION;
	config->max_ack_height_window_multiplier = 1;
	config->probe_rtt_based_on_bdp = false;
	config->probe_rtt_skipped_if_similar_rtt = false;
	config->probe_rtt_disabled_if_app_limited = false;
}

bbr_controller_t* bbr_create()
{
	bbr_controller_t* bbr = calloc(1, sizeof(bbr_controller_t));

	/*初始化RTT统计模块*/
	bbr_rtt_init(&bbr->rtt_stat);
	/*初始化BBR默认配置参数*/
	bbr_set_default_config(&bbr->config);

	/*创建windows filter*/
	wnd_filter_init(&bbr->max_bandwidth, kBandwidthWindowSize, max_val_func);
	wnd_filter_init(&bbr->max_ack_height, kBandwidthWindowSize, max_val_func);

	return bbr;
}





