/*-
* Copyright (c) 2017-2018 Razor, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/


#ifndef _DSHOW_UTILS_H_
#define _DSHOW_UTILS_H_

#include <atlcomcli.h>
#include <string>
#include <vector>
#include <list>

#define _DS_BEGIN	namespace ds {
#define _DS_END		}
#define _DS	::ds::

_DS_BEGIN



struct SCaptureDevice
{
	std::wstring FriendlyName;
	std::wstring Description;
	std::wstring DevicePath;
	bool bVideo;
	GUID guid;
	CComPtr<IBaseFilter> pFilter;
};

struct SEffectInfo
{
	std::wstring FriendlyName;
	GUID guid;
};


class DS_Utils
{
public:
	static HRESULT EnumAllCaptureDevice(std::vector<SCaptureDevice> &capDev);
	static HRESULT EnumCaptureDevice(std::vector<SCaptureDevice> &capDev, bool bVideo = true);

	static HRESULT EnumGraphFilters(CComPtr<IGraphBuilder> pGraph, std::vector<CComPtr<IBaseFilter> > &filters);

	static HRESULT EnumPins(CComPtr<IBaseFilter> pFilter, std::vector<CComPtr<IPin> > &pins);

	static HRESULT FindFilter(CComPtr<IGraphBuilder> pGraph, CLSID classid, CComPtr<IBaseFilter> &pFilter);

	static bool IsPinConnected(CComPtr<IPin> pPin, CComPtr<IPin> &pPinConnected);

	static HRESULT EnumEffect(CLSID classid, std::vector<SEffectInfo> &effects);

	static HRESULT SetCaptureFilter_FriendlyName(CComPtr<IBaseFilter> pFilter, std::wstring name);

	static HRESULT ShowCaptureFilterPropPage(CComPtr<IBaseFilter> pFilter, HWND hWndParent, UINT x, UINT y);

	static HRESULT GetPinCategory(CComPtr<IPin> pPin, GUID &pinCategory);
};

class Graph
{
public:
	Graph();
	virtual ~Graph();

	HRESULT InitInstance();
	HRESULT SetRenderWindow(HWND hWnd);
	HRESULT SetNotifyWindow(HWND hWnd, UINT message);

	HRESULT RunGraph();
	HRESULT PauseGraph();
	HRESULT StopGraph();
	virtual HRESULT ClearGraph();

	HRESULT SeekToFirst();
	HRESULT WaitForCompletion(long *pEvCode);

	HRESULT AudioMute();
	HRESULT AudioRestore();

	HRESULT VideoFullScreen();
	HRESULT VideoUnfullScreen();

	// 
	CComPtr<IMediaControl> GetMediaControl() { return m_pControl; }
	CComPtr<IMediaEventEx> GetMediaEvent() { return m_pEventEx; }
	CComPtr<IMediaSeeking> GetMediaSeeking() { return m_pSeeking; }
	CComPtr<IMediaPosition> GetMediaPosition() { return m_pPosition; }
	CComPtr<IBasicVideo> GetBasicVideo() { return m_pBasicVideo; }
	CComPtr<IBasicAudio> GetBasicAudio() { return m_pBasicAudio; }
	CComPtr<IVideoWindow> GetVideoWindow() { return m_pVideoWindow; }

protected:
	// Graph Building
	virtual HRESULT BuildGraph() = 0;

protected:
	CComPtr<IGraphBuilder>	m_pGraph;
	CComPtr<IMediaControl>	m_pControl;
	CComPtr<IMediaEventEx>	m_pEventEx;
	CComPtr<IMediaSeeking>	m_pSeeking;
	CComPtr<IMediaPosition>	m_pPosition;
	CComPtr<IBasicVideo>	m_pBasicVideo;
	CComPtr<IBasicAudio>	m_pBasicAudio;
	CComPtr<IVideoWindow>	m_pVideoWindow;

	long m_nAudioVolume;
};



_DS_END
#endif
