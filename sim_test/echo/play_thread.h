/*-
* Copyright (c) 2017-2018 Razor, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#ifndef __play_thread_h_
#define __play_thread_h_

#include "win32_thread.h"
#include "video_device.h"

class VideoPlayhread : public CWin32_Thread
{
public:
	VideoPlayhread();
	~VideoPlayhread();

	void set_video_devices(CFVideoPlayer* play);

private:
	void run();

	CFVideoPlayer* play_;
};

#endif



