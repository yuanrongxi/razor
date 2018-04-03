#include "interval_budget.h"
#include "common_test.h"

#define k_window_ms			500
#define k_bitrate_kbps		100
#define k_canbuild_up		0
#define k_cannot_build_up	-1

#define time_to_bytes(rate, ts) ((rate) * (ts)/ 8)

static void test_underuse()
{
	interval_budget_t budget;
	int delta_ts = 50;

	init_interval_budget(&budget, k_bitrate_kbps, k_cannot_build_up);
	increase_budget(&budget, delta_ts);

	EXPECT_EQ(budget_level_precent(&budget), delta_ts  * 100 / k_window_ms);
	EXPECT_EQ(budget_remaining(&budget), time_to_bytes(k_bitrate_kbps, delta_ts));
}

static void test_max_window()
{
	interval_budget_t budget;
	int delta_ts = 1000;

	init_interval_budget(&budget, k_bitrate_kbps, k_cannot_build_up);
	increase_budget(&budget, delta_ts);

	EXPECT_EQ(budget_level_precent(&budget), 100);
	EXPECT_EQ(budget_remaining(&budget), time_to_bytes(k_bitrate_kbps, k_window_ms));
}

static void test_change_bitrate()
{
	interval_budget_t budget;
	int delta_ts = k_window_ms / 2;

	init_interval_budget(&budget, k_bitrate_kbps, k_cannot_build_up);
	increase_budget(&budget, delta_ts);

	/*码率变小*/
	set_target_rate_kbps(&budget, k_bitrate_kbps / 10);
	EXPECT_EQ(budget_level_precent(&budget), 100);
	EXPECT_EQ(budget_remaining(&budget), time_to_bytes(k_bitrate_kbps / 10, k_window_ms));
}

static void test_add_bitrate()
{
	interval_budget_t budget;
	int delta_ts = k_window_ms;

	init_interval_budget(&budget, k_bitrate_kbps, k_cannot_build_up);
	increase_budget(&budget, delta_ts);

	/*码率变小*/
	set_target_rate_kbps(&budget, k_bitrate_kbps * 2);
	EXPECT_EQ(budget_level_precent(&budget), 50);
	EXPECT_EQ(budget_remaining(&budget), time_to_bytes(k_bitrate_kbps, k_window_ms));
}

static void test_overuse()
{
	interval_budget_t budget;
	int overuse_ts = 60;
	
	init_interval_budget(&budget, k_bitrate_kbps, k_cannot_build_up);

	int used_bytes = time_to_bytes(k_bitrate_kbps, overuse_ts);
	use_budget(&budget, used_bytes);

	EXPECT_EQ(budget_level_precent(&budget), -overuse_ts * 100 / k_window_ms);
	EXPECT_EQ(budget_remaining(&budget), 0);
}

static void test_build_up()
{
	interval_budget_t budget;
	int delta_ts = 50;

	init_interval_budget(&budget, k_bitrate_kbps, k_canbuild_up);

	increase_budget(&budget, delta_ts);
	EXPECT_EQ(budget_level_precent(&budget), delta_ts * 100 / k_window_ms);
	EXPECT_EQ(budget_remaining(&budget), time_to_bytes(k_bitrate_kbps, delta_ts));

	increase_budget(&budget, delta_ts);

	EXPECT_EQ(budget_level_precent(&budget), delta_ts * 100 * 2/ k_window_ms);
	EXPECT_EQ(budget_remaining(&budget), time_to_bytes(k_bitrate_kbps, delta_ts * 2));
}

static void test_not_build_up()
{
	interval_budget_t budget;
	int delta_ts = 50;

	init_interval_budget(&budget, k_bitrate_kbps, k_cannot_build_up);

	increase_budget(&budget, delta_ts);
	EXPECT_EQ(budget_level_precent(&budget), delta_ts * 100 / k_window_ms);
	EXPECT_EQ(budget_remaining(&budget), time_to_bytes(k_bitrate_kbps, delta_ts));

	increase_budget(&budget, delta_ts);

	EXPECT_EQ(budget_level_precent(&budget), delta_ts * 100 / k_window_ms);
	EXPECT_EQ(budget_remaining(&budget), time_to_bytes(k_bitrate_kbps, delta_ts));
}


void test_interval_budget()
{
	test_underuse();
	test_max_window();
	test_change_bitrate();
	test_add_bitrate();
	test_overuse();
	test_build_up();
}

