/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "cf_skiplist.h"


static uint32_t wb_skip_depth()
{
	uint32_t d;
	for(d = 1; d < SKIPLIST_MAXDEPTH; d++){
		if((rand() & 0xffff) > 0x4000)
			break;
	}

	return d;
}

static int skiplist_compare(skiplist_item_t k1, skiplist_item_t k2)
{
	return memcmp(&k1, &k2, sizeof(skiplist_item_t));
}

skiplist_t* skiplist_create(skiplist_compare_f compare_cb, skiplist_free_f free_cb, void* args)
{
	skiplist_t* sl = (skiplist_t *)calloc(1, sizeof(skiplist_t));
	sl->compare_fun = compare_cb;
	if (sl->compare_fun == NULL)
		sl->compare_fun = skiplist_compare;

	sl->free_fun = free_cb;
	sl->args = args;

	return sl;
}

void skiplist_clear(skiplist_t* sl)
{
	skiplist_iter_t* iter, *next;
	int i;

	for(iter = sl->entries[0]; iter != NULL; iter = next){
		next = iter->next[0];
		if(sl->free_fun != NULL)
			sl->free_fun(iter->key, iter->val, sl->args);
		free(iter);
	}

	for(i = 0; i < SKIPLIST_MAXDEPTH; ++i)
		sl->entries[i] = 0;
	sl->size = 0;
}

void skiplist_destroy(skiplist_t* sl)
{
	assert(sl != NULL);

	if(sl == NULL)
		return ;

	skiplist_clear(sl);
	free(sl);
}

/*search skiplist*/
static int skiplist_insert_stack(skiplist_t* sl, skiplist_iter_t*** stack, skiplist_item_t key)
{
	int i, cmp;
	skiplist_iter_t** iterp, *iter;

	for(i = SKIPLIST_MAXDEPTH - 1, iterp = &sl->entries[i]; i >= 0;){
		iter = *iterp;
		if(iter == NULL){ /*empty level, dropdown a level*/
			stack[i--] = iterp --;
			continue;
		}

		/*compare key!!*/
		cmp = sl->compare_fun(iter->key, key);

		if(cmp > 0) /*fix pos, dropdown a level*/
			stack[i--] = iterp--;
		else if(cmp == 0) /*repeat!!*/
			return -1;
		else
			iterp = &(iter->next[i]);
	}

	return 0;
}

void skiplist_insert(skiplist_t* sl, skiplist_item_t key, skiplist_item_t val)
{
	skiplist_iter_t* iter, **stack[SKIPLIST_MAXDEPTH];
	uint32_t i, depth;

	if(skiplist_insert_stack(sl, stack, key) != 0){
		if (sl->free_fun != NULL)
			sl->free_fun(key, val, sl->args);
	}
	else{
		i = 0;
		depth = wb_skip_depth();

		iter = (skiplist_iter_t*)calloc(1, sizeof(skiplist_iter_t) + depth * sizeof(skiplist_iter_t*));
		iter->key = key;
		iter->val = val;
		iter->depth = depth;

		for(i = 0; i < depth; ++i){
			iter->next[i] = *stack[i];
			*stack[i] = iter;
		}

		++sl->size;
	}
}

static skiplist_iter_t* skip_remove_stack(skiplist_t* sl, skiplist_iter_t*** stack, skiplist_item_t key)
{
	int i, cmp;
	skiplist_iter_t** iterp, *ret, *iter;

	ret = NULL;

	for(i = SKIPLIST_MAXDEPTH - 1, iterp = &sl->entries[i]; i >= 0;){
		iter = *iterp;
		if(iter == NULL){ /*empty level, dropdown a level*/
			stack[i--] = iterp --;
			continue;
		}

		/*compare key!!*/
		cmp = sl->compare_fun(iter->key, key);

		if(cmp == 0){
			ret = iter;
			if(i > 0)
				stack[i--] = iterp --;
			else{
				stack[i] = iterp;
				break;
			}
		}
		else if(cmp < 0)
			iterp = &iter->next[i];
		else /*nonexistent key*/
			if(i > 0)
				stack[i--] = iterp --;
			else 
				break;
	}

	return ret;
}

skiplist_iter_t* skiplist_remove(skiplist_t* sl, skiplist_item_t key)
{
	skiplist_iter_t **stack[SKIPLIST_MAXDEPTH], *iter, *ret;
	int i;

	iter = skip_remove_stack(sl, stack, key);
	if (iter != NULL){
		for (i = 0; i < iter->depth; ++i)
			*stack[i] = iter->next[i];
		
		if (sl->free_fun != NULL)
			sl->free_fun(iter->key, iter->val, sl->args);
		free(iter);

		if (sl->size > 0)
			--sl->size;

		ret = *stack[0];
	}
	else
		ret = NULL;
	
	return ret;
}

static skiplist_iter_t* skiplist_search_iter(skiplist_t* sl, skiplist_item_t key)
{
	int i, cmp;
	skiplist_iter_t** iterp;
	skiplist_iter_t* iter;

	for (i = SKIPLIST_MAXDEPTH - 1, iterp = &sl->entries[i]; i >= 0;){
		iter = *iterp;
		if (iter == NULL){
			--i;
			--iterp;
			continue;
		}

		/*compare key!!*/
		cmp = sl->compare_fun(iter->key, key);
		if(cmp == 0)
			return iter;
		else if(cmp > 0){
			--i;
			--iterp;
		}
		else{
			iterp = &iter->next[i];
		}
	}

	return NULL;
}

skiplist_iter_t* skiplist_search(skiplist_t* sl, skiplist_item_t key)
{
	return skiplist_search_iter(sl, key);
}

skiplist_iter_t* skiplist_first(skiplist_t* sl) {
	return sl->entries[0];
}

size_t skiplist_size(skiplist_t* sl)
{
	return sl->size;
}

int idu64_compare(skiplist_item_t k1, skiplist_item_t k2)
{
	if (k1.u64 > k2.u64)
		return 1;
	else if (k1.u64 < k2.u64)
		return -1;
	else
		return 0;
}

int idu32_compare(skiplist_item_t k1, skiplist_item_t k2)
{
	if (k1.u32 > k2.u32)
		return 1;
	else if (k1.u32 < k2.u32)
		return -1;
	else
		return 0;
}

int idu16_compare(skiplist_item_t k1, skiplist_item_t k2)
{
	if (k1.u16 > k2.u16)
		return 1;
	else if (k1.u16 < k2.u16)
		return -1;
	else
		return 0;
}

int idu8_compare(skiplist_item_t k1, skiplist_item_t k2)
{
	if (k1.u8 > k2.u8)
		return 1;
	else if (k1.u8 < k2.u8)
		return -1;
	else
		return 0;
}

int id64_compare(skiplist_item_t k1, skiplist_item_t k2)
{
	if (k1.i64 > k2.i64)
		return 1;
	else if (k1.i64 < k2.i64)
		return -1;
	else
		return 0;
}

int id32_compare(skiplist_item_t k1, skiplist_item_t k2)
{
	if (k1.i32 > k2.i32)
		return 1;
	else if (k1.i32 < k2.i32)
		return -1;
	else
		return 0;
}

int id16_compare(skiplist_item_t k1, skiplist_item_t k2)
{
	if (k1.i16 > k2.i16)
		return 1;
	else if (k1.i16 < k2.i16)
		return -1;
	else
		return 0;
}

int id8_compare(skiplist_item_t k1, skiplist_item_t k2)
{
	if (k1.i8 > k2.i8)
		return 1;
	else if (k1.i8 < k2.i8)
		return -1;
	else
		return 0;
}




