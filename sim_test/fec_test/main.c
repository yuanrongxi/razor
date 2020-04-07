#include <stdint.h>
#include "test_func.h"

int main(int argc, const char* argv[])
{
	//test_fec_xor();
	//test_num_fec();
	//test_flex_sender(20);
	//test_flex_sender(80);
	//test_flex_receiver(20, 2);
	test_flex_receiver(80, 9);
	return 0;
}


