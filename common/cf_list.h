/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#ifndef __WB_LIST_H_
#define __WB_LIST_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct base_list_unit_t{
	struct base_list_unit_t*		next;
	void*							pdata;
}base_list_unit_t;

typedef struct {
	base_list_unit_t*		head;
	base_list_unit_t*		tailer;
	size_t					size;
} base_list_t;


base_list_t*				create_list();
void						destroy_list(base_list_t* l);

void						list_push(base_list_t* l, void* data);
void*						list_pop(base_list_t* l);
void*						list_front(base_list_t* l);
void*						list_back(base_list_t* l);

size_t						list_size(base_list_t* l);

#define LIST_FOREACH(l, pos)							\
	for (pos = (l)->head; pos != NULL; pos = pos->next)	


#ifdef __cplusplus
}
#endif

#endif



