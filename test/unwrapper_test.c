#include <stdio.h>
#include <assert.h>
#include "cf_unwrapper.h"

static void test_unwrapper32()
{
	cf_unwrapper_t seq_32;
	int64_t vals[6] = { 0xfffffffc, 0xffffffff, 0xfffffffe, 0xfffffffd, (int64_t)0xffffffff + 1, (int64_t)0xffffffff + 2 };
	uint64_t val;
	int i;

	init_unwrapper32(&seq_32);
	seq_32.last_value = 65537 + 40000;

	for (i = 0; i < 6; i++){
		val = wrap_uint32(&seq_32, vals[i]);
		printf("src = %llu, dst = %llu\n", vals[i], val);
	}
}

static void test_unwrapper16()
{
	cf_unwrapper_t seq_16;
	int32_t vals[6] = { 65533, 65536, 65535, 65534, 65537, 65538 };
	uint32_t val;
	int i;

	init_unwrapper16(&seq_16);
	seq_16.last_value = 65532;

	for (i = 0; i < 6; i++){
		val = wrap_uint16(&seq_16, vals[i]);
		printf("src = %u, dst = %u\n", vals[i], val);
	}
}

static void test_for()
{
	cf_unwrapper_t seq_16;
	uint32_t i, val;

	init_unwrapper16(&seq_16);

	for (i = 0; i < 1000000; i += 10){
		val = wrap_uint16(&seq_16, i & 0xffff);
		assert(val == i);
	}
}


int test_unwrapper()
{
	uint32_t k = (5 << 26) / 1000;

	printf("k = %u \n", k);

	test_unwrapper32();
	printf("===============\n");

	test_unwrapper16();

	test_for();

	return 0;
}
