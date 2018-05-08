/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/


#ifndef _DSHOW_GRABBER_CB_H_
#define _DSHOW_GRABBER_CB_H_

#include <memory>
#include <qedit.h>
using namespace std::tr1;


struct SGrabberSample
{
	double SampleTime;
	shared_ptr<BYTE> pBuffer;
	long BufferLen;

	SGrabberSample()
	{
		SampleTime= 0;
		BufferLen = 0;
	}
	SGrabberSample(const SGrabberSample &rhs)
	{
		SampleTime = rhs.SampleTime;
		pBuffer = rhs.pBuffer;
		BufferLen = rhs.BufferLen;
	}
};

typedef void (*FN_BufferCB)(SGrabberSample sample);


class CGrabberCB : public ISampleGrabberCB
{
public:
	CGrabberCB(FN_BufferCB callback);
	~CGrabberCB();

	// fake out any COM ref counting
	STDMETHODIMP_(ULONG) AddRef() { return 2; }
	STDMETHODIMP_(ULONG) Release() { return 1; }

	// fake out any COM QI'ing
	STDMETHODIMP QueryInterface(REFIID riid, void **ppv)
	{
		if ((riid == IID_ISampleGrabberCB) || (riid == IID_IUnknown))
		{
			*ppv = (void *) static_cast<ISampleGrabberCB *> (this);
			return NOERROR;
		}
		return E_NOINTERFACE;
	}

	// We don't implement this interface for this example
	STDMETHODIMP SampleCB(double SampleTime, IMediaSample *pSample)
	{
		return 0;
	}

	STDMETHODIMP BufferCB(double SampleTime, BYTE *pBuffer, long BufferLen);

private:
	FN_BufferCB m_callback;
};


#endif
