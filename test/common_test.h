#ifndef __common_test_h_
#define __common_test_h_
#include <assert.h>
#include <stdint.h>

#define EXPECT_NEAR(src, dst, delta) assert(((dst) <= (src) + (delta)) && ((dst) >= (src) - (delta)))
#define EXPECT_EQ(src, dst) assert((src) == (dst))
#define EXPECT_LT(src, dst) assert((src) < (dst))
#define EXPECT_LE(src, dst) assert((src) <= (dst))
#define EXPECT_GE(src, dst) assert((src) >= (dst))
#define EXPECT_GT(src, dst) assert((src) > (dst))

typedef uint64_t random_t;

void cf_random(random_t* r, uint64_t seed);
uint32_t cf_rand(random_t* r, uint32_t t);
uint32_t cf_rand_region(random_t* r, uint32_t low, uint32_t high);

double cf_gaussian(random_t* r, double mean, double std);

double gaussian(double mean, double std);

#endif



