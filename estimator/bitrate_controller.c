#include "bitrate_controller.h"

/*触发上层的pacer和videosource改变码率*/
static void inline maybe_trigger_network_changed(bitrate_controller_t* ctrl)
{
	uint32_t rtt, bitrate;
	uint8_t fraction_loss;

	if (ctrl->trigger != NULL && ctrl->trigger_func != NULL
		&& bitrate_controller_get_parameter(ctrl, &bitrate, &fraction_loss, &rtt) == 0){
		ctrl->trigger_func(ctrl->trigger, bitrate, fraction_loss, rtt);
	}
}

bitrate_controller_t* bitrate_controller_create(void* trigger, bitrate_changed_func func)
{
	bitrate_controller_t* ctrl = calloc(1, sizeof(bitrate_controller_t));
	ctrl->last_bitrate_update_ts = GET_SYS_MS();
	ctrl->est = sender_estimation_create(10000, 1500000);

	ctrl->trigger = trigger;
	ctrl->trigger_func = func;

	maybe_trigger_network_changed(ctrl);

	return ctrl;
}

void bitrate_controller_destroy(bitrate_controller_t* ctrl)
{
	if (ctrl == NULL)
		return;

	if (ctrl->est != NULL){
		sender_estimation_destroy(ctrl->est);
		ctrl->est = NULL;
	}

	free(ctrl);
}

uint32_t bitrate_controller_available_bandwidth(bitrate_controller_t* ctrl)
{
	uint32_t bitrate;

	bitrate = ctrl->est->curr_bitrate;
	if (bitrate > 0){
		bitrate = bitrate - SU_MIN(bitrate, ctrl->reserved_bitrate_bps);
		bitrate = SU_MAX(bitrate, ctrl->est->max_conf_bitrate);
	}

	return bitrate;
}

void bitrate_controller_set_start_bitrate(bitrate_controller_t* ctrl, uint32_t start_bitrate)
{
	sender_estimation_set_send_bitrate(ctrl->est, start_bitrate);
	maybe_trigger_network_changed(ctrl);
}

void bitrate_controller_set_conf_bitrate(bitrate_controller_t* ctrl, uint32_t min_bitrate, uint32_t max_bitrate)
{
	sender_estimation_set_minmax_bitrate(ctrl->est, min_bitrate, max_bitrate);
	maybe_trigger_network_changed(ctrl);
}

void bitrate_controller_set_bitrates(bitrate_controller_t* ctrl, uint32_t bitrate, uint32_t min_bitrate, uint32_t max_bitrate)
{
	sender_estimation_set_minmax_bitrate(ctrl->est, min_bitrate, max_bitrate);
	sender_estimation_set_send_bitrate(ctrl->est, bitrate);
	maybe_trigger_network_changed(ctrl);
}

void bitrate_controller_reset_bitrates(bitrate_controller_t* ctrl, uint32_t bitrate, uint32_t min_bitrate, uint32_t max_bitrate)
{
	/*重置sender estimation*/
	sender_estimation_destroy(ctrl->est);
	ctrl->est = sender_estimation_create(10000, 1500000);

	sender_estimation_set_minmax_bitrate(ctrl->est, min_bitrate, max_bitrate);
	sender_estimation_set_send_bitrate(ctrl->est, bitrate);
	maybe_trigger_network_changed(ctrl);
}

void bitrate_controller_set_reserved(bitrate_controller_t* ctrl, uint32_t bitrate)
{
	ctrl->reserved_bitrate_bps = bitrate;
	maybe_trigger_network_changed(ctrl);
}

void bitrate_controller_on_remb(bitrate_controller_t* ctrl, uint32_t bitrate)
{
	sender_estimation_update_remb(ctrl->est, GET_SYS_MS(), bitrate);
	maybe_trigger_network_changed(ctrl);
}

void bitrate_controller_on_report(bitrate_controller_t* ctrl, uint32_t rtt, int64_t cur_ts, uint8_t fraction_loss, int packets_num)
{
	sender_estimation_update_block(ctrl->est, fraction_loss, rtt, packets_num, rtt);
	maybe_trigger_network_changed(ctrl);
}

void bitrate_controller_on_basedelay_result(bitrate_controller_t* ctrl, int update, int probe, uint32_t target_bitrate, int recovered_from_overuse)
{
	if (update == -1)
		return;

	sender_estimation_update_delay_base(ctrl->est, GET_SYS_MS(), target_bitrate);
	if (probe == 0)
		sender_estimation_set_send_bitrate(ctrl->est, target_bitrate);
	maybe_trigger_network_changed(ctrl);
}

void bitrate_controller_heartbeat(bitrate_controller_t* ctrl, int64_t cur_ts)
{
	if (cur_ts < ctrl->last_bitrate_update_ts + 25)
		return;

	sender_estimation_update(ctrl->est, cur_ts);
	maybe_trigger_network_changed(ctrl);

	ctrl->last_bitrate_update_ts = cur_ts;
}

int bitrate_controller_get_parameter(bitrate_controller_t* ctrl, uint32_t* bitrate, uint8_t* fraction_loss, uint32_t* rtt)
{
	int ret = -1;
	uint32_t cur_bitrate;

	cur_bitrate = ctrl->est->curr_bitrate;
	cur_bitrate -= SU_MIN(cur_bitrate, ctrl->reserved_bitrate_bps);
	*rtt = ctrl->est->last_rtt;
	*fraction_loss = ctrl->est->last_fraction_loss;
	*bitrate = SU_MAX(cur_bitrate, ctrl->est->min_conf_bitrate);

	/*判断网络状态是否发生了改变,发生了改变进行返回*/
	if (*fraction_loss != ctrl->last_fraction_loss || *rtt != ctrl->last_rtt || *bitrate != ctrl->last_bitrate_bps
		|| ctrl->last_reserved_bitrate_bps != ctrl->reserved_bitrate_bps){

		ctrl->last_fraction_loss = *fraction_loss;
		ctrl->last_bitrate_bps = *bitrate;
		ctrl->last_rtt = *rtt;
		ctrl->last_reserved_bitrate_bps = ctrl->reserved_bitrate_bps;

		ret = 0;
	}

	return ret;
}









