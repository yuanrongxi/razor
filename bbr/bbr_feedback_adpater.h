#ifndef __bbr_feedback_adpater_h_
#define __bbr_feedback_adpater_h_

#include "cf_platform.h"
#include "estimator_common.h"
#include "bbr_common.h"

typedef struct
{
	int64_t					feedback_time;

	size_t					data_in_flight;
	size_t					prior_in_flight;

	int						packets_num;
	bbr_packet_info_t		packets[MAX_FEELBACK_COUNT];
}bbr_feedback_t;

int		bbr_feedback_get_loss(bbr_feedback_t* feeback, bbr_packet_info_t* loss_arr, int arr_size);
int		bbr_feedback_get_received(bbr_feedback_t* feeback, bbr_packet_info_t* recvd_arr, int arr_size);

#endif


