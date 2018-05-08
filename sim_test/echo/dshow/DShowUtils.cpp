/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#include <Windows.h>
#include <streams.h>

#include <qedit.h>

#include "DShowUtils.h"

using namespace ds;


HRESULT DS_Utils::EnumAllCaptureDevice(std::vector<SCaptureDevice> &capDev)
{
	HRESULT hr = E_FAIL;

	capDev.clear();

	std::vector<SCaptureDevice> capDevVideo;
	std::vector<SCaptureDevice> capDevAudio;

	hr = EnumCaptureDevice(capDevVideo, true);
	if (FAILED(hr))
		return hr;

	hr = EnumCaptureDevice(capDevAudio, false);
	if (FAILED(hr))
		return hr;

	for (DWORD i = 0; i < capDevVideo.size(); i++)
		capDev.push_back(capDevVideo[i]);

	for (DWORD i = 0; i < capDevAudio.size(); i++)
		capDev.push_back(capDevAudio[i]);

	return S_OK;
}

HRESULT DS_Utils::EnumCaptureDevice(std::vector<SCaptureDevice> &capDev, bool bVideo)
{
	HRESULT hr = E_FAIL;

	capDev.clear();

	CComPtr<ICreateDevEnum> pDevEnum;
	CComPtr<IEnumMoniker> pEnum;

	pDevEnum.CoCreateInstance( CLSID_SystemDeviceEnum );
	if ( !pDevEnum )
		return hr;

	// Video/Audio
	if (bVideo)
		hr = pDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pEnum, 0);
	else
		hr = pDevEnum->CreateClassEnumerator(CLSID_AudioInputDeviceCategory, &pEnum, 0);
	if (FAILED(hr))
		return hr;

	if(pEnum == NULL)
		return S_FALSE;

	CComPtr<IMoniker> pMoniker;
	while (pEnum->Next(1, &pMoniker, NULL) == S_OK)
	{
		CComPtr<IPropertyBag> pPropBag;
		hr = pMoniker->BindToStorage(0, 0, IID_IPropertyBag, 
			(void**)(&pPropBag));
		if (FAILED(hr))
		{
			pMoniker.Release();
			continue;  // Skip this one, maybe the next one will work.
		} 

		VARIANT varNameDescription;
		VariantInit(&varNameDescription);

		VARIANT varNameFriendly;
		VariantInit(&varNameFriendly);

		VARIANT varNamePath;
		VariantInit(&varNamePath);

		SCaptureDevice cd;

		hr = pPropBag->Read(L"Description", &varNameDescription, 0);
		if (SUCCEEDED(hr))
			cd.Description = varNameDescription.bstrVal;
		hr = pPropBag->Read(L"FriendlyName", &varNameFriendly, 0);
		if (SUCCEEDED(hr))
			cd.FriendlyName = varNameFriendly.bstrVal;
		hr = pPropBag->Read(L"DevicePath", &varNamePath, 0);
		if (SUCCEEDED(hr))
			cd.DevicePath = varNamePath.bstrVal;

		CComPtr<IBaseFilter> pCap;
		try{
			hr = pMoniker->BindToObject(0, 0, IID_IBaseFilter, (void**)&pCap);
			if (SUCCEEDED(hr))
			{
				GUID guid = GUID_NULL;
				pCap->GetClassID(&guid);

				cd.bVideo = bVideo;
				cd.guid = guid;
				cd.pFilter = pCap;
				capDev.push_back(cd);
			}
		}
		catch(...)
		{

		}

		pPropBag.Release();
		pMoniker.Release();
	}

	return S_OK;
}

HRESULT DS_Utils::EnumGraphFilters(CComPtr<IGraphBuilder> pGraph, std::vector<CComPtr<IBaseFilter> > &filters)
{
	HRESULT hr = E_FAIL;
	CComPtr<IEnumFilters> pEnum;
	CComPtr<IBaseFilter> pFilter;

	filters.clear();

	hr = pGraph->EnumFilters(&pEnum);
	if (FAILED(hr))
		return hr;

	ULONG cFetched;
	while (pEnum->Next(1, &pFilter, &cFetched) == S_OK)
	{
		//FILTER_INFO FilterInfo;
		//hr = pFilter->QueryFilterInfo(&FilterInfo);
		//if (FAILED(hr))
		//{
		//	// "Could not get the filter info"
		//	continue;  // Maybe the next one will work.
		//}
		//// The FILTER_INFO structure holds a pointer to the Filter Graph
		//// Manager, with a reference count that must be released.
		//if (FilterInfo.pGraph != NULL)
		//{
		//	FilterInfo.pGraph->Release();
		//}

		filters.push_back(pFilter);

		pFilter.Release();
	}

	pEnum.Release();

	return S_OK;
}

