#ifndef __flex_fec_sender_h_
#define __flex_fec_sender_h_

#include "flex_fec_xor.h"
#include "cf_list.h"

typedef struct
{
	uint16_t		fec_id;
	uint8_t			row;
	uint8_t			col;
	uint32_t		base_id;

	int				first;

	int64_t			fec_ts;

	uint16_t		seg_size;
	uint16_t		segs_count;
	sim_segment_t** segs;

	uint16_t		cache_size;
	sim_segment_t** cache;
}flex_fec_sender_t;

flex_fec_sender_t*			flex_fec_sender_create();

void						flex_fec_sender_destroy(flex_fec_sender_t* fec);

void						flex_fec_sender_reset(flex_fec_sender_t* fec);

void						flex_fec_sender_add_segment(flex_fec_sender_t* fec, sim_segment_t* seg);

void						flex_fec_sender_update(flex_fec_sender_t* fec, uint8_t protect_fraction, base_list_t* out_fecs);
/*释放fec_update产生的fec packet对象*/
void						flex_fec_sender_release(flex_fec_sender_t* fec, base_list_t* out_fecs);

int							flex_fec_sender_num_packets(flex_fec_sender_t* fec, uint8_t protect_fraction);

#endif
