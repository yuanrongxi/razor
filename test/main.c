#include <stdio.h>
#include <time.h>
#include "cf_platform.h"
#include "test.h"

int main(int argc, const char* argv[])
{
	srand(time(NULL));
	
	/*test_trendline();*/
	/*test_aimd();*/
	/*test_inter_arrival();*/

	/*test_detector();*/
	/*test_sender_bandwidth_estimator();*/
	/*test_sender_history();*/
	test_ack_bitrate_estimator();

	return 0;
}


