/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#include "cf_platform.h"
#include "cf_list.h"
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

static base_list_t* main_queue = NULL;
su_mutex main_mutex;
static char g_info[1024] = { 0 };

static void notify_callback(void* event, int type, uint32_t val)
{
	thread_msg_t* msg = (thread_msg_t*)calloc(1, sizeof(thread_msg_t));
	msg->msg_id = el_unknown;

	switch (type){
	case sim_start_play_notify:
		msg->msg_id = el_start_play;
		msg->val = val;
		break;

	case sim_stop_play_notify:
		msg->msg_id = el_stop_play;
		msg->val = val;
		break;

	default:
		return;
	}

	su_mutex_lock(main_mutex);
	list_push(main_queue, msg);
	su_mutex_unlock(main_mutex);
}

static void notify_change_bitrate(void* event, uint32_t bitrate_kbps, int lost)
{
}

static void notify_state(void* event, const char* info)
{
	strcpy(g_info, info);
}

static int64_t play_video(uint8_t* video_frame, size_t size)
{
	int ret, count;
	uint8_t* pos, payload_type;
	size_t frame_size;
	int64_t frame_ts;
	 
	payload_type = 0;
	ret = sim_recv_video(video_frame, &size, &payload_type);
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

	/*printf("frame id = %d, frame_size = %d, frame_ts = %lld\n", count, frame_size, frame_ts);*/

	return frame_ts;
}

#define FRAME_SIZE (1024 * 1024 * 10)

static void main_loop_event()
{
	thread_msg_t* msg = NULL;
	int run = 1, packet_count, play_flag;
	uint8_t* frame;
	size_t size;
	uint32_t delay, max_delay;
	int64_t frame_ts, now_ts, tick_ts, msg_ts;

	frame = (uint8_t*)malloc(FRAME_SIZE * sizeof(uint8_t));
	size = FRAME_SIZE;

	msg_ts = tick_ts = GET_SYS_MS();
	delay = 0;
	max_delay = 0;
	packet_count = 0;
	play_flag = 0;

	while (run){

		now_ts = GET_SYS_MS();
		if (now_ts > 50 + msg_ts){
			msg_ts = now_ts;
			su_mutex_lock(main_mutex);

			if (list_size(main_queue) > 0){
				msg = (thread_msg_t*)list_front(main_queue);
				list_pop(main_queue);

				switch (msg->msg_id){
				case el_start_play:
					printf("start play, player = %u!!!\n", msg->val);
					play_flag = 1;
					break;

				case el_stop_play:
					printf("stop play, player = %u!!\n", msg->val);
					play_flag = 0;
					run = 0;
					break;
				}
			}

			su_mutex_unlock(main_mutex);
		}

		/*收视频模拟的频数据*/
		if (play_flag == 1){
			frame_ts = play_video(frame, size);

			/*进行数据合法性校验*/
			if (now_ts >= frame_ts && frame_ts > 0){
				delay = (uint32_t)(now_ts - frame_ts);

				max_delay = SU_MAX(delay, max_delay);
				packet_count++;
				if (tick_ts + 1000 < now_ts){

					printf("max_delay = %ums, %s\n", max_delay, g_info);
					packet_count = 0;
					max_delay = 0;
					tick_ts = now_ts;
				}
			}
			else if (frame_ts == -1)
				su_sleep(0, 5000);
		}
		else {
			su_sleep(0, 5000);
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
	main_queue = create_list();

	sim_init(16001, NULL, log_win_write, notify_callback, notify_change_bitrate, notify_state);

	main_loop_event();

	sim_destroy();
	destroy_list(main_queue);
	su_destroy_mutex(main_mutex);
	close_win_log();

	return 0;
}
