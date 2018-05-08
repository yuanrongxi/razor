/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/


#ifndef _DSHOW_CAMERA_PLAY_H_
#define _DSHOW_CAMERA_PLAY_H_

#include <qedit.h>

#include "DShowUtils.h"


#define MAX_VIDEO_FRAME_RATE 30

#define _DS_BEGIN	namespace ds {
#define _DS_END		}
#define _DS	::ds::

_DS_BEGIN


class CameraPlay_Graph : public Graph
{
public:
	CameraPlay_Graph();
	~CameraPlay_Graph();

	HRESULT InitInstance(CComPtr<IBaseFilter> pSourceVideo, GUID mediatype = GUID_NULL, GUID renderGUID = GUID_NULL, int nWidth = 320, int nHeight = 240);

	CComPtr<IAMStreamConfig> GetStreamConfig() { return m_pStreamConfig; }
	CComPtr<ISampleGrabber> GetPictureGrabber() { return m_pPictureGrabber; }


protected:
	virtual HRESULT ClearGraph();
	virtual HRESULT BuildGraph();

protected:
	CComPtr<ICaptureGraphBuilder2> m_pCaptureGraph;

	CComPtr<IBaseFilter> m_pSourceVideo;
	CComPtr<IBaseFilter> m_pPictureGrabberFilter;

	CComPtr<IAMStreamConfig> m_pStreamConfig;
	CComPtr<ISampleGrabber> m_pPictureGrabber;

	GUID m_mediatype;
	GUID m_renderGUID;

	int  m_nWidth;
	int  m_nHeight;

};


_DS_END
#endif
