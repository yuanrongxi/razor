/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#include "bbr_controller.h"
#include "razor_log.h"

#ifndef false
#define false 0
#endif

#ifndef true
#define true 1
#endif

/////////////////////////////////////////////////////////////////////////////////////
static void bbr_enter_startup_mode(bbr_controller_t* bbr);
static void bbr_enter_probe_bandwidth_mode(bbr_controller_t* bbr, int64_t now_ts);
static int32_t bbr_get_congestion_window(bbr_controller_t* bbr);
static int64_t bbr_get_min_rtt(bbr_controller_t* bbr);
static double bbr_get_pacing_gain(bbr_controller_t* bbr, int index);
static int32_t bbr_pacing_rate(bbr_controller_t* bbr);
static int32_t bbr_bandwidth_estimate(bbr_controller_t* bbr);
static int bbr_is_probing_for_more_bandwidth(bbr_controller_t* bbr);

static int bbr_update_round_trip_counter(bbr_controller_t* bbr, int64_t last_acked_packet);
static int bbr_update_bandwidth_and_min_rtt(bbr_controller_t* bbr, int64_t now_ts, bbr_packet_info_t packets[], int size);
static void bbr_discard_lost_packets(bbr_controller_t* bbr, bbr_packet_info_t packets[], int size);

static void bbr_update_gain_cycle_phase(bbr_controller_t* bbr, int64_t now_ts, size_t prior_in_flight, int losses);
static void bbr_check_if_full_bandwidth_reached(bbr_controller_t* bbr);

static void bbr_maybe_exit_startup_or_drain(bbr_controller_t* bbr, bbr_feedback_t* feedback);
static void bbr_maybe_enter_or_exit_probe_rtt(bbr_controller_t* bbr, int is_round_start, int min_rtt_expired);

static void bbr_update_recovery_state(bbr_controller_t* bbr, int64_t last_acked_packet, int losses, int is_round_start);
static void bbr_update_ack_aggregation_bytes(bbr_controller_t* bbr, int64_t ack_time, size_t newly_acked_bytes);

static void bbr_calculate_pacing_rate(bbr_controller_t* bbr);
static void bbr_calculate_congestion_window(bbr_controller_t* bbr, size_t bytes_acked);
static void bbr_calculate_recovery_window(bbr_controller_t* bbr, size_t bytes_acked, size_t bytes_lost, size_t bytes_in_flight);

/////////////////////////////////////////////////////////////////////////////////////

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

	bbr->sampler = sampler_create();

	/*创建windows filter*/
	wnd_filter_init(&bbr->max_bandwidth, kBandwidthWindowSize, max_val_func);
	wnd_filter_init(&bbr->max_ack_height, kBandwidthWindowSize, max_val_func);
	bbr->default_bandwidth = kInitialBandwidthKbps;

	bbr->aggregation_epoch_start_time = -1;
	bbr->aggregation_epoch_bytes = 0;
	bbr->bytes_acked_since_queue_drained = 0;
	bbr->max_aggregation_bytes_multiplier = 0;

	bbr->min_rtt = 0;
	bbr->last_rtt = 0;

	bbr->min_rtt_timestamp = 0;

	/*初始化拥塞窗口*/
	bbr->congestion_window = kInitialCongestionWindowPackets * kDefaultTCPMSS;
	bbr->initial_congestion_window = kInitialCongestionWindowPackets * kDefaultTCPMSS;
	bbr->max_congestion_window = kDefaultMaxCongestionWindowPackets * kDefaultTCPMSS;
	bbr->min_congestion_window = kDefaultMinCongestionWindowPackets * kDefaultTCPMSS;

	bbr->pacing_gain = 1;
	bbr->pacing_rate = 0;

	bbr->congestion_window_gain_constant = kProbeBWCongestionWindowGain;
	bbr->rtt_variance_weight = kBbrRttVariationWeight;
	bbr->cycle_current_offset = 0;
	bbr->last_cycle_start = 0;
	bbr->is_at_full_bandwidth = false;
	bbr->rounds_without_bandwidth_gain = 0;
	bbr->bandwidth_at_last_round = 0;
	bbr->exiting_quiescence = false;
	bbr->exit_probe_rtt_at = -1;

	bbr->probe_rtt_round_passed = false;
	bbr->last_sample_is_app_limited = false;
	bbr->recovery_state = NOT_IN_RECOVERY;

	bbr->end_recovery_at = -1;
	bbr->recovery_window = bbr->max_congestion_window;
	bbr->app_limited_since_last_probe_rtt = false;
	bbr->min_rtt_since_last_probe_rtt = -1;

	/*初始化BBR默认配置参数*/
	bbr_set_default_config(&bbr->config);

	razor_info("create bbr controller\n");

	bbr->constraints.at_time = -1;
	bbr->constraints.max_rate = -1;
	bbr->constraints.min_rate = -1;

	bbr_reset(bbr);

	return bbr;
}

