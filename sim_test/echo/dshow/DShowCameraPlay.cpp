/*-
* Copyright (c) 2017-2018 Razor, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/
#include <atlcomcli.h>
#include <Windows.h>
#include <InitGuid.h>
#include <streams.h>
#include <dvdmedia.h>

#include "DShowCameraPlay.h"


// {A1E8855F-2261-4327-8753-9C44E3FDB8A5}
DEFINE_GUID(IID_IAuthorize, 
	0xa1e8855f, 0x2261, 0x4327, 0x87, 0x53, 0x9c, 0x44, 0xe3, 0xfd, 0xb8, 0xa5);

//DEFINE_GUID(IID_ISampleGrabber,
//	0x6B652FFF, 0x11FE, 0x4fce, 0x92, 0xAD, 0x02, 0x66, 0xB5, 0xD7, 0xC7, 0x8F);
//const IID IID_ISampleGrabber = { 0x6B652FFF, 0x11FE, 0x4fce, { 0x92, 0xAD, 0x02, 0x66, 0xB5, 0xD7, 0xC7, 0x8F } };
interface IAuthorize : public IUnknown
{
public:
	virtual void SetAuthorize(wchar_t *code) = 0;
};


using namespace ds;


CameraPlay_Graph::CameraPlay_Graph()
{
	m_mediatype = GUID_NULL;
	m_renderGUID = GUID_NULL;

	m_nWidth = 320;
	m_nHeight = 240;
}

CameraPlay_Graph::~CameraPlay_Graph()
{
}

HRESULT CameraPlay_Graph::InitInstance(CComPtr<IBaseFilter> pSourceVideo, GUID mediatype, GUID renderGUID, int nWidth, int nHeight)
{
	HRESULT hr = E_FAIL;

	hr = Graph::InitInstance();
	if (FAILED(hr))
		return hr;

	StopGraph();
	ClearGraph();

	m_pCaptureGraph.CoCreateInstance( CLSID_CaptureGraphBuilder2 );
	if ( !m_pCaptureGraph )
		return hr;

	hr = m_pCaptureGraph->SetFiltergraph(m_pGraph);
	if (FAILED(hr))
		return hr;

	m_pSourceVideo = pSourceVideo;
	m_mediatype = mediatype;
	m_renderGUID = renderGUID;

	m_nWidth = nWidth;
	m_nHeight = nHeight;

	hr = BuildGraph();
	if (FAILED(hr))
		return hr;

	return hr;
}

HRESULT CameraPlay_Graph::ClearGraph()
{
	HRESULT hr = E_FAIL;

	m_pCaptureGraph = NULL;
	m_pSourceVideo = NULL;
	m_pPictureGrabberFilter = NULL;
	m_pStreamConfig = NULL;
	m_pPictureGrabber = NULL;

	return Graph::ClearGraph();
}

HRESULT CameraPlay_Graph::BuildGraph()
{
	HRESULT hr = E_FAIL;

	PIN_DIRECTION direction;

	hr = m_pGraph->AddFilter(m_pSourceVideo, L"");
	if (FAILED(hr))
		return hr;

	hr = m_pCaptureGraph->FindInterface(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Interleaved, 
		m_pSourceVideo, IID_IAMStreamConfig, (void **)&m_pStreamConfig);
	if (hr != NOERROR)
	{
		hr = m_pCaptureGraph->FindInterface(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, 
			m_pSourceVideo, IID_IAMStreamConfig, (void **)&m_pStreamConfig);
	}
	if (FAILED(hr))
		return hr;

	// Prefer 320x240
	AM_MEDIA_TYPE *pmt = NULL;
	int iCount = 0, iSize = 0;
	hr = m_pStreamConfig->GetNumberOfCapabilities(&iCount, &iSize);
	if (FAILED(hr))
		return hr;
	if (iSize == sizeof(VIDEO_STREAM_CONFIG_CAPS))
	{
		for (int iFormat = 0; iFormat < iCount; iFormat++)
		{
			VIDEO_STREAM_CONFIG_CAPS scc;
			AM_MEDIA_TYPE *pmtConfig;
			hr = m_pStreamConfig->GetStreamCaps(iFormat, &pmtConfig, (BYTE*)&scc);
			if (SUCCEEDED(hr))
			{
				LONG nWidth = 0;
				LONG nHeight = 0;
				if (pmtConfig->formattype == FORMAT_DvInfo)
				{
					DVINFO *pInfo = (DVINFO *)pmtConfig->pbFormat;
				}
				else if (pmtConfig->formattype == FORMAT_MPEGVideo)
				{
					MPEG1VIDEOINFO *pInfo = (MPEG1VIDEOINFO *)pmtConfig->pbFormat;
					nWidth = pInfo->hdr.bmiHeader.biWidth;
					nHeight = pInfo->hdr.bmiHeader.biHeight;
				}
				else if (pmtConfig->formattype == FORMAT_MPEG2Video)
				{
					MPEG2VIDEOINFO *pInfo = (MPEG2VIDEOINFO *)pmtConfig->pbFormat;
					nWidth = pInfo->hdr.bmiHeader.biWidth;
					nHeight = pInfo->hdr.bmiHeader.biHeight;
				}
				else if (pmtConfig->formattype == FORMAT_VideoInfo)
				{
					VIDEOINFOHEADER *pInfo = (VIDEOINFOHEADER *)pmtConfig->pbFormat;
					nWidth = pInfo->bmiHeader.biWidth;
					nHeight = pInfo->bmiHeader.biHeight;
				}
				else if (pmtConfig->formattype == FORMAT_VideoInfo2)
				{
					VIDEOINFOHEADER2 *pInfo = (VIDEOINFOHEADER2 *)pmtConfig->pbFormat;
					nWidth = pInfo->bmiHeader.biWidth;
					nHeight = pInfo->bmiHeader.biHeight;
				}

				if ((nWidth == m_nWidth) && (nHeight == m_nHeight))
				{
					pmt = pmtConfig;
					break;
				}

				DeleteMediaType(pmtConfig);
			}
		}
	}
	if (pmt != NULL)
	{
		hr = m_pStreamConfig->SetFormat(pmt);
		DeleteMediaType(pmt);
		if (FAILED(hr))
			return hr;
	}


	std::vector<CComPtr<IPin> > sourcePins;
	CComPtr<IPin> pSourceOutputpin;
	hr = DS_Utils::EnumPins(m_pSourceVideo, sourcePins);
	if (FAILED(hr))
		return hr;
	for (int i = 0; i < sourcePins.size(); i++)
	{
		CComPtr<IPin> pPin = sourcePins[i];
		hr = pPin->QueryDirection(&direction);
		if (FAILED(hr))
			return hr;
		if (direction == PINDIR_OUTPUT)
		{
			GUID pinCategory;
			if (SUCCEEDED(DS_Utils::GetPinCategory(pPin, pinCategory)))
			{
				if (pinCategory == PIN_CATEGORY_CAPTURE)
				{
					pSourceOutputpin = pPin;
					break;
				}
			}
		}
	}
	if (pSourceOutputpin == NULL)
		return E_FAIL;


	m_pPictureGrabberFilter.CoCreateInstance( CLSID_SampleGrabber );
	if ( !m_pPictureGrabberFilter )
		return E_FAIL;

	m_pPictureGrabber = CComQIPtr< ISampleGrabber, &IID_ISampleGrabber > ( m_pPictureGrabberFilter );
	if (m_pPictureGrabber == NULL)
		return E_FAIL;

	hr = m_pGraph->AddFilter(m_pPictureGrabberFilter, L"");
	if (FAILED(hr))
		return hr;

	if (m_mediatype != GUID_NULL)
	{
		AM_MEDIA_TYPE mt;
		memset(&mt, 0, sizeof(mt));
		mt.majortype = MEDIATYPE_Video;
		mt.subtype = m_mediatype;
		mt.bFixedSizeSamples = TRUE;
		mt.formattype = GUID_NULL;
		hr = m_pPictureGrabber->SetMediaType(&mt);
		if (FAILED(hr))
			return hr;
	}


	std::vector<CComPtr<IPin> > grabberPins;
	CComPtr<IPin> pGrabberInputpin;
	CComPtr<IPin> pGrabberOutputpin;
	hr = DS_Utils::EnumPins(m_pPictureGrabberFilter, grabberPins);
	if (FAILED(hr))
		return hr;
	if (grabberPins.size() != 2)
		return E_FAIL;
	hr = grabberPins[0]->QueryDirection(&direction);
	if (FAILED(hr))
		return hr;
	if (direction == PINDIR_INPUT)
	{
		pGrabberInputpin = grabberPins[0];
		pGrabberOutputpin = grabberPins[1];
	}else
	{
		pGrabberInputpin = grabberPins[1];
		pGrabberOutputpin = grabberPins[0];
	}

	hr = m_pGraph->Connect(pSourceOutputpin, pGrabberInputpin);
	if (FAILED(hr)) 
		return hr;

	if (m_renderGUID == GUID_NULL)
	{
		hr = m_pGraph->Render(pGrabberOutputpin);
		if (FAILED(hr))
			return hr;
	}
	else
	{
		CComPtr<IBaseFilter> renderFilter;
		renderFilter.CoCreateInstance( m_renderGUID );
		if ( !renderFilter )
			return E_FAIL;

		hr = m_pGraph->AddFilter(renderFilter, L"");
		if (FAILED(hr))
			return hr;

		CComPtr<IAuthorize> pAuthorize;
		pAuthorize = CComQIPtr< IAuthorize, &IID_IAuthorize > ( renderFilter );
		if (pAuthorize != NULL)
			pAuthorize->SetAuthorize(L"www.51.com.");

		std::vector<CComPtr<IPin> > renderPins;
		CComPtr<IPin> pRenderInputpin;
		hr = DS_Utils::EnumPins(renderFilter, renderPins);
		if (FAILED(hr))
			return hr;
		if (renderPins.size() != 1)
			return E_FAIL;
		hr = renderPins[0]->QueryDirection(&direction);
		if (FAILED(hr))
			return hr;
		if (direction == PINDIR_OUTPUT)
			return E_FAIL;
		pRenderInputpin = renderPins[0];

		hr = m_pGraph->Connect(pGrabberOutputpin, pRenderInputpin);
		if (FAILED(hr))
			return hr;
	}

	return hr;
}
