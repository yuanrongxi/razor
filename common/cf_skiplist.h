/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#ifndef __CF_SKIPLIST_H_
#define __CF_SKIPLIST_H_

#include <stdint.h>

#define SKIPLIST_MAXDEPTH	8

typedef union{
	int8_t		i8;
	uint8_t		u8;
	int16_t		i16;
	uint16_t	u16;
	int32_t		i32;
	uint32_t	u32;
	int64_t		i64;
	uint64_t	u64;
	void*		ptr;			/*custom context, such as: string*/
}skiplist_item_t;

/*skiplist iterator*/
typedef struct wb_skiplist_iter_s
{
	skiplist_item_t key;
	skiplist_item_t val;

	int	depth;

	struct wb_skiplist_iter_s* next[0];

}skiplist_iter_t;

/*key compare callback*/
typedef int(*skiplist_compare_f)(skiplist_item_t k1, skiplist_item_t k2);
/*free k/v pair callback*/
typedef void(*skiplist_free_f)(skiplist_item_t key, skiplist_item_t val, void* args);

typedef struct 
{
	size_t				size;	

	skiplist_compare_f	compare_fun;
	skiplist_free_f		free_fun;
	void*				args;					/*free¾ä±ú*/
	skiplist_iter_t*	entries[SKIPLIST_MAXDEPTH];
}skiplist_t;


skiplist_t*				skiplist_create(skiplist_compare_f compare_cb, skiplist_free_f free_cb, void* args);
void					skiplist_destroy(skiplist_t* sl);

void					skiplist_clear(skiplist_t* sl);
size_t					skiplist_size(skiplist_t* sl);

void					skiplist_insert(skiplist_t* sl, skiplist_item_t key, skiplist_item_t val);
skiplist_iter_t*		skiplist_remove(skiplist_t* sl, skiplist_item_t key);

skiplist_iter_t*		skiplist_search(skiplist_t* sl, skiplist_item_t key);
skiplist_iter_t*		skiplist_first(skiplist_t* sl);

/*traverse skiplist*/
#define SKIPLIST_FOREACH(sl, iter)	\
	for(iter = sl->entries[0]; iter != NULL; iter = iter->next[0])

int						id8_compare(skiplist_item_t k1, skiplist_item_t k2);
int						id16_compare(skiplist_item_t k1, skiplist_item_t k2);
int						id32_compare(skiplist_item_t k1, skiplist_item_t k2);
int						id64_compare(skiplist_item_t k1, skiplist_item_t k2);

int						idu8_compare(skiplist_item_t k1, skiplist_item_t k2);
int						idu16_compare(skiplist_item_t k1, skiplist_item_t k2);
int						idu32_compare(skiplist_item_t k1, skiplist_item_t k2);
int						idu64_compare(skiplist_item_t k1, skiplist_item_t k2);

#endif