HRESULT DS_Utils::EnumPins(CComPtr<IBaseFilter> pFilter, std::vector<CComPtr<IPin> > &pins)
{
	HRESULT hr = E_FAIL;
	CComPtr<IEnumPins> pEnum;
	CComPtr<IPin> pPin;

	pins.clear();

	hr = pFilter->EnumPins(&pEnum);
	if (FAILED(hr))
		return hr;

	while (pEnum->Next(1, &pPin, 0) == S_OK)
	{
		pins.push_back(pPin);
		pPin.Release();
	}

	pEnum.Release();
	return S_OK;
}

HRESULT DS_Utils::FindFilter(CComPtr<IGraphBuilder> pGraph, CLSID classid, CComPtr<IBaseFilter> &pFilter)
{
	HRESULT hr = E_FAIL;

	std::vector<CComPtr<IBaseFilter> > filters;
	hr = EnumGraphFilters(pGraph, filters);
	if (FAILED(hr))
		return hr;

	for (DWORD i = 0; i < filters.size(); i++)
	{
		CComPtr<IBaseFilter> &pFilterTmp = filters[i];

		if (pFilterTmp)
		{
			CLSID clsid;
			hr = pFilterTmp->GetClassID(&clsid);
			if (FAILED(hr))
				return hr;

			if (classid == clsid)
			{
				pFilter = pFilterTmp;
				return S_OK;
			}
		}
	}

	return S_FALSE;
}

bool DS_Utils::IsPinConnected(CComPtr<IPin> pPin, CComPtr<IPin> &pPinConnected)
{
	HRESULT hr = E_FAIL;
	bool bConnected;

	hr = pPin->ConnectedTo(&pPinConnected);
	if (hr == S_OK)
		bConnected = true;
	else
		bConnected = false;

	return bConnected;
}

HRESULT DS_Utils::EnumEffect(CLSID classid, std::vector<SEffectInfo> &effects)
{
	HRESULT hr = E_FAIL;

	effects.clear();

	CComPtr<ICreateDevEnum> pCreateDevEnum;
	CComPtr<IEnumMoniker> pEnumMoniker;

	hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL, 
		CLSCTX_INPROC_SERVER, IID_ICreateDevEnum, (void**)&pCreateDevEnum);
	if (FAILED(hr))
		return E_FAIL;

	hr = pCreateDevEnum->CreateClassEnumerator(classid, &pEnumMoniker, 0);
	if (hr == S_OK)  // S_FALSE means the category is empty.
	{
		CComPtr<IMoniker> pMoniker;
		while (S_OK == pEnumMoniker->Next(1, &pMoniker, NULL))
		{
			CComPtr<IPropertyBag> pBag;
			hr = pMoniker->BindToStorage(0, 0, IID_IPropertyBag, (void **)&pBag);
			if (FAILED(hr))
			{
				pMoniker.Release();
				continue;
			}
			VARIANT var;
			VariantInit(&var);
			hr = pBag->Read(OLESTR("FriendlyName"), &var, NULL);
			if (SUCCEEDED(hr))
			{
				VARIANT var2;
				GUID guid;
				var2.vt = VT_BSTR;
				pBag->Read(OLESTR("guid"), &var2, NULL);
				CLSIDFromString(var2.bstrVal, &guid);
				VariantClear(&var2);

				SEffectInfo effect;
				effect.FriendlyName = V_BSTR(&var);
				effect.guid = guid;
				effects.push_back(effect);
			}
			VariantClear(&var);
			pBag.Release();
			pMoniker.Release();
		}
		pEnumMoniker.Release();
	}
	pCreateDevEnum.Release();

	return hr;
}

