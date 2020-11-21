/*-
* Copyright (c) 2017-2018 Razor, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#ifndef __record_thread_h__
#define __record_thread_h__

#include "win32_thread.h"
#include "video_device.h"

class VideoRecordhread : public CWin32_Thread
{
public:
	VideoRecordhread();
	~VideoRecordhread();

	void set_video_devices(CFVideoRecorder* rec);

private:
	void run();

	CFVideoRecorder* rec_;
};


#endif



