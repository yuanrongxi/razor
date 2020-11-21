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

#include "DShowMic.h"
using namespace ds;


Mic_Graph::Mic_Graph()
{
}

Mic_Graph::~Mic_Graph()
{
}

HRESULT Mic_Graph::InitInstance(CComPtr<IBaseFilter> pSourceAudio, DWORD nSamplesPerSec, WORD nChannels, WORD wBitsPerSample)
{
	HRESULT hr = E_FAIL;

	hr = Graph::InitInstance();
	if (FAILED(hr))
		return hr;

	StopGraph();
	ClearGraph();

	m_pSourceAudio = pSourceAudio;
	m_nSamplesPerSec = nSamplesPerSec;
	m_nChannels = nChannels;
	m_wBitsPerSample = wBitsPerSample;

	hr = BuildGraph();

	return hr;
}

HRESULT Mic_Graph::ClearGraph()
{
	HRESULT hr = E_FAIL;

	m_pSourceAudio = NULL;
	m_pAudioGrabber = NULL;

	return Graph::ClearGraph();
}

HRESULT Mic_Graph::BuildGraph()
{
	HRESULT hr = E_FAIL;

	PIN_DIRECTION direction;

	//////////////////////////////////////////////////////////////////////////
	{
		//m_pSourceAudio = NULL;
		//m_pSourceAudio.CoCreateInstance( CLSID_AsyncReader );
		//if ( !m_pSourceAudio )
		//	return E_FAIL;
		//CComPtr<IFileSourceFilter> pSource;
		//pSource = CComQIPtr< IFileSourceFilter, &IID_IFileSourceFilter > ( m_pSourceAudio );
		//if (pSource == NULL)
		//	return E_FAIL;
		//hr = pSource->Load(L"E:\\mp3\\原声大碟.-.[毕业生.-.The.Graduate].专辑.(APE)\\graduate.wav", NULL);
	}
	//////////////////////////////////////////////////////////////////////////

	hr = m_pGraph->AddFilter(m_pSourceAudio, L"");
	if (FAILED(hr))
		return hr;

	CComPtr<IPin> pSourceOutputpin;
	{
		std::vector<CComPtr<IPin> > sourcePins;
		hr = DS_Utils::EnumPins(m_pSourceAudio, sourcePins);
		if (FAILED(hr))
			return hr;
		for (DWORD i = 0; i < sourcePins.size(); i++)
		{
			CComPtr<IPin> &pPin = sourcePins[i];
			hr = pPin->QueryDirection(&direction);
			if (FAILED(hr))
				return hr;
			if (direction == PINDIR_OUTPUT)
			{
				pSourceOutputpin = pPin;
				break;
			}
		}
	}
	if (!pSourceOutputpin)
		return E_FAIL;

	//////////////////////////////////////////////////////////////////////////
	// Add Filters

	// Sample Grabber
	CComPtr<IBaseFilter> pSampleGrabberFilter;
	pSampleGrabberFilter.CoCreateInstance( CLSID_SampleGrabber );
	if ( !pSampleGrabberFilter )
		return E_FAIL;

	m_pAudioGrabber = CComQIPtr< ISampleGrabber, &IID_ISampleGrabber > ( pSampleGrabberFilter );
	if (m_pAudioGrabber == NULL)
		return E_FAIL;

	hr = m_pGraph->AddFilter(pSampleGrabberFilter, L"");
	if (FAILED(hr))
		return hr;

	{
		WAVEFORMATEX wfx;
		memset(&wfx, 0, sizeof(wfx));
		wfx.wFormatTag = WAVE_FORMAT_PCM;
		wfx.nChannels = m_nChannels;
		wfx.wBitsPerSample = m_wBitsPerSample;
		wfx.nSamplesPerSec = m_nSamplesPerSec;
		wfx.nBlockAlign = wfx.nChannels * wfx.wBitsPerSample / 8;
		wfx.nAvgBytesPerSec = wfx.nBlockAlign * wfx.nSamplesPerSec;

		AM_MEDIA_TYPE mt;
		memset(&mt, 0, sizeof(mt));
		mt.majortype = MEDIATYPE_Audio;
		mt.subtype = MEDIASUBTYPE_PCM;
		mt.bFixedSizeSamples = TRUE;
		mt.formattype = GUID_NULL;
		mt.cbFormat = sizeof(WAVEFORMATEX);
		mt.pbFormat = (BYTE *)&wfx;
		hr = m_pAudioGrabber->SetMediaType(&mt);
		if (FAILED(hr))
			return hr;
	}

	std::vector<CComPtr<IPin> > grabberPins;
	CComPtr<IPin> pGrabberInputpin;
	CComPtr<IPin> pGrabberOutputpin;
	hr = DS_Utils::EnumPins(pSampleGrabberFilter, grabberPins);
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


	// NULL Render Filter
	CComPtr<IPin> pRenderInputpin;
	{
		CComPtr<IBaseFilter> pRenderFilter;
		pRenderFilter.CoCreateInstance( CLSID_NullRenderer );
		//pRenderFilter.CoCreateInstance( CLSID_AudioRender );
		if ( !pRenderFilter )
			return hr;

		hr = m_pGraph->AddFilter(pRenderFilter, L"");
		if (FAILED(hr))
			return hr;

		std::vector<CComPtr<IPin> > renderPins;
		hr = DS_Utils::EnumPins(pRenderFilter, renderPins);
		if (FAILED(hr))
			return hr;
		if (renderPins.size() != 1)
			return E_FAIL;
		pRenderInputpin = renderPins[0];
	}


	//////////////////////////////////////////////////////////////////////////
	// Connect

	hr = m_pGraph->Connect(pSourceOutputpin, pGrabberInputpin);
	if (FAILED(hr))
		return hr;

	hr = m_pGraph->Connect(pGrabberOutputpin, pRenderInputpin);
	if (FAILED(hr))
		return hr;

	return hr;
}
