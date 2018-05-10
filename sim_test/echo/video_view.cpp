/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#include "video_view.h"

VideoViewThread::VideoViewThread()
{

}

VideoViewThread::~VideoViewThread()
{

}

void VideoViewThread::set_video_devices(CFVideoRecorder* rec, CFVideoPlayer* player)
{
	rec_ = rec;
	player_ = player;
}

void VideoViewThread::run()
{
	uint8_t *data;
	int rc = 1024000, size;
	int key;
	uint8_t payload_type;

	data = (uint8_t*)malloc(rc * sizeof(uint8_t));

	while (m_run_flag){
		size = rec_->read(data, rc, key, payload_type);
		if (size > 0 && player_ != NULL){
			player_->write(data, size, payload_type);
		}
		else
			Sleep(1);
	}

	//rec_->close();
	free(data);
	m_run_flag = true;
}




