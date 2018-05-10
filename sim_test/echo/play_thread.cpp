/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#include "play_thread.h"
#include "sim_external.h"

VideoPlayhread::VideoPlayhread()
{

}

VideoPlayhread::~VideoPlayhread()
{

}

void VideoPlayhread::set_video_devices(CFVideoPlayer* play)
{
	play_ = play;
}

#define MAX_PIC_SIZE 1024000
void VideoPlayhread::run()
{
	uint8_t *data;
	uint8_t payload_type;

	data = (uint8_t*)malloc(MAX_PIC_SIZE * sizeof(uint8_t));

	while (m_run_flag){
		size_t rc = MAX_PIC_SIZE;
		if (sim_recv_video(data, &rc, &payload_type) == 0 && play_ != NULL){
			play_->write(data, rc, payload_type);
		}
		else
			Sleep(5);
	}

	free(data);
	m_run_flag = true;
}