void bbr_destroy(bbr_controller_t* bbr)
{
	if (bbr == NULL)
		return;

	if (bbr->sampler != NULL){
		sampler_destroy(bbr->sampler);
		bbr->sampler = NULL;
	}

	free(bbr);
}

void bbr_reset(bbr_controller_t* bbr)
{
	bbr->round_trip_count = 0;
	bbr->rounds_without_bandwidth_gain = 0;
	if (bbr->config.num_startup_rtts > 0){
		bbr->is_at_full_bandwidth = false;
		bbr_enter_startup_mode(bbr);
	}
	else{
		bbr->is_at_full_bandwidth = true;
		bbr_enter_probe_bandwidth_mode(bbr, bbr->constraints.at_time);
	}
}

static bbr_network_ctrl_update_t bbr_create_rate_upate(bbr_controller_t* bbr, int64_t at_time)
{
	int32_t bandwidth, target_rate, pacing_rate;
	int64_t rtt;
	bbr_network_ctrl_update_t ret;

	ret.congestion_window = -1;
	if (at_time == -1)
		return ret;

	bandwidth = bbr_bandwidth_estimate(bbr);
	if (bandwidth <= 0)
		bandwidth = bbr->default_bandwidth;

	rtt = bbr_get_min_rtt(bbr);

	/*确定pacing rate和target rate*/
	pacing_rate = bbr_pacing_rate(bbr);
	target_rate = bandwidth;
	if (bbr->mode == PROBE_RTT)
		target_rate = bandwidth * bbr->config.encoder_rate_gain_in_probe_rtt;
	else
		target_rate = bandwidth * bbr->config.encoder_rate_gain;

	target_rate = SU_MIN(pacing_rate, target_rate);
	if (bbr->constraints.at_time > 0){
		if (bbr->constraints.max_rate > 0){
			target_rate = SU_MIN(target_rate, bbr->constraints.max_rate);
			pacing_rate = SU_MIN(pacing_rate, bbr->constraints.max_rate);
		}
		if (bbr->constraints.min_rate > 0){
			target_rate = SU_MAX(target_rate, bbr->constraints.min_rate);
			pacing_rate = SU_MAX(pacing_rate, bbr->constraints.min_rate);
		}
	}

	/*返回拥塞控制窗口*/
	ret.congestion_window = bbr_get_congestion_window(bbr);

	/*返回target_rate信息*/
	ret.target_rate.at_time = at_time;
	ret.target_rate.bandwidth = bandwidth;
	ret.target_rate.rtt = rtt;
	ret.target_rate.loss_rate_ratio = 0;
	ret.target_rate.bwe_period = rtt * kGainCycleLength;
	ret.target_rate.target_rate = target_rate;

	/*返回pacer信息*/
	ret.pacer_config.at_time = at_time;
	ret.pacer_config.time_window = rtt / 4;
	ret.pacer_config.data_window = ret.pacer_config.time_window * pacing_rate;
	if (bbr_is_probing_for_more_bandwidth(bbr) == 1)
		ret.pacer_config.pad_window = ret.pacer_config.data_window;
	else
		ret.pacer_config.pad_window = 0;

	return ret;
}

