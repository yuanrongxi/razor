/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#include "win32_thread.h"

CWin32_Thread::CWin32_Thread() : m_thread_handle(NULL), m_run_flag(false)
{
}

CWin32_Thread::~CWin32_Thread()
{
	stop();
}
int CWin32_Thread::start()
{
	if(m_thread_handle != NULL)
		return 1;

	m_run_flag = true;

	DWORD thread_id;
	m_thread_handle = CreateThread(NULL, 0, CWin32_Thread::thread_work, (LPVOID)this, 0, &thread_id);

	SetThreadPriority(m_thread_handle, THREAD_PRIORITY_HIGHEST);

	return (m_thread_handle != NULL ? 1 : 0);
}

void CWin32_Thread::stop()
{
	if(m_thread_handle == NULL)
		return;

	m_run_flag = false;

	int i = 0; 
	while(i < 20 && !m_run_flag)
	{
		Sleep(100);
		i ++;
	}

	TerminateThread(m_thread_handle, 0);
	CloseHandle(m_thread_handle);

	m_thread_handle = NULL;
}


DWORD CWin32_Thread::thread_work(LPVOID lpData)
{
	CWin32_Thread *body = (CWin32_Thread *)lpData;

	::CoInitialize(NULL);

	if(body != NULL)
	{
		body->run();

		return 1;
	}
	else
		return 0;

	::CoUninitialize();
}
