/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#include "record_thread.h"
#include "sim_external.h"

VideoRecordhread::VideoRecordhread()
{

}

VideoRecordhread::~VideoRecordhread()
{

}

void VideoRecordhread::set_video_devices(CFVideoRecorder* rec)
{
	rec_ = rec;
}

void VideoRecordhread::run()
{
	uint8_t *data;
	uint8_t payload_type;
	int rc = MAX_PIC_SIZE, size;
	int key;

	data = (uint8_t*)malloc(rc * sizeof(uint8_t));

	while (m_run_flag){
		if (rec_ != NULL){
			size = rec_->read(data, rc, key, payload_type);
			if (size > 0)
				sim_send_video(payload_type, key, data, size);
		}

		Sleep(1);
	}

	free(data);
	m_run_flag = true;
}