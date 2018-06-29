/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#ifndef __bbr_controller_h_
#define __bbr_controller_h_

#include "bbr_rtt_stats.h"
#include "bbr_transfer_tracker.h"
#include "windowed_filter.h"
#include "bbr_feedback_adpater.h"
#include "bbr_common.h"

/*bbr状态*/
enum {
	/*Startup phase of the connection*/
	STARTUP,
	/*After achieving the highest possible bandwidth during the startup, lower the pacing rate in order to drain the queue*/
	DRAIN,
	/*Cruising mode*/
	PROBE_BW,
	/*Temporarily slow down sending in order to empty the buffer and measure the real minimum RTT*/
	PROBE_RTT
};

/* Indicates how the congestion control limits the amount of bytes in flight*/
enum{
	/*Do not limit*/
	NOT_IN_RECOVERY,
	/*Allow an extra outstanding byte for each byte acknowledged*/
	CONSERVATION,
	/*Allow 1.5 extra outstanding bytes for each byte acknowledged*/
	MEDIUM_GROWTH,
	/*Allow two extra outstanding bytes for each byte acknowledged (slow start)*/
	GROWTH
};

/*BBR的配置项目*/
typedef struct
{
	double probe_bw_pacing_gain_offset;
	double encoder_rate_gain;
	double encoder_rate_gain_in_probe_rtt;
	
	/* RTT delta to determine if startup should be exited due to increased RTT.*/
	int64_t exit_startup_rtt_threshold_ms;

	double probe_rtt_congestion_window_gain;

	/* Configurable in QUIC BBR:*/
	int exit_startup_on_loss;
	/* The number of RTTs to stay in STARTUP mode.  Defaults to 3.*/
	int64_t num_startup_rtts;
	/* When true, recovery is rate based rather than congestion window based.*/
	int rate_based_recovery;
	double max_aggregation_bytes_multiplier;
	/* When true, pace at 1.5x and disable packet conservation in STARTUP.*/
	int slower_startup;
	/* When true, disables packet conservation in STARTUP.*/
	int rate_based_startup;
	/* If true, will not exit low gain mode until bytes_in_flight drops below BDP or it's time for high gain mode.*/
	int fully_drain_queue;
	/* Used as the initial packet conservation mode when first entering recovery.*/
	int initial_conservation_in_startup;

	double max_ack_height_window_multiplier;
	/* If true, use a CWND of 0.75*BDP during probe_rtt instead of 4 packets.*/
	int probe_rtt_based_on_bdp;
	/* If true, skip probe_rtt and update the timestamp of the existing min_rtt
	  to now if min_rtt over the last cycle is within 12.5% of the current
	 min_rtt. Even if the min_rtt is 12.5% too low, the 25% gain cycling and
	 2x CWND gain should overcome an overly small min_rtt.*/
	int probe_rtt_skipped_if_similar_rtt;
	/*If true, disable PROBE_RTT entirely as long as the connection was recently app limited.*/
	int probe_rtt_disabled_if_app_limited;
}bbr_config_t;

/*bbr对象*/

typedef struct
{
	bbr_rtt_stat_t					rtt_stat;						/*rtt延迟平滑计算模块*/
	bbr_target_rate_constraint_t	constraints;
	
	int								mode;							/*bbr模式状态*/
	bbr_config_t					config;							/*bbr的参数配置*/

	size_t							total_bytes_acked;
	int64_t							last_acked_packet_sent_time;
	int64_t							last_acked_packet_ack_time;

	int								is_app_limited;
	int64_t							end_of_app_limited_phase;
	int64_t							round_trip_count;

	int64_t							last_send_timestamp;
	int64_t							current_round_trip_end;

	int32_t							default_bandwidth;
	windowed_filter_t				max_bandwidth;

	windowed_filter_t				max_ack_height;

	int64_t							aggregation_epoch_start_time;
	size_t							aggregation_epoch_bytes;
	size_t							bytes_acked_since_queue_drained;
	double							max_aggregation_bytes_multiplier;

	int64_t							min_rtt;
	int64_t							last_rtt;
	int64_t							min_rtt_timestamp;

	size_t							congestion_window;
	size_t							initial_congestion_window;
	size_t							max_congestion_window;

	int32_t							pacing_rate;
	double							pacing_gain;
	double							congestion_window_gain;
	double							congestion_window_gain_constant;
	double							rtt_variance_weight;

	int								exit_startup_on_loss;

	int								cycle_current_offset;
	int64_t							last_cycle_start;

	int								is_at_full_bandwidth;

	int								rounds_without_bandwidth_gain;

	int32_t							bandwidth_at_last_round;

	int64_t							exit_probe_rtt_at;
	int								probe_rtt_round_passed;

	int								last_sample_is_app_limited;

	int								recovery_state;

	int64_t							end_recovery_at;

	size_t							recovery_window;

	int								app_limited_since_last_probe_rtt;
	int64_t							min_rtt_since_last_probe_rtt;

}bbr_controller_t;


bbr_controller_t*					bbr_create();
void								bbr_destroy(bbr_controller_t* bbr);
void								bbr_reset(bbr_controller_t* bbr);

bbr_network_ctrl_update_t			bbr_on_network_availability(bbr_controller_t* bbr, bbr_network_availability_t* av);
bbr_network_ctrl_update_t			bbr_on_newwork_router_change(bbr_controller_t* bbr);
bbr_network_ctrl_update_t			bbr_on_heartbeat(bbr_controller_t* bbr, int64_t now_ts);
bbr_network_ctrl_update_t			bbr_on_send_packet(bbr_controller_t* bbr, bbr_packet_info_t* packet);
bbr_network_ctrl_update_t			bbr_on_target_rate_constraints(bbr_controller_t* bbr, bbr_target_rate_constraint_t* target);
bbr_network_ctrl_update_t			bbr_on_feedback(bbr_controller_t* bbr, bbr_feedback_t* feedback);
bbr_network_ctrl_update_t			bbr_on_loss_report(bbr_controller_t* bbr, bbr_loss_report_t* report);
bbr_network_ctrl_update_t			bbr_on_rtt_update(bbr_controller_t* bbr, bbr_rtt_update_t* info);



#endif



