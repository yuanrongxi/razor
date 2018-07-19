#include "common_test.h"
#include "bbr_controller.h"
#include "cf_list.h"

/*60kps*/
#define kInitialBitrate			60
#define kDefaultStartTime		10000000

#define kDataRateMargin			0.3
#define kMinDataRateFactor		(1 - kDataRateMargin)
#define kMaxDataRateFactor		(1 + kDataRateMargin)

static void init_test_bbr_config(bbr_target_rate_constraint_t* co, int32_t min_rate, int32_t max_rate)
{
	co->at_time = kDefaultStartTime;
	co->min_rate = min_rate;
	co->max_rate = max_rate;
}

static void test_bbr_hb()
{
	bbr_controller_t* bbr;
	bbr_target_rate_constraint_t co;
	bbr_network_ctrl_update_t update;

	init_test_bbr_config(&co, 0, 5 * kInitialBitrate);

	bbr = bbr_create(&co, kInitialBitrate);

	update = bbr_on_heartbeat(bbr, kDefaultStartTime);

	bbr_destroy(bbr);
}

typedef struct
{
	bbr_controller_t*	bbr;
	int64_t				interval_ts;
	int64_t				curr_ts;
	int64_t				packet_number;
	int64_t				accumulated_buffer;
	int64_t				last_ts;
	bbr_network_ctrl_update_t update;
	base_list_t*		outstanding;
}test_bbr_controller_t;

static void test_bbr_controller_init(test_bbr_controller_t* ctrl, bbr_target_rate_constraint_t* co, int32_t starting_bandwidth)
{
	ctrl->curr_ts = 100000;
	co->at_time = ctrl->curr_ts;

	ctrl->bbr = bbr_create(co, starting_bandwidth);
	ctrl->packet_number = 1;
	ctrl->interval_ts = 100000000;
	ctrl->accumulated_buffer = 0;
	ctrl->last_ts = -1;

	ctrl->update = bbr_on_heartbeat(ctrl->bbr, ctrl->curr_ts);

	ctrl->outstanding = create_list();
}

static void test_bbr_controller_destroy(test_bbr_controller_t* ctrl)
{
	base_list_unit_t* iter;
	bbr_packet_info_t* packet;

	if (ctrl->bbr != NULL){
		bbr_destroy(ctrl->bbr);
		ctrl->bbr = NULL;
	}

	LIST_FOREACH(ctrl->outstanding, iter){
		packet = iter->pdata;
		free(packet);
	}

	destroy_list(ctrl->outstanding);
}

static bbr_packet_info_t test_bbr_next(test_bbr_controller_t* ctrl, bbr_network_ctrl_update_t cache, int64_t curr_ts, int64_t time_delta)
{
	int32_t actual_send_rate;
	bbr_packet_info_t packet;

	actual_send_rate = SU_MAX(cache.target_rate.target_rate, bbr_pacer_pad_rate(&cache.pacer_config));

	packet.send_time = curr_ts;
	packet.recv_time = -1;
	packet.data_in_flight = 0;
	packet.size = time_delta * actual_send_rate;
	
	return packet;
}

