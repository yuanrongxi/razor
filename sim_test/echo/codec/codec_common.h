/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#ifndef __echo_codec_common_h___
#define __echo_codec_common_h___

#ifdef _MSC_VER	/* MSVC */
#pragma warning(disable: 4996)
#define snprintf	_snprintf
#define vsnprintf	_vsnprintf
#define strcasecmp	stricmp
#define strncasecmp strnicmp
#endif

#include <stdint.h>

extern "C"
{
#include "x264.h"
#include "libavcodec/avcodec.h"
#include "libavutil/avutil.h"
#include "libswscale/swscale.h"
#include "libavutil/pixfmt.h"
};

#include "rate_stat.h"

/*各个分辨率值*/
#define PIC_WIDTH_160	160
#define PIC_HEIGHT_120	120

#define PIC_WIDTH_320	320
#define PIC_HEIGHT_240	240

#define PIC_WIDTH_480	480
#define PIC_HEIGHT_360	360

#define PIC_WIDTH_640	640
#define PIC_HEIGHT_480	480

#define PIC_WIDTH_960	800
#define PIC_HEIGHT_640	600

#define PIC_WIDTH_1280	1280
#define PIC_HEIGHT_720	720

#define PIC_WIDTH_1920	1920
#define PIC_HEIGHT_1080 1080

enum Resolution
{
	VIDEO_120P,
	VIDEO_240P,
	VIDEO_360P,
	VIDEO_480P,
	VIDEO_640P,
	VIDEO_720P,
	VIDEO_1080P,
};

#define KEY_FRAME_SEC 4

#define RESOLUTIONS_NUMBER 7

#define DEFAULT_FRAME_RATE 16

/*480P的码率范围和起始码率*/
#define MAX_VIDEO_BITRAE (1000 * 1000)
#define MIN_VIDEO_BITARE (32 * 1000)
#define START_VIDEO_BITRATE (800 * 1000) 

typedef struct
{
	int			resolution;				/*分辨率*/
	int			codec_width;			/*编码图像宽度*/
	int			codec_height;			/*编码图像高度*/
	uint32_t	max_rate;				/*最大码率，kbps*/
	uint32_t	min_rate;				/*最小码率, kbps*/
	uint32_t	start_rate;				/*起始码率，kbps*/
}encoder_resolution_t;

extern encoder_resolution_t resolution_infos[RESOLUTIONS_NUMBER];

//codec payload type,这里并没有用ffmpeg的codec ID,主要是考虑ffmpeg的codec ID是32位整数表示，在网络协议传输过程不需要这么长的描述
enum
{
	codec_raw = 32,
	codec_h263,
	codec_mpeg4,
	codec_h264,
	codec_h265,
	
	codec_vp8,
	codec_vp9
};

class VideoEncoder
{
public:
	VideoEncoder();
	virtual ~VideoEncoder();

	bool init(int frame_rate, int src_width, int src_height, int dst_width, int dst_height);
	void destroy();
	
	void set_bitrate(uint32_t bitrate_kbps, int lost);

	int get_codec_width() const;
	int get_codec_height() const;

	virtual bool encode(uint8_t *in, int in_size, enum PixelFormat pix_fmt, uint8_t *out, int *out_size, int *frame_type, bool request_keyframe = false) = 0;
	virtual int32_t get_bitrate() const = 0;

protected:
	void try_change_resolution();
	int find_resolution(uint32_t birate_kpbs);

	virtual bool open_encoder() = 0;
	virtual void reconfig_encoder(uint32_t bitrate) = 0;
	virtual void close_encoder() = 0;

protected:
	bool			inited_;

	unsigned int	src_width_;				// Input Width
	unsigned int	src_height_;			// Input Height

	int				frame_rate_;			// frame
	uint32_t		bitrate_kbps_;			// 网络反馈的码率

	int				max_resolution_;		// 初始编码分辨率,用户指定的最大分辨率
	int				curr_resolution_;		// 当前编码器所处的分辨率
	int				frame_index_;

	rate_stat_t		rate_stat_;
};

/*解码器接口*/
class VideoDecoder
{
public:
	VideoDecoder();
	virtual ~VideoDecoder();

	bool init();
	void destroy();

	bool decode(uint8_t *in, int in_size, uint8_t **out, int &out_width, int& out_height, int32_t &pict_type);
	uint32_t get_width() const { return curr_width_; };
	uint32_t get_height() const { return curr_height_; };


protected:
	virtual void set_codec_id() = 0;

protected:
	bool				inited_;

	int					curr_width_;
	int					curr_height_;

	uint8_t*            buff_;
	int					buff_size_;

	AVFrame*			de_frame_;
	AVCodec*			de_codec_;
	AVCodecContext*		de_context_;

	AVPacket			avpkt_;
	AVPicture			outpic_;

	SwsContext*			sws_context_;

	enum AVCodecID		codec_id_;
};

#endif
