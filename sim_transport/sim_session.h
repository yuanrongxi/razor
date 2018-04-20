
struct __sim_session
{
	su_socket		s;
	su_addr			peer;

	uint32_t		cid;
	uint32_t		uid;				/*本端用户ID*/

	uint32_t		rtt;				/*rtt值*/
	uint32_t		rtt_var;			/*rtt误差修正值*/

	int				state;				/*状态*/
	volatile int	run;				/*run线程标示 */
	su_mutex		mutex;				/*保护锁*/

	sim_sender_t*	sender;				/*视频发送器*/
	sim_receiver_t*	receiver;			/*视频接收器*/

	int				resend;				/*重发次数*/
	uint64_t		commad_ts;			/*信令时间戳*/
	uint64_t		stat_ts;

	uint32_t		rbandwidth;			/*接收带宽*/
	uint32_t		sbandwidth;			/*发送带宽*/
	uint32_t		rcount;
	uint32_t		scount;

	bin_stream_t	sstrm;

	su_thread		thr;				/*线程ID*/

	vch_notify_fn	notify_cb;
	vch_adjust_fn	adjust_cb;
	vch_state_fn	state_cb;
};

