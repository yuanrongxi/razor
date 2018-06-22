#include "common_test.h"
#include "windowed_filter.h"

typedef struct
{
	windowed_filter_t		min_rtt;
	windowed_filter_t		max_bw;
}windowed_filter_test_t;


static void windowed_filter_test_init(windowed_filter_test_t* f)
{
	wnd_filter_init(&f->min_rtt, 99, min_val_func);
	wnd_filter_init(&f->max_bw, 99, max_val_func);
}

static void init_min_filter(windowed_filter_test_t* f)
{
	int i;
	int64_t now_ts = 0;
	int64_t rtt_sample = 10;

	for (i = 0; i < 5; ++i){
		wnd_filter_update(&f->min_rtt, rtt_sample, now_ts);
		now_ts += 25;
		rtt_sample += 10;
	}

	EXPECT_EQ(20, wnd_filter_best(&f->min_rtt));
	EXPECT_EQ(40, wnd_filter_second_best(&f->min_rtt));
	EXPECT_EQ(50, wnd_filter_third_best(&f->min_rtt));
}

static void init_max_filter(windowed_filter_test_t* f)
{
	int i;
	int64_t now_ts = 0;
	size_t bw_sample = 1000;

	for (i = 0; i < 5; ++i){
		wnd_filter_update(&f->max_bw, bw_sample, now_ts);
		now_ts += 25;
		bw_sample -= 100;
	}

	EXPECT_EQ(900, wnd_filter_best(&f->max_bw));
	EXPECT_EQ(700, wnd_filter_second_best(&f->max_bw));
	EXPECT_EQ(600, wnd_filter_third_best(&f->max_bw));
}

static void test_inited_windowed_filter()
{
	windowed_filter_test_t f;
	windowed_filter_test_init(&f);
	init_min_filter(&f);
	init_max_filter(&f);
}

static void test_sample_change_thirdbest_min()
{
	int64_t rtt_sample, now_ts;
	windowed_filter_test_t f;
	windowed_filter_test_init(&f);
	init_min_filter(&f);

	rtt_sample = wnd_filter_third_best(&f.min_rtt) - 5;
	now_ts = 101;
	wnd_filter_update(&f.min_rtt, rtt_sample, now_ts);
	
	EXPECT_EQ(20, wnd_filter_best(&f.min_rtt));
	EXPECT_EQ(40, wnd_filter_second_best(&f.min_rtt));
	EXPECT_EQ(rtt_sample, wnd_filter_third_best(&f.min_rtt));
}

void test_windowed_filter()
{
	test_inited_windowed_filter();
	test_sample_change_thirdbest_min();
}
