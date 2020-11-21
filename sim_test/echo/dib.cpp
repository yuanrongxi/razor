/*-
* Copyright (c) 2017-2018 Razor, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/
#include "dib.h"
#include <string>


//////////////////////////////////////////////////////////////////////////
//CDib
//////////////////////////////////////////////////////////////////////////
CDib::CDib()
{
	init();
}

CDib::~CDib()
{
	fini();
}

void CDib::init()
{
	m_pDib = NULL;//dib buffer
	m_pData = NULL;//buffer point to dib buffer
	m_pRGB = NULL;//palette buffer
	
	m_pBmpInfo = NULL;//bmp info
	m_pBmpInfoHeader = NULL;// bmp info header	

	ZeroMemory(&m_BmpFileHeader, sizeof(BITMAPFILEHEADER));
	m_BmpFileHeader.bfType = 0x4D42;//"BM"

	m_valid = false;
}

void CDib::fini()
{
	if(is_valid())
	{
		if(m_pDib)
			free(m_pDib);
		
		init();
	}
}

//////////////////////////////////////////////////////////////////////////
//bmp operate
bool CDib::create( int _width, int _height, int _bits /* = 24 */ )
{
	fini();

	long _linesize = _bits * _width;

	while(_linesize % 4)
		_linesize++;
	
	long _dibsize = sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD) * get_palette_size(_bits) \
		+ _linesize * _height / 8;

	//allocate dib buffer
	m_pDib = (BYTE *)malloc(_dibsize);

	if(!m_pDib)
		return false;

	//init
	memset(m_pDib, 0, _dibsize);

	//set info ptr
	m_pBmpInfo = (BITMAPINFO*)m_pDib;
	m_pBmpInfoHeader = &m_pBmpInfo->bmiHeader;
	m_pData = (m_pDib) + sizeof(BITMAPINFOHEADER) \
		+ sizeof(RGBQUAD) * get_palette_size(_bits);

	if(get_palette_size(_bits) <= 0)
		m_pRGB = NULL;
	else
		m_pRGB = (RGBQUAD *)(m_pDib + sizeof(BITMAPINFOHEADER)) ;

	//init param
	m_pBmpInfoHeader->biSize = sizeof(BITMAPINFOHEADER);
	
	//save param
	m_pBmpInfoHeader->biWidth		= _width;
	m_pBmpInfoHeader->biHeight		= _height;
	m_pBmpInfoHeader->biBitCount	= _bits;
	m_pBmpInfoHeader->biSizeImage	= _dibsize;
	m_pBmpInfoHeader->biPlanes		= 1;
	
	m_BmpFileHeader.bfSize = sizeof(BITMAPFILEHEADER) + _dibsize;
	m_BmpFileHeader.bfOffBits = sizeof(BITMAPFILEHEADER) \
		+ sizeof(BITMAPINFOHEADER) + get_palette_size();

	//is valid now
	m_valid = true;

	return false;
}

bool CDib::destroy()
{
	fini();

	return true;
}

void* CDib::get_dib_bits()
{
	return m_pData;
}

bool CDib::set_dib_bits( void* _data, long _len, bool flag)
{
	if(is_valid() && _data)
	{
		if(flag)
			memcpy(m_pData, _data, _len);
		else{
			unsigned char* src = NULL;
			unsigned char* dst = NULL;

			unsigned int line_num = m_pBmpInfoHeader->biWidth * 3;
			for(int i = m_pBmpInfoHeader->biHeight - 1; i >= 0; i --){
				src = (unsigned char *)_data + i * line_num;
				dst = m_pData + (m_pBmpInfoHeader->biHeight - i - 1) * line_num;
				memcpy(dst, src, line_num);
			}

		}
		return true;
	}

	return false;
}

void CDib::reset_dib_bits( int _value )
{
	if(is_valid())
	{
		memset(m_pData, _value, get_dib_size());
	}
}



//////////////////////////////////////////////////////////////////////////
bool CDib::load( const char* _filename )
{
	fini();
	
	bool bret = false;
	
	return bret;
}

bool CDib::save( const char* _filename )
{

	
	return false;
}



//////////////////////////////////////////////////////////////////////////
//attr
bool CDib::is_valid() const
{
	return m_valid;
}

