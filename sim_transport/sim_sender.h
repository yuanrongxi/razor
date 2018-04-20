struct __sim_sender
{
	int							state;
	uint32_t					base;				/*接受端报告的已经受理的seq*/
	uint32_t					base_ts;			/*确认收到的帧的时间戳*/

	uint32_t					frame_ts;			/*当前发送的ts，是个相对值*/
	uint64_t					start_ts;			/*上一帧的绝对时间戳*/

	skiplist_t*					cache;				/*发送窗口*/

	uint64_t					tick_ts;
	uint64_t					rtt_ts;

	int							actived;

	razor_sender_t*				cc;					/*拥塞控制对象*/
};



