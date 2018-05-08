/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/
#ifndef __DIB_H
#define __DIB_H


#include <windows.h>


//////////////////////////////////////////////////////////////////////////
typedef struct  tagColor
{
	char blue;
	char green;
	char red;	
}Color_t;


//////////////////////////////////////////////////////////////////////////
class CDib
{
public:
	CDib();
	virtual ~CDib();

public:
	//bmp operate
	bool create(int _width, int _height, int _bits = 24);
	bool destroy();

	//bmp data
	BITMAPINFO* get_bmp_info() const;

	void*	get_dib_bits();
	bool	set_dib_bits(void* _data, long _len, bool flag = true);
	void	reset_dib_bits(int _value);

	//bmp file
	bool load(const char* _filename);
	bool save(const char* _filename);

	//draw
	int bitblt(HDC hdc, int _xDest, int _yDest) const;
	int bitblt(HDC hdc, int _xDest, int _yDest, int _width, int _height, \
				  int _xSrc, int _ySrc) const;

	int stretch_blt(HDC hdc, int _dX, int _dY, int _dWidth, int _dHeight, \
		int _sX, int _sY, int _sWidth, int _sHeight) const;

	//attr
	bool is_valid() const;

	int get_width() const;
	int get_height() const;
	
	int get_palette_size() const;
	int get_palette_size(int _bits) const;

	int get_dib_size() const;
	int get_dib_width() const;

	//color convert
	void to_yuv420p(void* _buffer);
	void to_yuv420p(void* _buffer, int _width, int _height);

	void from_yuv420p(void* _buffer);
	void from_yuv420p(void* _buffer, int _width, int _height);

	//magic

public:
	static long rgb_2_yuv420p(const unsigned char* _yuv, unsigned char* _rgb, 
		int _src_width, int _src_height, int _dst_width, int _dst_height,
		unsigned _rgbIncrement, bool _flib, bool _flibRB);

	static long yuv420p_2_rgb(const unsigned char* _rgb, unsigned char* _yuv, 
		int _width, int _height, 
		unsigned _rgbIncrement, bool _flib, bool _flibRB);

protected:
	void init();
	void fini();

protected:
	static long rgb_2_yuv420p_same_size(const unsigned char* _rgb, unsigned char* _yuv, 
		int _width, int _height, 
		unsigned _rgbIncrement, bool _flib, bool _flibRB);

	static long rgb_2_yuv420p_resize(const unsigned char* _rgb, unsigned char* _yuv, 
		int _src_width, int _src_height, int _dst_width, int _dst_height,
		unsigned _rgbIncrement, bool _flib, bool _flibRB);

protected:
	BYTE* m_pDib;//dib buffer
	BYTE* m_pData;//buffer point to dib buffer
	RGBQUAD* m_pRGB;//palette buffer
	
	BITMAPFILEHEADER m_BmpFileHeader;//bmp file header
	BITMAPINFO* m_pBmpInfo;//bmp info
	BITMAPINFOHEADER* m_pBmpInfoHeader;// bmp info header

	bool m_valid;
};


#endif//__DIB_H