int CDib::get_width() const
{
	if(is_valid())
		return m_pBmpInfoHeader->biWidth;

	return 0;
}

int CDib::get_height() const
{
	if(is_valid())
		return m_pBmpInfoHeader->biHeight;

	return 0;	
}

int CDib::get_palette_size() const
{
	if(is_valid())
		return get_palette_size(m_pBmpInfoHeader->biBitCount);

	return 0;
}

int CDib::get_palette_size( int _bits ) const
{
	switch(_bits)
	{
	case 1:
		return 2;
	case 4:
		return 16;
	case 8:
		return 256;
	default:
		return 0;
	}	
}

int CDib::get_dib_size() const
{
	if(is_valid())
	{
		long _linesize = m_pBmpInfoHeader->biBitCount / 8 * m_pBmpInfoHeader->biWidth;
		
		while(_linesize % 4)
			_linesize++;

		return _linesize * m_pBmpInfoHeader->biHeight;
	}

	return 0;
}

//just line size, line size should be times 4
int CDib::get_dib_width() const
{
	if(is_valid())
	{
		long _linesize = m_pBmpInfoHeader->biBitCount / 8 * m_pBmpInfoHeader->biWidth;
		
		while(_linesize % 4)
			_linesize++;

		return _linesize;
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////
//draw
int CDib::bitblt( HDC hdc, int _xDest, int _yDest, int _width, int _height, \
				  int _xSrc, int _ySrc ) const
{
	if(is_valid())
	{
		// 防止缩放失真
		::SetStretchBltMode(hdc, STRETCH_HALFTONE);

		return ::SetDIBitsToDevice(hdc, _xDest, _yDest, _width, _height, \
			_xSrc, m_pBmpInfoHeader->biHeight - _ySrc - _height, \
			0, m_pBmpInfoHeader->biHeight, m_pData, m_pBmpInfo, DIB_RGB_COLORS);
	}

	return 0;
}

int CDib::bitblt( HDC hdc, int _xDest, int _yDest ) const
{
	if(is_valid())
	{
		// 防止缩放失真
		::SetStretchBltMode(hdc, STRETCH_HALFTONE);

		return ::SetDIBitsToDevice(hdc, _xDest, _yDest, m_pBmpInfoHeader->biWidth, m_pBmpInfoHeader->biHeight, \
			0, 0, \
			0, m_pBmpInfoHeader->biHeight, m_pData, m_pBmpInfo, DIB_RGB_COLORS);
	}
	
	return 0;
}

int CDib::stretch_blt( HDC hdc, int _dX, int _dY, int _dWidth, int _dHeight, \
					   int _sX, int _sY, int _sWidth, int _sHeight ) const
{
	if(is_valid())
	{
		// 防止缩放失真
		::SetStretchBltMode(hdc, STRETCH_HALFTONE);
		
		return ::StretchDIBits(hdc, _dX, _dY, _dWidth, _dHeight, \
			_sX, _sY, _sWidth, _sHeight, m_pData, \
			m_pBmpInfo, DIB_RGB_COLORS, SRCCOPY);
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////
//color convert
//计算常量
#define BLACK_Y 0
#define BLACK_U 128
#define BLACK_V 128

#define RGB2Y(b, g, r, y) \
	y = (unsigned char)(((int)30 * (r)  + (int)59 * (g) + (int)11 * (b)) / 100)

#define RGB2YUV(b, g, r, y, cb, cr) \
	RGB2Y(b, g, r, y); \
	cb = (unsigned char)(((int) - 17 * r  - (int)33 * g +(int)50 * b + 12800) / 100); \
	cr = (unsigned)(((int)50 * r  - (int)42 *g  - (int)8 * b + 12800) / 100)

#define LIMIT(x) (unsigned char) (((x > 0xffffff) ? 0xff0000 : ((x <= 0xffff) ? 0 : x & 0xff0000)) >> 16)

//////////////////////////////////////////////////////////////////////////
void CDib::to_yuv420p( void* _buffer )
{
	to_yuv420p(_buffer, get_width(), get_height());
}

void CDib::to_yuv420p( void* _buffer, int _width, int _height )
{
	if(is_valid())
	{
		rgb_2_yuv420p(m_pData, (unsigned char*)_buffer, \
			get_width(), get_height(), _width, _height, 3, true, false);
	}
}

void CDib::from_yuv420p( void* _buffer )
{
	from_yuv420p(_buffer, get_width(), get_height());
}

void CDib::from_yuv420p( void* _buffer, int _width, int _height )
{
	if(is_valid())
	{
		yuv420p_2_rgb((unsigned char*)_buffer, m_pData, _width, _height, 3, true, false);
	}
}

long CDib::yuv420p_2_rgb(const unsigned char* _yuv, unsigned char* _rgb, int _width, int _height, \
						  unsigned _rgbIncrement, bool _flib, bool _flibRB)
{
	if (_yuv == _rgb)
		return 0;

	unsigned char *dstImageFrame;
	unsigned int nbytes = _width * _height;
	const unsigned char *yplane = _yuv;               // 1 byte Y (luminance) for each pixel
	const unsigned char *uplane = yplane + nbytes;      // 1 byte U for a block of 4 pixels
	const unsigned char *vplane = uplane + (nbytes / 4);  // 1 byte V for a block of 4 pixels

	unsigned int pixpos[4] = {0, 1, _width, _height + 1};
	unsigned int originalPixpos[4] = {0, 1, _width, _width + 1};

	unsigned int   x, y, p;

	long int yvalue;
	long int l, r, g, b;

	if (_flib)
	{
		dstImageFrame = _rgb + ((_height - 2) * _width * _rgbIncrement);
		pixpos[0] = _width;
		pixpos[1] = _width +1;
		pixpos[2] = 0;
		pixpos[3] = 1;
	}
	else
		dstImageFrame = _rgb;

	for (y = 0; y < _height; y += 2)
	{
		for (x = 0; x < _width; x += 2)
		{
			// The RGB value without luminance
			long int  cr, cb, rd, gd, bd;
			
			cb = *uplane - 128;
			cr = *vplane - 128;
			rd = 104635 * cr;					//106986*cr
			gd = -25690 * cb - 53294 * cr;		// -26261*cb  +   -54496*cr 
			bd = 132278 * cb;					// 135221*cb
			
			// Add luminance to each of the 4 pixels			
			for (p = 0; p < 4; p++)
			{
				yvalue = *(yplane + originalPixpos[p]) - 16;
				
				if (yvalue < 0) yvalue = 0;
				
				l = 76310 * yvalue;
				
				r = l + rd;
				g = l + gd;
				b = l + bd;
				
				unsigned char* rgpPtr = dstImageFrame + _rgbIncrement * pixpos[p];
				
				if (_flibRB) 
				{
					*rgpPtr++ = LIMIT(r);
					*rgpPtr++ = LIMIT(g);
					*rgpPtr++ = LIMIT(b);
				}
				else 
				{
					*rgpPtr++ = LIMIT(b);
					*rgpPtr++ = LIMIT(g);
					*rgpPtr++ = LIMIT(r);
				}

				if (_rgbIncrement == 4)
					*rgpPtr = 0;
			}

			yplane += 2;
			dstImageFrame += _rgbIncrement * 2;

			uplane++;
			vplane++;
		}

		yplane += _width;

		if (_flib)
			dstImageFrame -= 3 * _rgbIncrement * _width;
		else
			dstImageFrame += _rgbIncrement * _width;
	}

	return nbytes * 24 / 8;	
}

long CDib::rgb_2_yuv420p( const unsigned char* _rgb, unsigned char* _yuv, \
						 int _src_width, int _src_height, int _dst_width, int _dst_height, \
						 unsigned _rgbIncrement, bool _flib, bool _flibRB )
{
	if((_src_width == _dst_width) && (_src_height == _dst_height))
		return rgb_2_yuv420p_same_size(_rgb, _yuv, _src_width, _src_height, \
		_rgbIncrement, _flib, _flibRB);
	else
		return rgb_2_yuv420p_resize(_rgb, _yuv, _src_width, _src_height, _dst_width, _dst_height, \
		_rgbIncrement, _flib, _flibRB);

	return 0;
}

long CDib::rgb_2_yuv420p_same_size( const unsigned char* _rgb, unsigned char* _yuv, \
								   int _width, int _height, \
								   unsigned _rgbIncrement, bool _flib, bool _flibRB )
{
	const unsigned planeSize = _width * _height;
	const unsigned halfWidth = _width >> 1;

	// get pointers to the data
	unsigned char* yplane  = _yuv;
	unsigned char* uplane  = _yuv + planeSize;
	unsigned char* vplane  = _yuv + planeSize + (planeSize >> 2);
	const unsigned char* rgbIndex = _rgb;

	for (unsigned y = 0; y < _height; y++)
	{
		unsigned char* yline  = yplane + (y * _width);
		unsigned char* uline  = uplane + ((y >> 1) * halfWidth);
		unsigned char* vline  = vplane + ((y >> 1) * halfWidth);

		if (_flib)
			rgbIndex = _rgb + (_width * (_height - 1 - y) * _rgbIncrement);

		for (unsigned x = 0; x < _width; x += 2)
		{
			if(_flibRB)	
			{
				RGB2Y(rgbIndex[2], rgbIndex[1], rgbIndex[0], *yline);
			}
			else
			{
				RGB2Y(rgbIndex[0], rgbIndex[1], rgbIndex[2], *yline);
			}
			
			rgbIndex += _rgbIncrement;
			
			yline++;
			
			if(_flibRB)	
			{
				RGB2YUV(rgbIndex[2], rgbIndex[1], rgbIndex[0], *yline, *uline, *vline);
			}
			else
			{
				RGB2YUV(rgbIndex[0], rgbIndex[1], rgbIndex[2], *yline, *uline, *vline);
			}
			
			rgbIndex += _rgbIncrement;
			
			yline++;
			uline++;
			vline++;
		}
	}	

	return planeSize * 12 / 8;
}

long CDib::rgb_2_yuv420p_resize( const unsigned char* _rgb, unsigned char* _yuv, \
								int _src_width, int _src_height, int _dst_width, int _dst_height, \
								unsigned _rgbIncrement, bool _flib, bool _flibRB )
{
	int planeSize = _dst_width * _dst_height;
	const int halfWidth = _dst_width >> 1;
	unsigned min_width, min_height;

	min_width  = min(_dst_width, _src_width);
	min_height = min(_dst_height, _src_height);

	// get pointers to the data
	unsigned char* yplane  = _yuv;
	unsigned char* uplane  = _yuv + planeSize;
	unsigned char* vplane  = _yuv + planeSize + (planeSize >> 2);
	const unsigned char* rgbIndex = _rgb;

	for (unsigned y = 0; y < min_height; y++) 
	{
		unsigned char* yline  = yplane + (y * _dst_width);
		unsigned char* uline  = uplane + ((y >> 1) * halfWidth);
		unsigned char* vline  = vplane + ((y >> 1) * halfWidth);

		if (_flib)
			rgbIndex = _rgb + (_src_width * (min_height - 1 - y) * _rgbIncrement); 

		for (unsigned x = 0; x < min_width; x += 2)
		{
			RGB2Y(rgbIndex[0], rgbIndex[1], rgbIndex[2], *yline);
			
			rgbIndex += _rgbIncrement;
			
			yline++;
			
			RGB2YUV(rgbIndex[0], rgbIndex[1], rgbIndex[2], *yline, *uline, *vline);
			
			rgbIndex += _rgbIncrement;
			
			yline++;
			uline++;
			vline++;
		}

		// Crop if source width > dest width
		if (_src_width > _dst_width)
			rgbIndex += _rgbIncrement * (_src_width - _dst_width);

		// Pad if dest width < source width
		if (_dst_width > _src_width)
		{
			memset(yline, BLACK_Y, _dst_width - _src_width);
			memset(uline, BLACK_U, (_dst_width - _src_width) >> 1);
			memset(vline, BLACK_V, (_dst_width - _src_width) >> 1);
		}
	}

	// Pad if dest height > source height
	if (_dst_height > _src_height)
	{
		unsigned char* yline  = yplane + (_src_height * _dst_height);
		unsigned char* uline  = uplane + ((_src_height >> 1) * halfWidth);
		unsigned char* vline  = vplane + ((_src_height >> 1) * halfWidth);
		unsigned fill = (_dst_height - _src_height) * _dst_height;

		memset(yline, BLACK_Y, fill);
		memset(uline, BLACK_U, fill >> 2);
		memset(vline, BLACK_V, fill >> 2);
	}	

	return planeSize * 12 / 8;
}

BITMAPINFO* CDib::get_bmp_info() const
{
	return m_pBmpInfo;
}