bbr_network_ctrl_update_t bbr_on_network_availability(bbr_controller_t* bbr, bbr_network_availability_t* av)
{
	bbr_reset(bbr);
	bbr_rtt_connection_migration(&bbr->rtt_stat);

	return bbr_create_rate_upate(bbr, av->at_time);
}

bbr_network_ctrl_update_t bbr_on_newwork_router_change(bbr_controller_t* bbr)
{
	/*暂时没有实现*/
	return bbr_create_rate_upate(bbr, -1);
}

void bbr_on_remote_bitrate_report(bbr_controller_t* bbr, bbr_remote_bitrate_report_t* report)
{

}

void bbr_on_loss_report(bbr_controller_t* bbr, bbr_loss_report_t* report)
{

}

void bbr_on_rtt_update(bbr_controller_t* bbr, bbr_rtt_update_t* info)
{

}

bbr_network_ctrl_update_t bbr_on_heartbeat(bbr_controller_t* bbr, int64_t now_ts)
{
	return bbr_create_rate_upate(bbr, now_ts);
}

bbr_network_ctrl_update_t bbr_on_target_rate_constraints(bbr_controller_t* bbr, bbr_target_rate_constraint_t* msg)
{
	bbr->constraints = *msg;
	return bbr_create_rate_upate(bbr, msg->at_time);
}

void bbr_on_send_packet(bbr_controller_t* bbr, bbr_packet_info_t* packet)
{
	bbr->last_sent_packet = packet->seq;
	if (packet->data_in_flight == 0 && sampler_is_app_limited(bbr->sampler) == 1)
		bbr->exiting_quiescence = true;

	if (bbr->aggregation_epoch_start_time == -1)
		bbr->aggregation_epoch_start_time = packet->send_time;

	/*记录发送的包序列信息*/
	sampler_on_packet_sent(bbr->sampler, packet->send_time, packet->seq, packet->size, packet->data_in_flight);
}

static inline int bbr_in_slow_start(bbr_controller_t* bbr)
{
	return bbr->mode == STARTUP ? 1 : 0;
}

static inline int bbr_in_recovery(bbr_controller_t* bbr)
{
	return bbr->recovery_state != NOT_IN_RECOVERY ? true : false;
}

static inline int bbr_can_send(bbr_controller_t* bbr, size_t bytes_in_flight)
{
	return bytes_in_flight < bbr_get_congestion_window(bbr) ? true : false;
}

static int32_t bbr_pacing_rate(bbr_controller_t* bbr)
{
	if (bbr->pacing_rate == 0)
		return kHighGain * bbr->initial_congestion_window / bbr_get_min_rtt(bbr);
	else
		return bbr->pacing_rate;
}

static int32_t bbr_bandwidth_estimate(bbr_controller_t* bbr)
{
	return (int32_t)wnd_filter_best(&bbr->max_bandwidth);
}

/*计算BDP的发送窗大小*/
static size_t bbr_get_target_congestion_window(bbr_controller_t* bbr, double gain)
{
	size_t bdp, congestion_window;
	
	bdp = (int32_t)(bbr_get_min_rtt(bbr) * bbr_bandwidth_estimate(bbr));
	congestion_window = gain * bdp;
	
	if (congestion_window <= 0)
		congestion_window = gain * bbr->initial_congestion_window;

	return SU_MAX(congestion_window, bbr->min_congestion_window);
}

/*获取在probe_rtt模式下的拥塞窗口大小*/
static size_t bbr_probe_rtt_congestion_window(bbr_controller_t* bbr)
{
	if (bbr->config.probe_rtt_based_on_bdp)
		return bbr_get_target_congestion_window(bbr, bbr->config.probe_rtt_congestion_window_gain);
	else
		return bbr->min_congestion_window;
}

