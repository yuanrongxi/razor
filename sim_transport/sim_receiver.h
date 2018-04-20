
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

	uint32_t			min_fid;
	uint32_t			max_fid;
	uint32_t			frame_ts;		/*已经播放的相对视频时间戳*/
	uint64_t			play_ts;		/*当前系统时钟的时间戳*/

	uint32_t			max_ts;			/*缓冲区中最大的ts*/

	uint32_t			frame_timer;	/*帧间隔时间*/
	uint32_t			wait_timer;		/*cache缓冲时间，以毫秒为单位，这个cache的时间长度应该是> rtt + 2 * rtt_val,根据重发报文的次数来决定*/

	int					state;
	int					loss_flag;

	sim_frame_t*		frames;
}sim_frame_cache_t;

struct __sim_receiver
{
	uint32_t			base_uid;
	uint32_t			base_seq;
	uint32_t			max_seq;

	skiplist_t*			loss;
	int					loss_count;				/*单位时间内出现丢包的次数*/

	sim_frame_cache_t*	cache;

	uint64_t			ack_ts;
	uint64_t			cache_ts;
	uint64_t			active_ts;

	int					actived;
};


