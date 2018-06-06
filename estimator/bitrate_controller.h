/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#ifndef __bitrate_controller_h_
#define __bitrate_controller_h_

#include "sender_bandwidth_estimator.h"
#include "razor_api.h"

/*发送端码率控制器，会根据REMB/base delay和网络反馈报文调节和控制发送带宽*/
typedef struct
{
	int64_t					last_bitrate_update_ts;
	int64_t					notify_ts;					/*定时通告上层*/

	uint32_t				reserved_bitrate_bps;
	
	uint32_t				last_bitrate_bps;
	uint8_t					last_fraction_loss;
	uint32_t				last_rtt;
	uint32_t				last_reserved_bitrate_bps;
	sender_estimation_t*	est;

	void*					trigger;
	bitrate_changed_func	trigger_func;

}bitrate_controller_t;

bitrate_controller_t*	bitrate_controller_create(void* trigger, bitrate_changed_func func);
void					bitrate_controller_destroy(bitrate_controller_t* ctrl);

/*获取可用码率*/
uint32_t				bitrate_controller_available_bandwidth(bitrate_controller_t* ctrl);

/*设置起始码率*/
void					bitrate_controller_set_start_bitrate(bitrate_controller_t* ctrl, uint32_t start_bitrate);
void					bitrate_controller_set_conf_bitrate(bitrate_controller_t* ctrl, uint32_t min_bitrate, uint32_t max_bitrate);
void					bitrate_controller_set_bitrates(bitrate_controller_t* ctrl, uint32_t bitrate, uint32_t min_bitrate, uint32_t max_bitrate);
void					bitrate_controller_reset_bitrates(bitrate_controller_t* ctrl, uint32_t bitrate, uint32_t min_bitrate, uint32_t max_bitrate);

void					bitrate_controller_set_reserved(bitrate_controller_t* ctrl, uint32_t reserved);

int						bitrate_controller_get_parameter(bitrate_controller_t* ctrl, uint32_t* bitrate, uint8_t* fraction_loss, uint32_t* rtt);

void					bitrate_controller_on_basedelay_result(bitrate_controller_t* ctrl, int update, int probe, uint32_t target_bitrate, int state);

void					bitrate_controller_heartbeat(bitrate_controller_t* ctrl, int64_t cur_ts, uint32_t acked_bitrate);

void					bitrate_controller_on_report(bitrate_controller_t* ctrl, uint32_t rtt, int64_t cur_ts, uint8_t fraction_loss, int packets_num, uint32_t acked_bitrate);
void					bitrate_controller_on_remb(bitrate_controller_t* ctrl, uint32_t bitrate);

#endif



