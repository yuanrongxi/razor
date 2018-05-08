/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#ifndef __video_view_h_
#define __video_view_h_

#include "win32_thread.h"
#include "video_device.h"


class VideoViewThread : public CWin32_Thread
{
public:
	VideoViewThread();
	~VideoViewThread();

	void set_video_devices(CFVideoRecorder* rec, CFVideoPlayer* player);

private:
	void run();

	CFVideoRecorder* rec_;
	CFVideoPlayer* player_;

};

#endif