/*获得bbr当前的拥塞窗口大小*/
static size_t bbr_get_congestion_window(bbr_controller_t* bbr)
{
	/*如果是评估最小rtt模式下，需要取最小的并发窗口*/
	if (bbr->mode == PROBE_RTT)
		return bbr_probe_rtt_congestion_window(bbr);

	/*如果是在recovery阶段且受recover window的约束，返回约束后的值*/
	if (bbr_in_recovery(bbr) && !bbr->config.rate_based_recovery
		&& !(bbr->mode == STARTUP && bbr->config.rate_based_startup))
		return SU_MIN(bbr->congestion_window, bbr->recovery_window);

	return bbr->congestion_window;
}

bbr_network_ctrl_update_t bbr_on_feedback(bbr_controller_t* bbr, bbr_feedback_t* feedback)
{
	int64_t	feedback_recv_time, last_acked_packet;
	int loss_num = 0, acked_num = 0, i;
	bbr_packet_info_t *last_sent_packet, loss_packets[MAX_FEELBACK_COUNT], acked_packets[MAX_FEELBACK_COUNT];
	size_t total_data_acked_before, data_acked_size, data_lost_size;
	int is_round_start = false, min_rtt_expired = false;

	/*没有反馈单元,直接返回*/
	if (feedback->packets_num <= 0)
		return bbr_create_rate_upate(bbr, feedback->feedback_time);

	/*统计RTT*/
	last_sent_packet = &feedback->packets[feedback->packets_num - 1];
	bbr_rtt_update(&bbr->rtt_stat, feedback_recv_time - last_sent_packet->send_time, 0, feedback_recv_time);

	total_data_acked_before = sampler_total_data_acked(bbr->sampler);

	/*对丢包的处理*/
	loss_num = bbr_feedback_get_loss(feedback, loss_packets, MAX_FEELBACK_COUNT);
	bbr_discard_lost_packets(bbr, loss_packets, loss_num);

	/*对acked的处理*/
	acked_num = bbr_feedback_get_received(feedback, acked_packets, MAX_FEELBACK_COUNT);
	/*对丢包速率进行评估*/
	bbr_loss_filter_update(&bbr->loss_rate, feedback_recv_time, feedback->packets_num, loss_num);

	if (acked_num > 0){
		last_acked_packet = acked_packets[acked_num - 1].seq;
		is_round_start = bbr_update_round_trip_counter(bbr, last_acked_packet);
		min_rtt_expired = bbr_update_bandwidth_and_min_rtt(bbr, feedback_recv_time, acked_packets, acked_num);

		bbr_udpate_recovery_state(bbr, last_acked_packet, loss_num > 0 ? true : false, is_round_start);

		data_acked_size = sampler_total_data_acked(bbr->sampler) - total_data_acked_before;
		bbr_update_ack_aggregation_bytes(bbr, feedback_recv_time, data_acked_size);
		if (bbr->max_aggregation_bytes_multiplier > 0){
			if (feedback->data_in_flight <= 1.25 * bbr_get_target_congestion_window(bbr, bbr->pacing_rate))
				bbr->bytes_acked_since_queue_drained = 0;
			else
				bbr->bytes_acked_since_queue_drained += data_acked_size;
		}
	}

	/*Handle logic specific to PROBE_BW mode*/
	if (bbr->mode == PROBE_BW)
		bbr_update_gain_cycle_phase(bbr, feedback_recv_time, feedback->prior_in_flight, loss_num > 0 ? true : false);

	/* Handle logic specific to STARTUP and DRAIN modes */
	if (is_round_start && !bbr->is_at_full_bandwidth)
		bbr_check_if_full_bandwidth_reached(bbr);
	
	bbr_maybe_exit_startup_or_drain(bbr, feedback);

	/*Handle logic specific to PROBE_RTT,判断是否要进行RTT评估或结束RTT评估*/
	bbr_maybe_enter_or_exit_probe_rtt(bbr, feedback, is_round_start, min_rtt_expired);

	/*计算拥塞窗口大小*/
	data_acked_size = sampler_total_data_acked(bbr->sampler) - total_data_acked_before;
	data_lost_size = 0;
	for (i = 0; i < loss_num; ++i)
		data_lost_size += loss_packets[i].size;

	bbr_calculate_pacing_rate(bbr);
	bbr_calculate_congestion_window(bbr, data_acked_size);
	bbr_calculate_recovery_window(bbr, data_acked_size, data_lost_size, feedback->data_in_flight);

	/*带宽统计向前移动*/
	if (acked_num > 0)
		sampler_remove_old(bbr->sampler, last_acked_packet);

	return bbr_create_rate_upate(bbr, feedback->feedback_time);
}

