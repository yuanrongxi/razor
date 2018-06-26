#include "bbr_transfer_tracker.h"
#include "common_test.h"


void test_bbr_transfer_tracker()
{
	bbr_trans_tracker_t* tracker;
	tracker_result_t result;

	tracker = tracker_create();

	tracker_add_sample(tracker, 5555, 100000, 100100);
	tracker_add_sample(tracker, 1000, 100020, 100120);
	tracker_add_sample(tracker, 1000, 100040, 100140);
	tracker_add_sample(tracker, 1000, 100060, 100160);
	tracker_add_sample(tracker, 1000, 100080, 100180);

	result = tracker_get_rates_by_ack_time(tracker, 100120, 100200);
	EXPECT_EQ(result.ack_data_size, 4000);
	EXPECT_EQ(result.ack_time, 80);
	EXPECT_EQ(result.send_time, 80);

	tracker_clear_old(tracker, 100140);

	result = tracker_get_rates_by_ack_time(tracker, 100120, 100200);
	EXPECT_EQ(result.ack_data_size, 2000);
	EXPECT_EQ(result.ack_time, 40);
	EXPECT_EQ(result.send_time, 40);

	tracker_destroy(tracker);

}
