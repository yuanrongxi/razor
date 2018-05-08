/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#ifndef __WIN32_THREAD_H_
#define __WIN32_THREAD_H_

#include <windows.h>

class CWin32_Thread
{
public:
	CWin32_Thread();
	virtual ~CWin32_Thread();

	int start();
	void stop();

protected:	
	static DWORD WINAPI thread_work(LPVOID lpData);
	virtual void run() = 0;

	bool m_run_flag;

	HANDLE m_thread_handle;
};

#endif