/*bbr进入STARTUP模式*/
static void bbr_enter_startup_mode(bbr_controller_t* bbr)
{
	bbr->mode = STARTUP;
	bbr->pacing_gain = kHighGain;
	bbr->congestion_window_gain = kHighGain;
}

static double bbr_get_pacing_gain(bbr_controller_t* bbr, int index)
{
	if (index == 0)
		return 1 + bbr->config.probe_bw_pacing_gain_offset;
	else if (index == 1)
		return 1 - bbr->config.probe_bw_pacing_gain_offset;
	else
		return 1;
}
/*bbr进入带宽评估模式*/
static void bbr_enter_probe_bandwidth_mode(bbr_controller_t* bbr, int64_t now_ts)
{
	bbr->mode = PROBE_BW;
	bbr->congestion_window_gain = bbr->congestion_window_gain_constant;

	bbr->cycle_current_offset = rand() % (kGainCycleLength - 1);
	if (bbr->cycle_current_offset >= 1)
		bbr->cycle_current_offset += 1;

	bbr->last_cycle_start = now_ts;
	bbr->pacing_gain = bbr_get_pacing_gain(bbr, bbr->cycle_current_offset);
}

static void bbr_discard_lost_packets(bbr_controller_t* bbr, bbr_packet_info_t packets[], int size)
{
	int i;
	for (i = 0; i < size; ++i)
		sampler_on_packet_lost(bbr->sampler, packets[i].seq);
}

/*判断是否过了一个round trip周期，如果是，进行计数器+1，并更新新一期的round trip的位置信息*/
static int bbr_update_round_trip_counter(bbr_controller_t* bbr, int64_t last_acked_packet)
{
	if (last_acked_packet > bbr->current_round_trip_end){
		bbr->round_trip_count++;
		bbr->current_round_trip_end = bbr->last_sent_packet;
		return true;
	}
	else
		return false;
}

#define kMinRttExpiry 10000
static int bbr_should_extend_min_rtt_expiry(bbr_controller_t* bbr)
{
	if (bbr->config.probe_rtt_disabled_if_app_limited && bbr->app_limited_since_last_probe_rtt)
		return true;

	if (bbr->config.probe_rtt_skipped_if_similar_rtt && bbr->app_limited_since_last_probe_rtt
		&& bbr->min_rtt_since_last_probe_rtt <= bbr->min_rtt * kSimilarMinRttThreshold)
		return true;

	return false;
}

