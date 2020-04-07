#ifndef __flex_fec_receiver_h_
#define __flex_fec_receiver_h_

#include "flex_fec_xor.h"
#include "cf_skiplist.h"
#include "cf_list.h"

typedef void(*flex_fec_free_f)(sim_fec_t* fec, void* args);
typedef void(*flex_segment_free_f)(sim_segment_t* fec, void* args);

typedef struct
{
	uint16_t		fec_id;
	
	uint8_t			col;
	uint8_t			row;

	uint32_t		base_id;
	uint16_t		count;

	int				inited;

	skiplist_t*		segs;
	skiplist_t*		fecs;

	uint16_t		cache_size;
	sim_segment_t** cache;

	int				fec_ts;

	flex_fec_free_f flex_fec_free_cb;
	flex_segment_free_f flex_seg_free_cb;
	void*			args;
}flex_fec_receiver_t;

flex_fec_receiver_t*		flex_fec_receiver_create(flex_segment_free_f seg_free, flex_fec_free_f fec_free, void* args);

void						flex_fec_receiver_desotry(flex_fec_receiver_t* r);
void						flex_fec_receiver_reset(flex_fec_receiver_t* r);
void						flex_fec_receiver_active(flex_fec_receiver_t* r, uint16_t fec_id, uint8_t col, uint8_t row, uint32_t base_id, uint16_t count);

sim_segment_t*				flex_fec_receiver_on_fec(flex_fec_receiver_t* r, sim_fec_t* fec);
int							flex_fec_receiver_on_segment(flex_fec_receiver_t* r, sim_segment_t* seg, base_list_t* out);

#endif


