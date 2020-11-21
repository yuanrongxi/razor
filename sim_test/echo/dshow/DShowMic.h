/*-
* Copyright (c) 2017-2018 Razor, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/


#ifndef _DSHOW_MIC_H_
#define _DSHOW_MIC_H_


#include "DShowUtils.h"

#include <qedit.h>


#define _DS_BEGIN	namespace ds {
#define _DS_END		}
#define _DS	::ds::

_DS_BEGIN


class Mic_Graph : public Graph
{
public:
	Mic_Graph();
	~Mic_Graph();

	HRESULT InitInstance(CComPtr<IBaseFilter> pSourceAudio, DWORD nSamplesPerSec = 44100, WORD nChannels = 2, WORD wBitsPerSample = 16);

	CComPtr<ISampleGrabber> GetAudioGrabber() { return m_pAudioGrabber; }

protected:
	virtual HRESULT ClearGraph();
	virtual HRESULT BuildGraph();

protected:
	CComPtr<IBaseFilter> m_pSourceAudio;

	CComPtr<ISampleGrabber> m_pAudioGrabber;

	DWORD m_nSamplesPerSec;
	WORD m_nChannels;
	WORD m_wBitsPerSample;
};


_DS_END
#endif
