#include "common_test.h"
#include "cf_platform.h"
#include <math.h>

void cf_random(random_t* r, uint64_t seed)
{
	*r = seed;
}

static uint64_t next_output(random_t* r) 
{
	*r ^= *r >> 12;
	*r ^= *r << 25;
	*r ^= *r >> 27;
	return *r * 2685821657736338717ull;
}

uint32_t cf_rand(random_t* r, uint32_t t)
{
	uint32_t x = next_output(r);
	// If x / 2^32 is uniform on [0,1), then x / 2^32 * (t+1) is uniform on
	// the interval [0,t+1), so the integer part is uniform on [0,t].
	uint64_t result = x * ((uint64_t)(t)+1);
	result >>= 32;
	return result;
}

uint32_t cf_rand_region(random_t* r, uint32_t low, uint32_t high)
{
	return cf_rand(r, high - low) + low;
}

double cf_gaussian(random_t* r, double mean, double std)
{
	const double kPi = 3.14159265358979323846;
	double u1 = (double)(next_output(r)) * 1.0 / 0xFFFFFFFFFFFFFFFFull;
	double u2 = (double)(next_output(r)) * 1.0 / 0xFFFFFFFFFFFFFFFFull;
	return mean + std * sqrt(-2 * log(u1)) * cos(2 * kPi * u2);
}

double gaussian(double mean, double std)
{
	const double kPi = 3.14159265358979323846;
	double u1 = (double)(rand()) * 1.0 / 0xFFFFFFFF;
	double u2 = (double)(rand()) * 1.0 / 0xFFFFFFFF;
	return mean + std * sqrt(-2 * log(u1)) * cos(2 * kPi * u2);
}






