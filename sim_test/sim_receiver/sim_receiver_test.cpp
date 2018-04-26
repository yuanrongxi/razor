/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#include <list>
#include "cf_platform.h"
#include "sim_external.h"
#include "audio_log.h"

#include <time.h>
#include <assert.h>

enum
{
	el_start_play,
	el_stop_play,

	el_unknown = 1000
};

typedef struct
{
	int			msg_id;
	uint32_t	val;
}thread_msg_t;

typedef std::list<thread_msg_t>	msg_queue_t;


static msg_queue_t main_queue;
su_mutex main_mutex;

static void notify_callback(int type, uint32_t val)
{
	thread_msg_t msg;
	msg.msg_id = el_unknown;

	switch (type){
	case sim_start_play_notify:
		msg.msg_id = el_start_play;
		msg.val = val;
		break;

	case sim_stop_play_notify:
		msg.msg_id = el_stop_play;
		msg.val = val;
		break;

	default:
		return;
	}

	su_mutex_lock(main_mutex);
	main_queue.push_back(msg);
	su_mutex_unlock(main_mutex);
}

static void notify_change_bitrate(uint32_t bitrate_kbps)
{
}

static void notify_state(uint32_t rbw, uint32_t sbw)
{
	printf("send bandwidth = %uKB/s, recv bandwidth = %uKB/s\n", sbw, rbw);
}

static int64_t play_video(uint8_t* video_frame, size_t size)
{
	int ret, count;
	uint8_t* pos;
	size_t frame_size;
	int64_t frame_ts;
	 
	ret = sim_recv_video(video_frame, &size);
	if (ret != 0) 
		return -1;

	pos = video_frame;
	/*校验数据的完整性*/

	memcpy(&frame_size, pos, sizeof(frame_size));
	pos += sizeof(frame_size);
	memcpy(&count, pos, sizeof(count));
	pos += sizeof(count);
	memcpy(&frame_ts, pos, sizeof(frame_ts));

	assert(frame_size == size);

	return frame_ts;
}

#define FRAME_SIZE (1024 * 1024)

static void main_loop_event()
{
	thread_msg_t msg;
	int run = 1, packet_count, play_flag;
	uint8_t* frame;
	size_t size;
	uint32_t delay;
	int64_t frame_ts, now_ts, tick_ts;

	frame = (uint8_t*)malloc(FRAME_SIZE * sizeof(uint8_t));
	size = FRAME_SIZE;

	tick_ts = GET_SYS_MS();
	delay = 0;
	packet_count = 0;
	play_flag = 0;

	while (run){

		if (main_queue.size() > 0){
			msg = main_queue.front();
			main_queue.pop_front();

			switch (msg.msg_id){
			case el_start_play:
				printf("start play, player = %u!!!\n", msg.val);
				play_flag = 1;
				break;

			case el_stop_play:
				printf("stop play, player = %u!!\n", msg.val);
				play_flag = 0;
				run = 0;
				break;
			}
		}

		su_sleep(0, 10000);

		/*收视频模拟的频数据*/
		if (play_flag == 1){
			frame_ts = play_video(frame, size);

			/*进行数据合法性校验*/
			now_ts = GET_SYS_MS();
			if (now_ts >= frame_ts && frame_ts > 0){
				delay += (uint32_t)(now_ts - frame_ts);

				if (tick_ts + 10 * 1000 < now_ts){
					printf("%u\n", delay / packet_count);
					packet_count = 0;
					delay = 0;
					tick_ts = now_ts;
				}
			}
		}
	}

	free(frame);
}

int main(int argc, const char* argv[])
{
	srand((uint32_t)time(NULL));

	if (open_win_log("receiver.log") != 0){
		assert(0);
		return -1;
	}

	main_mutex = su_create_mutex();

	sim_init(16001, log_win_write, notify_callback, notify_change_bitrate, notify_state);

	main_loop_event();

	sim_destroy();
	su_destroy_mutex(main_mutex);
	close_win_log();

	return 0;
}
