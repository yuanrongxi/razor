/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#include "remote_bitrate_estimator.h"

#define k_min_bitrate					64000	  /*8KB/S*/
#define k_max_bitrate					12800000  /*1.6MB/S*/
#define k_process_interval_ms			500
#define k_max_update_timeout			2000

#define k_rate_window_size				1000
#define k_rate_scale					8000

static void rbe_update_estimate(remote_bitrate_estimator_t* est, int64_t now_ts);

remote_bitrate_estimator_t* rbe_create()
{
	remote_bitrate_estimator_t* rbe = calloc(1, sizeof(remote_bitrate_estimator_t));
	rbe->interval_ts = k_process_interval_ms;
	rbe->last_update_ts = GET_SYS_MS();
	rbe->last_packet_ts = -1;

	/*初始化带宽统计器*/
	rbe->last_incoming_bitrate = -1;
	rate_stat_init(&rbe->incoming_bitrate, k_rate_window_size, k_rate_scale);

	rbe->aimd = aimd_create(k_min_bitrate, k_max_bitrate);
	rbe->detector = overuse_create();
	rbe->inter_arrival = create_inter_arrival(0, 5);
	rbe->kalman = kalman_filter_create();

	return rbe;
}

void rbe_destroy(remote_bitrate_estimator_t* est)
{
	if (est == NULL)
		return;

	rate_stat_destroy(&est->incoming_bitrate);

	if (est->aimd != NULL){
		aimd_destroy(est->aimd);
		est->aimd = NULL;
	}

	if (est->detector != NULL){
		overuse_destroy(est->detector);
		est->detector = NULL;
	}

	if (est->inter_arrival != NULL){
		destroy_inter_arrival(est->inter_arrival);
		est->inter_arrival = NULL;
	}

	if (est->kalman != NULL){
		kalman_filter_destroy(est->kalman);
		est->kalman = NULL;
	}

	free(est);
}

/*重置过载评估评估器*/
static void rbe_reset(remote_bitrate_estimator_t* est)
{
	if (est->detector != NULL){
		overuse_destroy(est->detector);
		est->detector = overuse_create();
	}

	if (est->inter_arrival != NULL){
		destroy_inter_arrival(est->inter_arrival);
		est->inter_arrival = create_inter_arrival(0, 5);
	}

	if (est->kalman != NULL){
		kalman_filter_destroy(est->kalman);
		est->kalman = kalman_filter_create();
	}
}

void rbe_update_rtt(remote_bitrate_estimator_t* est, uint32_t rtt)
{
	aimd_set_rtt(est->aimd, rtt);
}

void rbe_set_min_bitrate(remote_bitrate_estimator_t* est, uint32_t min_bitrate)
{
	aimd_set_min_bitrate(est->aimd, min_bitrate);
}

void rbe_set_max_bitrate(remote_bitrate_estimator_t* est, uint32_t max_bitrate)
{
	aimd_set_max_bitrate(est->aimd, max_bitrate);
}

int rbe_last_estimate(remote_bitrate_estimator_t* est, uint32_t* bitrate_bps)
{
	if (est->aimd->inited == -1)
		return -1;

	*bitrate_bps = est->aimd->curr_rate;

	return 0;
}

int rbe_heartbeat(remote_bitrate_estimator_t* est, int64_t now_ts, uint32_t* remb)
{
	uint32_t bitrate;
	*remb = 0;

	if (now_ts >= est->last_update_ts + est->interval_ts){
		est->last_update_ts = now_ts;

		if (est->last_packet_ts > 0 && est->last_packet_ts + k_max_update_timeout > now_ts)
			rbe_update_estimate(est, now_ts);

		/*进行REMB的发送*/
		if (rbe_last_estimate(est, &bitrate) == 0){
			*remb = bitrate;
			return 0;
		}
	}

	return -1;
}

void rbe_incoming_packet(remote_bitrate_estimator_t* est, uint32_t timestamp, int64_t arrival_ts, size_t payload_size, int64_t now_ts)
{
	int64_t delta_arrival;
	uint32_t delta_ts;

	int incoming_rate, prev_state, delta_size;

	/*就算当前接收到数据的带宽码率,当前接收到的数据码率需要作为输入参数输入到aimd进行带宽调整*/
	incoming_rate = rate_stat_rate(&est->incoming_bitrate, now_ts);
	if (incoming_rate >= 0){
		est->last_incoming_bitrate = incoming_rate;
	}
	else if (est->last_incoming_bitrate > 0){
		rate_stat_reset(&est->incoming_bitrate);
		est->last_incoming_bitrate = 0;
	}
	rate_stat_update(&est->incoming_bitrate, payload_size, now_ts);

	/*更新接收时间*/
	est->last_packet_ts = now_ts;

	prev_state = est->detector->state;
	delta_arrival = 0;
	delta_ts = 0;
	delta_size = 0;

	/*进行kalman filter判断网络是否过载*/
	if (inter_arrival_compute_deltas(est->inter_arrival, timestamp, arrival_ts, now_ts, payload_size, &delta_ts, &delta_arrival, &delta_size) == 0){
		kalman_filter_update(est->kalman, delta_arrival, delta_ts, delta_size, est->detector->state, now_ts);
		overuse_detect(est->detector, est->kalman->offset, delta_ts, est->kalman->num_of_deltas, now_ts);
	}

	if (est->detector->state == kBwOverusing){
		incoming_rate = rate_stat_rate(&est->incoming_bitrate, now_ts);
		/*网络的过载状态发生变化了，必须立即下调码率*/
		if (incoming_rate > 0 && (prev_state != kBwOverusing || aimd_time_reduce_further(est->aimd, now_ts, incoming_rate) == 0))
			rbe_update_estimate(est, now_ts);
	}

	est->last_packet_ts = now_ts;
}

static void rbe_update_estimate(remote_bitrate_estimator_t* est, int64_t now_ts)
{
	int state;
	double sum_var_noise = 0.0;
	rate_control_input_t input;
	uint32_t target_bitrate;

	state = kBwNormal;
	if (now_ts > est->last_packet_ts + k_max_update_timeout) /*太长时间没收发送方的报文，可以认为是间歇性断开，并进行重置*/
		rbe_reset(est);
	else{
		sum_var_noise = est->kalman->var_noise;
		state = est->detector->state;
	}

	/*进行aimd bitrate调节*/
	input.noise_var = sum_var_noise;
	input.incoming_bitrate = rate_stat_rate(&est->incoming_bitrate, now_ts);
	input.state = state;
	target_bitrate = aimd_update(est->aimd, &input, now_ts);
	if (est->aimd->inited == 0)
		est->interval_ts = aimd_get_feelback_interval(est->aimd);
}

