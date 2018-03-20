#ifndef __estimator_common_h_
#define __estimator_common_h_

enum {
	kBwNormal = 0,
	kBwUnderusing = 1,
	kBwOverusing = 2,
};

enum {
	kRcHold, 
	kRcIncrease, 
	kRcDecrease
};

enum { 
	kRcNearMax, 
	kRcAboveMax, 
	kRcMaxUnknown 
};

typedef struct
{
	int			state;
	uint32_t	incoming_bitrate;
	double		noise_var;
}rate_control_input_t;

#define DEFAULT_RTT 200

#endif



