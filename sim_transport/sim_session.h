/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

struct __sim_session
{
	su_socket		s;
	su_addr			peer;				

	uint32_t		scid;				/*sender call id*/
	uint32_t		rcid;				/*receiver call id*/
	uint32_t		uid;				/*本端用户ID*/

	uint32_t		rtt;				/*rtt值*/
	uint32_t		rtt_var;			/*rtt误差修正值*/
	uint8_t			loss_fraction;		/*丢包率, 0 ~ 255之间的数，100% = 255*/
	uint32_t		fir_seq;

	int				state;				/*状态*/
	int				interrupt;			/*中断*/

	int				transport_type;		/*拥塞控制的类型*/
	int				padding;			/*cc是否采用填充模式*/

	volatile int	run;				/*run线程标示 */
	su_mutex		mutex;				/*用于上层多线程操作的保护锁*/
	su_thread		thr;				/*线程ID*/

	sim_sender_t*	sender;				/*视频发送器*/
	sim_receiver_t*	receiver;			/*视频接收器*/

	int				resend;				/*重发次数*/
	int64_t			commad_ts;			/*信令时间戳*/
	int64_t			stat_ts;

	uint32_t		rbandwidth;			/*接收带宽*/
	uint32_t		sbandwidth;			/*发送带宽*/
	uint32_t		rcount;				/*接收的报文数量*/
	uint32_t		scount;				/*发送的报文数量*/
	uint32_t		video_bytes;
	uint32_t		max_frame_size;		/*周期内最大帧*/

	int				min_bitrate;		/*配置的最小码率，单位：bps*/
	int				max_bitrate;		/*配置的最大码率，单位: bps*/
	int				start_bitrate;		/*配置的起始码率，单位: bps*/

	sim_notify_fn	notify_cb;
	sim_change_bitrate_fn change_bitrate_cb;
	sim_state_fn	state_cb;
	void*			event;

	bin_stream_t	sstrm;
};

