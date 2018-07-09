#include "bbr_feedback_adpater.h"


int bbr_feedback_get_loss(bbr_feedback_t* feedback, bbr_packet_info_t* loss_arr, int arr_size)
{
	int i, count;

	count = 0;
	for (i = 0; i < feedback->packets_num; ++i){
		if (feedback->packets[i].recv_time <= 0)
			loss_arr[count++] = feedback->packets[i];
	}

	return count;
}

int bbr_feedback_get_received(bbr_feedback_t* feedback, bbr_packet_info_t* recvd_arr, int arr_size)
{
	int i, count;

	count = 0;
	for (i = 0; i < feedback->packets_num; ++i){
		if (feedback->packets[i].recv_time > 0)
			recvd_arr[count++] = feedback->packets[i];
	}
	return count;
}