HRESULT DS_Utils::SetCaptureFilter_FriendlyName(CComPtr<IBaseFilter> pFilter, std::wstring name)
{
	HRESULT hr = E_FAIL;
	BOOL bFound = FALSE;

	GUID guidFilter = GUID_NULL;
	hr = pFilter->GetClassID(&guidFilter);
	if (FAILED(hr))
		return hr;

	CComPtr<ICreateDevEnum> pDevEnum;
	CComPtr<IEnumMoniker> pEnum;

	pDevEnum.CoCreateInstance( CLSID_SystemDeviceEnum );
	if ( !pDevEnum )
		return hr;

	// Video
	hr = pDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pEnum, 0);
	if (FAILED(hr))
		return hr;

	CComPtr<IMoniker> pMoniker;
	while (pEnum->Next(1, &pMoniker, NULL) == S_OK)
	{
		CComPtr<IPropertyBag> pPropBag;
		hr = pMoniker->BindToStorage(0, 0, IID_IPropertyBag, 
			(void**)(&pPropBag));
		if (FAILED(hr))
		{
			pMoniker.Release();
			continue;  // Skip this one, maybe the next one will work.
		} 

		CComPtr<IBaseFilter> pCap;
		hr = pMoniker->BindToObject(0, 0, IID_IBaseFilter, (void**)&pCap);
		if (SUCCEEDED(hr))
		{
			GUID guid = GUID_NULL;
			hr = pCap->GetClassID(&guid);
			if (FAILED(hr))
			{
				pPropBag.Release();
				pMoniker.Release();
				return hr;
			}

			if (guid == guidFilter)
			{
				bFound = TRUE;

				VARIANT varNameFriendly;
				VariantInit(&varNameFriendly);
				varNameFriendly.vt = VT_BSTR;
				varNameFriendly.bstrVal = SysAllocString(name.c_str());

				hr = pPropBag->Write(L"FriendlyName", &varNameFriendly);

				SysFreeString(varNameFriendly.bstrVal);

				pPropBag.Release();
				pMoniker.Release();
				break;
			}
		}

		pPropBag.Release();
		pMoniker.Release();
	}

	if (!bFound)
		return E_FAIL;

	return hr;
}

HRESULT DS_Utils::ShowCaptureFilterPropPage(CComPtr<IBaseFilter> pFilter, HWND hWndParent, UINT x, UINT y)
{
	HRESULT hr = E_FAIL;
	CComPtr<ISpecifyPropertyPages> pSpec;
	CAUUID cauuid;

	hr = pFilter->QueryInterface(IID_ISpecifyPropertyPages, (void **)&pSpec);
	if (hr == S_OK)
	{
		hr = pSpec->GetPages(&cauuid);
		hr = OleCreatePropertyFrame(hWndParent, x, y, NULL, 1,
			(IUnknown **)&pFilter, cauuid.cElems,
			(GUID *)cauuid.pElems, 0, 0, NULL);

		CoTaskMemFree(cauuid.pElems);
		pSpec.Release();
	}

	return hr;
}

HRESULT DS_Utils::GetPinCategory(CComPtr<IPin> pPin, GUID &pinCategory)
{
	HRESULT hr = E_FAIL;

	CComPtr<IKsPropertySet> pKs;
	hr = pPin->QueryInterface(IID_IKsPropertySet, (void **)&pKs);
	if (FAILED(hr))
		return hr;

	DWORD cbReturned;
	hr = pKs->Get(AMPROPSETID_Pin, AMPROPERTY_PIN_CATEGORY, NULL, 0, 
		&pinCategory, sizeof(GUID), &cbReturned);

	pKs.Release();
	return hr;
}



//////////////////////////////////////////////////////////////////////////

Graph::Graph()
{
	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

	m_nAudioVolume = 0;
}

Graph::~Graph()
{
	ClearGraph();

	CoUninitialize();
}

HRESULT Graph::InitInstance()
{
	HRESULT hr = E_FAIL;

	if (m_pGraph != NULL)
		return S_OK;

	m_pGraph.CoCreateInstance( CLSID_FilterGraph );
	if ( !m_pGraph )
		return hr;

	m_pControl = CComQIPtr< IMediaControl, &IID_IMediaControl > ( m_pGraph );
	if (m_pControl == NULL)
		return E_FAIL;

	m_pEventEx = CComQIPtr< IMediaEventEx, &IID_IMediaEventEx > ( m_pGraph );
	if (m_pEventEx == NULL)
		return E_FAIL;

	m_pSeeking = CComQIPtr< IMediaSeeking, &IID_IMediaSeeking > ( m_pGraph );
	if (m_pSeeking == NULL)
		return E_FAIL;

	m_pPosition = CComQIPtr< IMediaPosition, &IID_IMediaPosition > ( m_pGraph );
	if (m_pPosition == NULL)
		return E_FAIL;

	m_pBasicVideo = CComQIPtr< IBasicVideo, &IID_IBasicVideo > ( m_pGraph );
	if (m_pBasicVideo == NULL)
		return E_FAIL;

	m_pBasicAudio = CComQIPtr< IBasicAudio, &IID_IBasicAudio > ( m_pGraph );
	if (m_pBasicAudio == NULL)
		return E_FAIL;

	m_pVideoWindow = CComQIPtr< IVideoWindow, &IID_IVideoWindow > ( m_pGraph );
	if (m_pVideoWindow == NULL)
		return E_FAIL;

	return S_OK;
}

