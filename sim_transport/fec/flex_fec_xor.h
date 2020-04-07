#ifndef __flex_fec_xor_h_
#define __flex_fec_xor_h_

#include "cf_platform.h"
#include "sim_proto.h"

int flex_fec_generate(sim_segment_t* segs[], int segs_count, sim_fec_t* fec);
int flex_fec_recover(sim_segment_t* segs[], int segs_count, sim_fec_t* fec, sim_segment_t* out_seg);

#endif



