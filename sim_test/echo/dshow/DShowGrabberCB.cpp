/*-
* Copyright (c) 2017-2018 Razor, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/


#include <Windows.h>
#include <InitGuid.h>
#include <streams.h>

#include "DShowGrabberCB.h"
#include <stdio.h>


CGrabberCB::CGrabberCB(FN_BufferCB callback)
{
	m_callback = callback;
}

CGrabberCB::~CGrabberCB()
{
	m_callback = NULL;
}

STDMETHODIMP CGrabberCB::BufferCB(double SampleTime, BYTE *pBuffer, long BufferLen)
{
	if (!pBuffer)
		return E_POINTER;

	if (IsBadReadPtr(pBuffer, BufferLen))
		return E_FAIL;

	if (m_callback == NULL)
		return E_FAIL;

	SGrabberSample sample;
	sample.SampleTime = SampleTime;
	sample.pBuffer = std::auto_ptr<BYTE>(new BYTE [BufferLen]);
	sample.BufferLen = BufferLen;
	memcpy(sample.pBuffer.get(), pBuffer, BufferLen);

	m_callback(sample);

	return S_OK;
}
