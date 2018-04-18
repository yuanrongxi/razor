#include "cc_loss_stat.h"
#include "common_test.h"

static void test_no_loss()
{
	int i, num;
	uint16_t seq;
	uint8_t fraction_loss;
	int64_t now_ts;

	cc_loss_statistics_t loss_stat;

	loss_statistics_init(&loss_stat);
	seq = 65530;
	now_ts = 1000;

	for (i = 0; i < 1200; ++i){
		now_ts++;
		loss_statistics_incoming(&loss_stat, seq++);
	}

	fraction_loss = 0;
	num = 0;
	loss_statistics_calculate(&loss_stat, now_ts, &fraction_loss, &num);
	EXPECT_EQ(num, 1200);
	EXPECT_EQ(fraction_loss, 0);

	loss_statistics_destroy(&loss_stat);
}

static void test_out_of_order()
{
	int i, num;
	uint16_t seq;
	uint8_t fraction_loss;
	int64_t now_ts;

	cc_loss_statistics_t loss_stat;

	loss_statistics_init(&loss_stat);
	seq = 65529;
	now_ts = 1000;

	for (i = 2; i < 1202; ++i){
		now_ts++;
		if (i % 10 == 0)
			loss_statistics_incoming(&loss_stat, seq + 1);
		else if (i % 10 == 1)
			loss_statistics_incoming(&loss_stat, seq - 1);
		else
			loss_statistics_incoming(&loss_stat, seq);
		seq++;
	}

	fraction_loss = 0;
	num = 0;
	loss_statistics_calculate(&loss_stat, now_ts, &fraction_loss, &num);
	
	EXPECT_EQ(num, 1200);
	EXPECT_EQ(fraction_loss, 0);

	loss_statistics_destroy(&loss_stat);
}

static void test_lost_packet()
{
	int i, num;
	uint16_t seq;
	uint8_t fraction_loss;
	int64_t now_ts;

	cc_loss_statistics_t loss_stat;

	loss_statistics_init(&loss_stat);
	seq = 65530;
	now_ts = 1000;

	for (i = 2; i < 602; ++i){
		now_ts++;
		seq++;
		if (i % 10 != 0)
			loss_statistics_incoming(&loss_stat, seq);
	}

	/*第一个周期统计*/
	fraction_loss = 0;
	num = 0;
	loss_statistics_calculate(&loss_stat, now_ts, &fraction_loss, &num);
	EXPECT_EQ(num, 600);
	EXPECT_EQ(fraction_loss, 25);

	for (i = 2; i < 602; ++i){
		now_ts++;
		seq++;
		if (i % 10 != 0)
			loss_statistics_incoming(&loss_stat, seq);
	}

	/*在发起一个周期统计*/
	fraction_loss = 0;
	num = 0;
	loss_statistics_calculate(&loss_stat, now_ts, &fraction_loss, &num);
	EXPECT_EQ(num, 600);
	EXPECT_EQ(fraction_loss, 25);

	loss_statistics_destroy(&loss_stat);
}

void test_loss_stat()
{
	test_no_loss();
	test_out_of_order();
	test_lost_packet();
}




