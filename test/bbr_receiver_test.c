#include "bbr_receiver.h"
#include "bbr_common.h"
#include "common_test.h"

typedef struct
{
	cf_unwrapper_t	unwrapper;
	bbr_receiver_t*	r;
	int				run;
}bbr_recv_tester_t;



static void send_bbr_feedback(void* handler, const uint8_t* payload, int payload_size)
{
	int i;
	bin_stream_t strm;
	bbr_feedback_msg_t msg = { 0 };

	bbr_recv_tester_t* t = (bbr_recv_tester_t*)handler;

	bin_stream_init(&strm);
	bin_stream_resize(&strm, payload_size);
	bin_stream_set_used_size(&strm, payload_size);
	memcpy(strm.data, payload, payload_size);


	bbr_feedback_msg_decode(&strm, &msg);

	bin_stream_destroy(&strm);
}

static void init_bbr_recv_tester(bbr_recv_tester_t* t)
{
	t->r = bbr_receive_create(t, send_bbr_feedback);
	init_unwrapper16(&t->unwrapper);
}

static void destroy_bbr_recv_tester(bbr_recv_tester_t* t)
{
	if (t->r != NULL){
		bbr_receive_destroy(t->r);
		t->r = NULL;
	}
}


void test_bbr_receiver()
{
	bbr_recv_tester_t t;
	int i;
	int16_t array1[20] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 12, 14, 15,16, 17, 18, 20, 21 };
	int16_t array2[20] = { 19, 22, 23, 24, 25, 26, 27, 29, 31, 32, 33, 34, 35, 36, 38, 40, 41, 42, 43, 44};
	init_bbr_recv_tester(&t);

	for (i = 0; i < 20; i++){
		bbr_receive_on_received(t.r, array1[i], 0, 100, 1);
		su_sleep(0, 1000);
	}
	su_sleep(0, 1000 * 100);
	
	for (i = 0; i < 20; i++){
		bbr_receive_on_received(t.r, array2[i], 0, 100, 1);
		su_sleep(0, 1000);
	}

	su_sleep(0, 1000 * 500);
	printf("heartbeat!!\n");

	bbr_receive_check_acked(t.r);

	destroy_bbr_recv_tester(&t);
}



