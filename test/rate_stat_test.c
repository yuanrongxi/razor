#include "rate_stat.h"
#include "common_test.h"

#define default_stat_wnd		1000
#define default_stat_scale		8000
static void test_avage_rate()
{
	rate_stat_t rate;
	int64_t now_ts;

	int i, bitrate;

	rate_stat_init(&rate, default_stat_wnd, default_stat_scale);

	now_ts = GET_SYS_MS();

	for (i = 0; i < 2000; i++){
		now_ts++;
		rate_stat_update(&rate, 10, now_ts);
		rate_stat_update(&rate, 10, now_ts);
	}

	bitrate = rate_stat_rate(&rate, now_ts);
	EXPECT_EQ(bitrate, 20 * default_stat_scale);

	rate_stat_destroy(&rate);
}

static void test_interval_rate()
{
	rate_stat_t rate;
	int64_t now_ts;

	int i, bitrate;

	rate_stat_init(&rate, default_stat_wnd, default_stat_scale);

	now_ts = GET_SYS_MS();

	for (i = 0; i < 2000; i++){
		now_ts++;
		if (i % 2 == 0)
			rate_stat_update(&rate, 10, now_ts);
	}

	bitrate = rate_stat_rate(&rate, now_ts);
	EXPECT_EQ(bitrate, 5 * default_stat_scale);

	rate_stat_destroy(&rate);
}

static void test_var_bytes()
{
	rate_stat_t rate;
	int64_t now_ts;

	int i, bitrate;

	rate_stat_init(&rate, default_stat_wnd, default_stat_scale);

	now_ts = GET_SYS_MS();

	for (i = 0; i < 2000; i++){
		now_ts++;
		if (i % 2 == 0)
			rate_stat_update(&rate, 10-5, now_ts);
		else
			rate_stat_update(&rate, 10+5, now_ts);
	}

	bitrate = rate_stat_rate(&rate, now_ts);
	EXPECT_EQ(bitrate, 10 * default_stat_scale);

	rate_stat_destroy(&rate);
}

void test_rate_stat()
{
	test_avage_rate();
	test_interval_rate();
	test_var_bytes();
}



