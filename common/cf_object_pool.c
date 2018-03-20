#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <assert.h>

#include "cf_object_pool.h"

#ifdef WIN32
/*fuck vs-2013*/
#pragma warning(disable: 4996)
#endif

ob_pool_t* pool_create(const char* name, size_t ob_size, size_t array_size,
						ob_constructor_t constructor, ob_destructor_t destructor, ob_check_t check, ob_reset_t reset)
{
	ob_pool_t* pool = (ob_pool_t *)calloc(1, sizeof(ob_pool_t));
#ifdef WIN32
	char* nm = _strdup(name);
#else
	char* nm = strdup(name);
#endif
	void** ptr = (void **)calloc(1, sizeof(void*)* array_size);
	if(ptr == NULL || nm == NULL || pool == NULL){
		free(ptr);
		free(nm);
		free(pool);

		return NULL;
	}

	pool->name = nm;
	pool->ptr = ptr;
	pool->array_size = array_size;
	pool->ob_size = ob_size;

	pool->constructor = constructor;
	pool->destructor = destructor;
	pool->check = check;
	pool->reset = reset;

	return pool;
}

void pool_destroy(ob_pool_t* pool)
{ 
	while(pool->curr > 0){
		void* ptr = pool->ptr[--pool->curr];
		if(pool->destructor)
			pool->destructor(ptr);

		free(ptr);
	}

	free(pool->name);
	free(pool->ptr);
	free(pool);
}

void* pool_alloc(ob_pool_t* pool)
{
	void* ret;

	if(pool->curr > 0){
		ret = pool->ptr[--pool->curr];
	}
	else{
		ret = malloc(pool->ob_size);
		if(ret != NULL && pool->constructor(ret) != 0){
			free(ret);
			ret = NULL;
		}
	}

	if(ret != NULL)
		pool->reset(ret, 1);
	
	pool->obj_count++;

	return ret;
}

void pool_free(ob_pool_t* pool, void* ob)
{
	if(ob == NULL || pool->check(ob) != 0){
		return ;
	}

	if (pool->obj_count > 0)
		pool->obj_count--;

	pool->reset(ob, 0);

	if(pool->curr < pool->array_size){
		pool->ptr[pool->curr ++] = ob;
	}
	else{
		if(pool->destructor != NULL)
			pool->destructor(ob);

		free(ob);
	}
}

void pool_print(ob_pool_t* pool)
{
	if(pool == NULL){
		printf("cell pool's ptr = NULL!");
		return;
	}
	printf("cell pool:\n");
	printf("\t name = %s\n\tob size = %d\n\tarray size = %d\n\tcurr = %d\n", 
		pool->name, (int)pool->ob_size, pool->array_size, pool->curr);
}

int32_t get_pool_info(ob_pool_t* pool, char* buf)
{
	int32_t pos = 0;
	assert(pool != NULL);
	
	pos = sprintf(buf, "%s pool:\n", pool->name);
	pos += sprintf(buf + pos, "\tob size = %d\n\tarray size = %d\n\tcurr = %d, objs count = %u\n", (int)pool->ob_size, pool->array_size, pool->curr, pool->obj_count);

	return pos;
}