static int bbr_update_bandwidth_and_min_rtt(bbr_controller_t* bbr, int64_t now_ts, bbr_packet_info_t packets[], int size)
{
	int i, min_rtt_expired;
	int64_t sample_rtt = -1;
	bbr_bandwidth_sample_t sample;

	for (i = 0; i < size; ++i){
		sample = sampler_on_packet_acked(bbr->sampler, now_ts, packets[i].seq);
		bbr->last_sample_is_app_limited = sample.is_app_limited;

		/*对RTT的计算*/
		if (sample.rtt > 0){
			if (sample_rtt == -1)
				sample_rtt = sample.rtt;
			else
				sample_rtt = SU_MIN(sample_rtt, sample.rtt);
		}

		/*进行带宽统计和滤波*/
		if (!sample.is_app_limited || sample.bandwidth > bbr_bandwidth_estimate(bbr))
			wnd_filter_update(&bbr->max_bandwidth, sample.bandwidth, bbr->round_trip_count);
	}

	if (sample_rtt == -1)
		return false;

	bbr->last_rtt = sample_rtt;
	if (bbr->min_rtt_since_last_probe_rtt == -1)
		bbr->min_rtt_since_last_probe_rtt = sample_rtt;
	else
		bbr->min_rtt_since_last_probe_rtt = SU_MIN(bbr->min_rtt_since_last_probe_rtt, sample_rtt);

	/*评估是否要RTT的作用时间是否过期*/
	min_rtt_expired = (bbr->min_rtt > 0 && now_ts > (bbr->min_rtt_timestamp + kMinRttExpiry)) ? true : false;
	if (min_rtt_expired || sample_rtt < bbr->min_rtt || bbr->min_rtt <= 0){
		if (bbr_should_extend_min_rtt_expiry(bbr))
			min_rtt_expired = false;
		else
			bbr->min_rtt = sample_rtt;

		bbr->min_rtt_timestamp = now_ts;
		bbr->min_rtt_since_last_probe_rtt = -1;
		bbr->app_limited_since_last_probe_rtt = false;
	}

	return min_rtt_expired;
}

/*如果是在probe_bw模式下，尝试进行pacing_gain参数放大，以此来探测最大的带宽*/
static void bbr_update_gain_cycle_phase(bbr_controller_t* bbr, int64_t now_ts, size_t prior_in_flight, int losses)
{
	int gain_cycling;

	gain_cycling = (now_ts - bbr->last_cycle_start > bbr_get_min_rtt(bbr)) ? true : false;

	// If the pacing gain is above 1.0, the connection is trying to probe the
	// bandwidth by increasing the number of bytes in flight to at least
	// pacing_gain * BDP.  Make sure that it actually reaches the target, as long
	// as there are no losses suggesting that the buffers are not able to hold
	// that much.
	if (bbr->pacing_gain > 1.0 && !losses && prior_in_flight < bbr_get_target_congestion_window(bbr, bbr->pacing_gain))
		gain_cycling = false;

	// If pacing gain is below 1.0, the connection is trying to drain the extra
	// queue which could have been incurred by probing prior to it.  If the number
	// of bytes in flight falls down to the estimated BDP value earlier, conclude
	// that the queue has been successfully drained and exit this cycle early.	
	if (bbr->pacing_gain < 1.0 && prior_in_flight < bbr_get_target_congestion_window(bbr, 1))
		gain_cycling = true;

	if (gain_cycling){
		bbr->cycle_current_offset = (bbr->cycle_current_offset + 1) % kGainCycleLength;
		bbr->last_cycle_start = now_ts;

		if (bbr->config.fully_drain_queue && bbr->pacing_gain < 1 && bbr_get_pacing_gain(bbr, bbr->cycle_current_offset) == 1
			&& prior_in_flight < bbr_get_target_congestion_window(bbr, 1))
			return;

		bbr->pacing_gain = bbr_get_pacing_gain(bbr, bbr->cycle_current_offset);
	}
}

/*对STARTUP和DRAIN状态下的带宽进行判断，判断当前带宽是否达到链路最高，如达到最高，设置为带宽充满标示*/
static void bbr_check_if_full_bandwidth_reached(bbr_controller_t* bbr)
{
	size_t target;

	/*上层应用暂停了bbr控制模式*/
	if (bbr->last_sample_is_app_limited)
		return;

	target = bbr->bandwidth_at_last_round * kStartupGrowthTarget;
	if (target <= bbr_bandwidth_estimate(bbr)){
		bbr->bandwidth_at_last_round = bbr_bandwidth_estimate(bbr);
		bbr->rounds_without_bandwidth_gain = 0;
	}
	else{
		bbr->rounds_without_bandwidth_gain++;

		if (bbr->rounds_without_bandwidth_gain >= bbr->config.num_startup_rtts
			|| (bbr->config.exit_startup_on_loss && bbr_in_recovery(bbr)))
			bbr->is_at_full_bandwidth = true;
	}
}





