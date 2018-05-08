#include "pace_sender.h"
#include "common_test.h"

#define k_packet_size	1000
#define k_delay_ms		5

static int total_size = 0;
static int send_total_size = 0;
static int64_t g_update_ts;

static void send_test(void* handler, uint32_t seq, int retrans, size_t size)
{
	int64_t now_ts = GET_SYS_MS();
	int out_bitrate, in_bitrate;

	total_size += size;

	if (g_update_ts + 1000 <= now_ts){
		out_bitrate = total_size / (now_ts - g_update_ts);
		in_bitrate = send_total_size / (now_ts - g_update_ts);
		printf("out bitrate = %dkbps, in bitrate = %dkbps\n", out_bitrate, in_bitrate);
		g_update_ts = now_ts;
		total_size = 0;
		send_total_size = 0;
	}
}

#define k_queue_ms 250
static void test_min_bitrate()
{
	int64_t now_ts, send_ts;
	pace_sender_t* pace = pace_create(NULL, send_test, k_queue_ms);
	uint32_t seq = 0, i = 0;

	pace_set_estimate_bitrate(pace, 100 * 1000 * 8);

	printf("pacing bitrate = %dkpbs\n", 100);

	send_ts = now_ts = g_update_ts = GET_SYS_MS();

	while (1){
		now_ts = GET_SYS_MS();

		if (now_ts >= send_ts + k_delay_ms * 2){
			send_ts = now_ts;
			pace_insert_packet(pace, seq++, -1, k_packet_size / 2, now_ts);
			send_total_size += k_packet_size / 2;
		}

		su_sleep(0, 1000);
		if (i++ % 1000 == 0){
			printf("alr time = %lld, delay = %lld\n", pace_get_limited_start_time(pace), pace_expected_queue_ms(pace));
		}

		pace_try_transmit(pace, GET_SYS_MS());
	}

	pace_destroy(pace);
}

static void test_max_bitrate()
{
	int64_t now_ts, send_ts;
	pace_sender_t* pace = pace_create(NULL, send_test, k_queue_ms);
	uint32_t seq = 0, cur_bitrate, i = 0;

	cur_bitrate = 100 * 1000 * 8;
	pace_set_estimate_bitrate(pace, cur_bitrate);

	printf("pacing bitrate = %dkpbs\n", 100);

	send_ts = now_ts = g_update_ts = GET_SYS_MS();

	while (1){
		now_ts = GET_SYS_MS();

		if (now_ts >= send_ts + k_delay_ms){
			send_ts = now_ts;
			pace_insert_packet(pace, seq++, -1, k_packet_size, now_ts);
			send_total_size += k_packet_size;

			/*模拟一个带宽增加量*/
			if (pace_expected_queue_ms(pace) > k_delay_ms * 4){
				cur_bitrate = cur_bitrate + cur_bitrate / 64;
				pace_set_estimate_bitrate(pace, cur_bitrate);
			}
		}

		su_sleep(0, 1000);

		if (i++ % 1000 == 0)
			printf("alr time = %lld, delay ms = %lld\n", pace_get_limited_start_time(pace), pace_expected_queue_ms(pace));

		pace_try_transmit(pace, GET_SYS_MS());
	}

	pace_destroy(pace);
}

void test_pace()
{
	//test_min_bitrate();
	//test_max_bitrate();
}