HRESULT Graph::SetRenderWindow(HWND hWnd)
{
	HRESULT hr = E_FAIL;

	if (m_pVideoWindow != NULL)
	{
		hr = m_pVideoWindow->put_Owner((OAHWND)hWnd);
		if (FAILED(hr))
			return hr;
		hr = m_pVideoWindow->put_WindowStyle(WS_CHILD | WS_CLIPSIBLINGS);
		if (FAILED(hr))
			return hr;

		RECT grc;
		::GetClientRect(hWnd, &grc);
		hr = m_pVideoWindow->SetWindowPosition(0, 0, grc.right, grc.bottom);
		if (FAILED(hr))
			return hr;
	}
	return hr;
}

HRESULT Graph::SetNotifyWindow(HWND hWnd, UINT message)
{
	HRESULT hr = E_FAIL;
	if (m_pEventEx != NULL)
		hr = m_pEventEx->SetNotifyWindow((OAHWND)hWnd, message, NULL);
	return hr;
}

HRESULT Graph::RunGraph()
{
	HRESULT hr = E_FAIL;
	if (m_pControl != NULL)
		hr = m_pControl->Run();
	return hr;
}

HRESULT Graph::PauseGraph()
{
	HRESULT hr = E_FAIL;
	if (m_pControl != NULL)
		hr = m_pControl->Pause();
	return hr;
}

HRESULT Graph::StopGraph()
{
	HRESULT hr = E_FAIL;
	if (m_pControl != NULL)
	{
		hr = m_pControl->Stop();
		if (FAILED(hr))
			return hr;
	}

	SeekToFirst();
	return hr;
}

HRESULT Graph::ClearGraph()
{
	HRESULT hr = E_FAIL;

	hr = StopGraph();
	if (FAILED(hr))
		return hr;

	if (m_pGraph)
	{
		CComPtr< IEnumFilters >  pEnum;
		hr = m_pGraph->EnumFilters(&pEnum);
		if (FAILED(hr))
			return hr;

		std::list< IBaseFilter * > listFilters;
		CComPtr< IBaseFilter >  pFilter;

		ULONG cFetched;
		while (pEnum->Next(1, &pFilter, &cFetched) == S_OK)
		{
			//hr = m_pGraph->RemoveFilter(pFilter);
			IBaseFilter *p = pFilter;
			p->AddRef();
			listFilters.push_back(p);
			pFilter.Release();

			if (FAILED(hr))
				return hr;
		}

		std::list< IBaseFilter * >::iterator i;
		for (i = listFilters.begin(); i != listFilters.end(); i++)
		{
			IBaseFilter *p = *i;
			hr = m_pGraph->RemoveFilter(p);
			p->Release();
			if (FAILED(hr))
				return hr;
		}
	}

	return hr;
}

HRESULT Graph::SeekToFirst()
{
	// Reset to first frame of movie

	HRESULT hr = E_FAIL;
	LONGLONG pos = 0;
	if (m_pSeeking != NULL)
		hr = m_pSeeking->SetPositions(&pos, AM_SEEKING_AbsolutePositioning,
		NULL, AM_SEEKING_NoPositioning);
	return hr;
}

HRESULT Graph::WaitForCompletion(long *pEvCode)
{
	HRESULT hr = E_FAIL;
	if (m_pEventEx != NULL)
		hr = m_pEventEx->WaitForCompletion(INFINITE, pEvCode);
	return hr;
}

HRESULT Graph::AudioMute()
{
	HRESULT hr;
	hr = m_pBasicAudio->get_Volume(&m_nAudioVolume);
	if (FAILED(hr))
		return hr;

	return m_pBasicAudio->put_Volume(-10000);
}

HRESULT Graph::AudioRestore()
{
	return m_pBasicAudio->put_Volume(m_nAudioVolume);
}

HRESULT Graph::VideoFullScreen()
{
	return m_pVideoWindow->put_FullScreenMode(OATRUE);
}

HRESULT Graph::VideoUnfullScreen()
{
	return m_pVideoWindow->put_FullScreenMode(OAFALSE);
}
