/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#include "remote_estimator_proxy.h"

#define DEFAULT_PROXY_INTERVAL_TIME 100
#define BACK_WINDOWS_MS 500


estimator_proxy_t* estimator_proxy_create(size_t packet_size, uint32_t ssrc)
{
	estimator_proxy_t* proxy = calloc(1, sizeof(estimator_proxy_t));
	proxy->header_size = packet_size;
	proxy->ssrc = ssrc;
	proxy->hb_ts = -1;
	proxy->wnd_start_seq = -1;
	proxy->max_arrival_seq = -1;
	proxy->send_interval_ms = DEFAULT_PROXY_INTERVAL_TIME;

	proxy->arrival_times = skiplist_create(id64_compare, NULL, NULL);

	init_unwrapper16(&proxy->unwrapper);

	return proxy;
}

void estimator_proxy_destroy(estimator_proxy_t* proxy)
{
	if (proxy == NULL)
		return;

	if (proxy->arrival_times != NULL){
		skiplist_destroy(proxy->arrival_times);
		proxy->arrival_times = NULL;
	}

	free(proxy);
}

#define MAX_IDS_NUM 100
void estimator_proxy_incoming(estimator_proxy_t* proxy, int64_t arrival_ts, uint32_t ssrc, uint16_t seq)
{
	int64_t sequence, ids[MAX_IDS_NUM];

	skiplist_iter_t* iter;
	skiplist_item_t key, val;
	int num, i;

	if (arrival_ts < 0)
		return;

	proxy->ssrc = ssrc;
	sequence = wrap_uint16(&proxy->unwrapper, seq);

	if (sequence > proxy->wnd_start_seq + 32767)
		return;

	if (proxy->max_arrival_seq <= proxy->wnd_start_seq){
		num = 0;
		/*删除过期的到达时间统计，因为UDP会乱序，这里只会删除时间超过500毫秒且是当前报文之前的报文的记录*/
		SKIPLIST_FOREACH(proxy->arrival_times, iter){
			if (iter->key.i64 < sequence && arrival_ts >= iter->val.i64 + BACK_WINDOWS_MS && num < MAX_IDS_NUM)
				ids[num++] = iter->key.i64;
		}

		for (i = 0; i < num; ++i){
			key.i64 = ids[i];
			skiplist_remove(proxy->arrival_times, key);
		}
	}

	if (proxy->wnd_start_seq == -1)
		proxy->wnd_start_seq = seq;
	else if (sequence < proxy->wnd_start_seq)
		proxy->wnd_start_seq = sequence;

	/*保存接收到的最大sequence*/
	proxy->max_arrival_seq = SU_MAX(proxy->max_arrival_seq, sequence);

	key.i64 = sequence;
	val.i64 = arrival_ts;
	skiplist_insert(proxy->arrival_times, key, val);
}

static int proxy_bulid_feelback_packet(estimator_proxy_t* proxy, feedback_msg_t* msg)
{
	skiplist_iter_t* iter;
	int64_t new_start_seq = -1;

	if (proxy->max_arrival_seq <= proxy->wnd_start_seq &&  skiplist_size(proxy->arrival_times) > 2)
		return -1;
	
	msg->min_ts = -1;
	msg->samples_num = 0;
	msg->base_seq = proxy->wnd_start_seq;

	SKIPLIST_FOREACH(proxy->arrival_times, iter){

		if (iter->key.i64 >= proxy->wnd_start_seq){
			/*找到最早到达的报文时间戳*/
			if (msg->min_ts == -1 || msg->min_ts > iter->val.i64)
				msg->min_ts = iter->val.i64;

			msg->samples[msg->samples_num].seq = (iter->key.i64 & 0xffff);
			msg->samples[msg->samples_num].ts = iter->val.i64;
			msg->samples_num++;

			/*更新下一个feelback的起始位置*/
			new_start_seq = iter->key.i64 + 1;

			if (msg->samples_num >= MAX_FEELBACK_COUNT)
				break;
		}
	}

	/*进行到达时间序列编码*/
	if (msg->samples_num > 2){
		proxy->wnd_start_seq = new_start_seq;
		return 0;
	}
	
	return -1;
}

int estimator_proxy_heartbeat(estimator_proxy_t* proxy, int64_t cur_ts, feedback_msg_t* msg)
{
	if (cur_ts >= proxy->hb_ts + proxy->send_interval_ms || proxy->wnd_start_seq + DEFAULT_PROXY_INTERVAL_TIME <= proxy->max_arrival_seq){
		proxy->hb_ts = cur_ts;
		return proxy_bulid_feelback_packet(proxy, msg);
	}
	return -1;
}

#define kMaxSendIntervalMs 250
#define kMinSendIntervalMs 50

/*码率发生变化时重新评估发送间隔时间*/
void estimator_proxy_bitrate_changed(estimator_proxy_t* proxy, uint32_t bitrate)
{
	double rate = bitrate * 0.05;
	proxy->send_interval_ms = (int64_t)((proxy->header_size * 8.0 * 1000.0) / rate + 0.5);
	proxy->send_interval_ms = SU_MAX(SU_MIN(proxy->send_interval_ms, kMaxSendIntervalMs), kMinSendIntervalMs);

}










