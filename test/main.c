#include <stdio.h>
#include "cf_platform.h"
#include "test.h"

int main(int argc, const char* argv[])
{
	srand(time(NULL));
	
	/*test_trendline();*/
	/*test_aimd();*/
	/*test_inter_arrival();*/

	test_detector();

	return 0;
}


