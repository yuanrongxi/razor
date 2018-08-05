/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

enum{
	buffer_waiting = 0,
	buffer_playing
};

typedef struct
{
	uint32_t			fid;
	uint32_t			last_seq;
	uint32_t			ts;
	int					frame_type;

	int					seg_number;
	sim_segment_t**		segments;
}sim_frame_t;

typedef struct
{
	uint32_t			size;

	uint32_t			min_seq;

	uint32_t			min_fid;
	uint32_t			max_fid;
	uint32_t			play_frame_ts;
	uint32_t			max_ts;			/*缓冲区中最大的ts*/

	uint32_t			frame_ts;		/*已经播放的相对视频时间戳*/
	uint64_t			play_ts;		/*当前系统时钟的时间戳*/

	uint32_t			frame_timer;	/*帧间隔时间*/
	uint32_t			wait_timer;		/*cache缓冲时间，以毫秒为单位，这个cache的时间长度应该是> rtt + 2 * rtt_val,根据重发报文的次数来决定*/

	int					state;
	int					loss_flag;

	float				f;

	sim_frame_t*		frames;
}sim_frame_cache_t;

enum{
	fir_normal,
	fir_flightting,
};

struct __sim_receiver
{
	uint32_t			base_uid;
	uint32_t			base_seq;
	uint32_t			max_seq;
	uint32_t			max_ts;

	skiplist_t*			loss;
	int					loss_count;				/*单位时间内出现丢包的次数*/

	sim_frame_cache_t*	cache;

	uint64_t			ack_ts;
	uint64_t			cache_ts;
	uint64_t			active_ts;

	int					actived;

	/*和FIR有关的参数*/
	uint32_t			fir_seq;			/*请求关键帧的消息seq*/
	int					fir_state;			/*当前fir的状态，*/

	int					cc_type;

	razor_receiver_t*	cc;
	sim_session_t*		s;
};