static void test_run_simulation(test_bbr_controller_t* ctrl, int64_t duration, 
	int64_t packet_interval, int32_t actual_bandwidth, int64_t propagation_delay)
{
	int64_t start_time = ctrl->curr_ts;
	int64_t last_process_time = ctrl->curr_ts;
	
	int64_t time_in_flight, total_delay;

	int send_flag = 0, index;
	size_t data_in_flight;
	base_list_unit_t* iter;

	bbr_packet_info_t *packet, *second_packet, sent_packet;

	bbr_feedback_t feedback;

	while (ctrl->curr_ts < duration + start_time){
		send_flag = 1;
	
		/*判断是否可以继续发送新的报文*/
		if (ctrl->update.congestion_window > 0){
			data_in_flight = 0;

			LIST_FOREACH(ctrl->outstanding, iter){
				packet = iter->pdata;
				data_in_flight += packet->size;
			}
			if (data_in_flight > ctrl->update.congestion_window)
				send_flag = 0;
		}

		/*进行发包模拟*/
		if (send_flag == 1 || list_size(ctrl->outstanding) < 2){ /*必须有2个报文在通道中传输 TCPMSS*/
			sent_packet = test_bbr_next(ctrl, ctrl->update, ctrl->curr_ts, packet_interval);
			sent_packet.seq = ctrl->packet_number++;
			sent_packet.data_in_flight = sent_packet.size;
			
			LIST_FOREACH(ctrl->outstanding, iter){
				packet = iter->pdata;
				sent_packet.data_in_flight += packet->size;
			}

			ctrl->update = bbr_on_send_packet(ctrl->bbr, &sent_packet);

			/*计算发送的间隔时间值*/
			time_in_flight = sent_packet.size / actual_bandwidth;
			ctrl->accumulated_buffer += time_in_flight;
			total_delay = propagation_delay + ctrl->accumulated_buffer;

			packet = calloc(1, sizeof(bbr_packet_info_t));
			*packet = sent_packet;
			packet->recv_time = sent_packet.send_time + total_delay;

			list_push(ctrl->outstanding, packet);
		}

		/*进行feedback操作*/
		int64_t buffer_consumed = SU_MIN(ctrl->accumulated_buffer, packet_interval);
		ctrl->accumulated_buffer -= buffer_consumed;

		index = 0;
		if (list_size(ctrl->outstanding) >= 2){
			LIST_FOREACH(ctrl->outstanding, iter){
				if (++index == 2){
					second_packet = iter->pdata;
					break;
				}
			}

			/*如果最前面2个报文单元是满足feedback的，进行feedback数据拼凑*/
			if (second_packet->recv_time + propagation_delay <= ctrl->curr_ts){
				feedback.prior_in_flight = 0;
				LIST_FOREACH(ctrl->outstanding, iter){
					packet = iter->pdata;
					feedback.prior_in_flight += packet->size;
				}

				feedback.packets_num = 0;
				while (list_size(ctrl->outstanding) > 0){
					packet = list_front(ctrl->outstanding);
					if (ctrl->curr_ts >= packet->recv_time + propagation_delay){
						feedback.packets[feedback.packets_num++] = *packet;
						ctrl->last_ts = packet->recv_time;

						free(packet);
						list_pop(ctrl->outstanding);
					}
					else
						break;
				}

				feedback.feedback_time = ctrl->last_ts + propagation_delay;

				feedback.data_in_flight = 0;
				LIST_FOREACH(ctrl->outstanding, iter){
					packet = iter->pdata;
					feedback.data_in_flight += packet->size;
				}

				ctrl->update = bbr_on_feedback(ctrl->bbr, &feedback);
			}
		}

		/*进行heartbeat*/
		ctrl->curr_ts += packet_interval;
		if (ctrl->curr_ts > last_process_time + ctrl->interval_ts)
			ctrl->update = bbr_on_heartbeat(ctrl->bbr, ctrl->curr_ts);
	}

	ctrl->update = bbr_on_heartbeat(ctrl->bbr, ctrl->curr_ts);
}

static void bbr_test_update_send_rate()
{
	test_bbr_controller_t test;
	bbr_target_rate_constraint_t co;
	int32_t rate;

	init_test_bbr_config(&co, 0, 600);
	test_bbr_controller_init(&test, &co, 60);

	test_run_simulation(&test, 5000, 10, 300, 100);
	rate = wnd_filter_best(&test.bbr->max_bandwidth);
	printf("rate = %ukbps\n", rate);

	EXPECT_GE(test.update.target_rate.target_rate, 300 * kMinDataRateFactor);
	EXPECT_LE(test.update.target_rate.target_rate, 300 * kMaxDataRateFactor);

	printf("accumulated_buffer = %ums\n", test.accumulated_buffer);
	test_run_simulation(&test, 30000, 10, 500, 100);
	rate = wnd_filter_best(&test.bbr->max_bandwidth);
	printf("rate = %ukbps\n", rate);
	EXPECT_GE(test.update.target_rate.target_rate, 500 * kMinDataRateFactor);
	EXPECT_LE(test.update.target_rate.target_rate, 500 * kMaxDataRateFactor);

	test_run_simulation(&test, 30000, 10, 200, 100);
	EXPECT_GE(test.update.target_rate.target_rate, 200 * kMinDataRateFactor);
	EXPECT_LE(test.update.target_rate.target_rate, 200 * kMaxDataRateFactor);

	printf("accumulated_buffer = %ums\n", test.accumulated_buffer);
	test_run_simulation(&test, 30000, 10, 100, 200);
	rate = wnd_filter_best(&test.bbr->max_bandwidth);
	printf("rate = %ukbps\n", rate);

	EXPECT_GE(test.update.target_rate.target_rate, 100 * kMinDataRateFactor);
	EXPECT_LE(test.update.target_rate.target_rate, 100 * kMaxDataRateFactor);

	printf("accumulated_buffer = %ums\n", test.accumulated_buffer);
	test_run_simulation(&test, 30000, 10, 50, 400);
	rate = wnd_filter_best(&test.bbr->max_bandwidth);
	printf("rate = %ukbps\n", rate);

	EXPECT_GE(test.update.target_rate.target_rate, 50 * kMinDataRateFactor);
	EXPECT_LE(test.update.target_rate.target_rate, 50 * kMaxDataRateFactor);

	printf("accumulated_buffer = %ums\n", test.accumulated_buffer);

	test_bbr_controller_destroy(&test);
}

void test_bbr_proc()
{
	test_bbr_hb();
	bbr_test_update_send_rate();
}





