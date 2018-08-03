#include <stdio.h>
#include <time.h>
#include "cf_platform.h"
#include "test.h"

int main(int argc, const char* argv[])
{
	srand(time(NULL));
	
	/*test_unwrapper();*/

	/*test_trendline();*/
	/*test_aimd();*/
	/*test_inter_arrival();*/

	/*test_detector();*/
	/*test_sender_bandwidth_estimator();*/
	/*test_sender_history();*/
	/*test_ack_bitrate_estimator();*/
	/*test_delay_base_bwe();*/
	/*test_interval_budget();*/
	test_pacer_queue();
	/*test_pace();*/
	
	/*test_rate_stat();*/
	/*test_rbe();*/
	/*test_loss_stat();*/
	/*test_windowed_filter();*/
	/*test_bbr_transfer_tracker();*/
	/*test_bandwidth_sampler();*/
	/*test_bbr_proc();*/
	/*test_bbr_receiver();*/
	return 0;
}


