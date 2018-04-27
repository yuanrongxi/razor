/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#ifndef __remote_bitrate_estimator_h_
#define __remote_bitrate_estimator_h_

#include "aimd_rate_control.h"
#include "inter_arrival.h"
#include "overuse_detector.h"
#include "kalman_filter.h"
#include "cf_stream.h"
#include "rate_stat.h"

/*接收端利用卡尔曼算法和aimd来评估发送端带宽，然后通过REMB方式反馈给发送端进行带宽调节,这里只是做一个可选测试*/
typedef struct
{
	int64_t						last_update_ts;
	int64_t						interval_ts;
	int64_t						last_packet_ts;

	int							last_incoming_bitrate;
	rate_stat_t					incoming_bitrate;
	

	aimd_rate_controller_t*		aimd;
	overuse_detector_t*			detector;
	inter_arrival_t*			inter_arrival;
	kalman_filter_t*			kalman;
}remote_bitrate_estimator_t;

remote_bitrate_estimator_t*		rbe_create();
void							rbe_destroy(remote_bitrate_estimator_t* est);

void							rbe_update_rtt(remote_bitrate_estimator_t* est, uint32_t rtt);
void							rbe_set_min_bitrate(remote_bitrate_estimator_t* est, uint32_t min_bitrate);
void							rbe_set_max_bitrate(remote_bitrate_estimator_t* est, uint32_t max_bitrate);

int								rbe_last_estimate(remote_bitrate_estimator_t* est, uint32_t* birate_bps);

void							rbe_incoming_packet(remote_bitrate_estimator_t* est, uint32_t timestamp, int64_t arrival_ts, size_t payload_size, int64_t now_ts);

int								rbe_heartbeat(remote_bitrate_estimator_t* est, int64_t now_ts, uint32_t* remb);


#endif



