#include "flex_fec_xor.h"

int flex_fec_generate(sim_segment_t* segs[], int segs_count, sim_fec_t* fec)
{
	register int i, j;
	sim_segment_t* seg;

	if (segs_count <= 1)
		return -1;

	fec->fec_meta.seq = segs[0]->packet_id;
	fec->fec_meta.fid = segs[0]->fid;
	fec->fec_meta.ts = segs[0]->timestamp;
	fec->fec_meta.payload_type = segs[0]->payload_type;
	fec->fec_meta.ftype = segs[0]->ftype;
	fec->fec_meta.index = segs[0]->index;
	fec->fec_meta.total = segs[0]->total;
	fec->fec_meta.size = segs[0]->data_size;

	memcpy(fec->fec_data, segs[0]->data, segs[0]->data_size);

	fec->fec_data_size = 0;
	for (i = 0; i < segs_count; i++){
		if (fec->fec_data_size < segs[i]->data_size)
			fec->fec_data_size = segs[i]->data_size;
	}
	/*补0参与运算*/
	memset(fec->fec_data + segs[0]->data_size, 0x00, fec->fec_data_size - segs[0]->data_size);

	for (i = 1; i < segs_count; i++){
		seg = segs[i];

		fec->fec_meta.seq ^= seg->packet_id;
		fec->fec_meta.fid ^= seg->fid;
		fec->fec_meta.ts ^= seg->timestamp;
		fec->fec_meta.payload_type ^= seg->payload_type;
		fec->fec_meta.ftype ^= seg->ftype;
		fec->fec_meta.index ^= seg->index;
		fec->fec_meta.total ^= seg->total;
		fec->fec_meta.size ^= seg->data_size;

		/*用0补齐,然后进行XOR运算*/
		memset(seg->data + seg->data_size, 0x00, fec->fec_data_size - seg->data_size);
		for (j = 0; j < fec->fec_data_size; j++)
			fec->fec_data[j] ^= seg->data[j];
	}

	return 0;
}

int flex_fec_recover(sim_segment_t* segs[], int segs_count, sim_fec_t* fec, sim_segment_t* out_seg)
{
	register int i, j;
	sim_segment_t* seg;

	if (segs_count <= 0)
		return -1;

	/*初始化XOR头*/
	out_seg->packet_id = fec->fec_meta.seq;
	out_seg->fid = fec->fec_meta.fid;
	out_seg->timestamp = fec->fec_meta.ts;
	out_seg->payload_type = fec->fec_meta.payload_type;
	out_seg->ftype = fec->fec_meta.ftype;
	out_seg->index = fec->fec_meta.index;
	out_seg->total = fec->fec_meta.total;
	out_seg->data_size = fec->fec_meta.size;

	memcpy(out_seg->data, fec->fec_data, fec->fec_data_size);

	/*求值*/
	for (i = 0; i < segs_count; i++){
		seg = segs[i];

		out_seg->packet_id ^= seg->packet_id;
		out_seg->fid ^= seg->fid;
		out_seg->timestamp ^= seg->timestamp;
		out_seg->payload_type ^= seg->payload_type;
		out_seg->ftype ^= seg->ftype;
		out_seg->index ^= seg->index;
		out_seg->total ^= seg->total;
		out_seg->data_size ^= seg->data_size;

		/*补0参与运算*/
		memset(seg->data + seg->data_size, 0x00, fec->fec_data_size - seg->data_size);

		for (j = 0; j < fec->fec_data_size; j++)
			out_seg->data[j] ^= seg->data[j];
	}

	/*FEC是以最大包长度为基准的，data size不会大于fec_data_size*/
	if (out_seg->data_size > fec->fec_data_size)
		return -1;

	out_seg->fec_id = fec->fec_id;

	return 0;
}



