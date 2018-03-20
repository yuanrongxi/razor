#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "cf_mem_slab.h"
#include "cf_platform.h"

#define WB_SLAB_SIZE		1024
#define WB_MAX_SLAB_SIZE	(1024 * 1024)
#define BLOCK_SIZE			(4 * 1024 * 1024)

#define SLAB_IN_MAGIC		0x04958ef0
#define SLAB_OUT_MAGIC		0x48576eed
/*分配一个4M大小的block,并切分成slab*/
static void slab_add_block(mem_block_t* mem)
{
	mem_slab_t* slab;
	size_t pos;
	void* block;

	block = malloc(BLOCK_SIZE);

	if (mem == NULL || block == NULL){
		log_error("slab out of memory!\n");
		assert(0);

		return;
	}

	if (mem->block_num == mem->blocks_size){
		mem->blocks_size = mem->blocks_size * 2;
		mem->blocks = realloc(mem->blocks, mem->blocks_size * sizeof(void*));
	}

	mem->blocks[mem->block_num++] = block;

	/*切分成slab*/
	pos = 0;
	do{
		slab = (mem_slab_t*)((char*)block + pos);
		slab->magic = SLAB_IN_MAGIC;
		slab->next = mem->head;
		mem->head = slab;

		pos += mem->slab_size;
	} while (pos < BLOCK_SIZE);
}

static inline void slab_algin_size(mem_block_t* mem)
{
	int i, size;

	if (mem->slab_size > WB_MAX_SLAB_SIZE)
		mem->slab_size = WB_MAX_SLAB_SIZE;
	else if (mem->slab_size < WB_SLAB_SIZE)
		mem->slab_size = WB_SLAB_SIZE;

	/*做对其控制，防止切片失败*/
	for (i = 10; i <= 20; ++i){
		size = 1 << i;
		if (mem->slab_size <= size){
			mem->slab_size = size;
			break;
		}
	}
}

mem_block_t* slab_create(int slab_size)
{
	mem_block_t*	mem;

	mem = (mem_block_t*)calloc(1, sizeof(mem_block_t));

	mem->blocks_size = 16;

	mem->slab_size = slab_size;
	slab_algin_size(mem);

	mem->blocks = calloc(mem->blocks_size, sizeof(void*));
	mem->usable_size = mem->slab_size - sizeof(mem_slab_t);

	mem->head = NULL;
	/*建立slab链表关系*/
	slab_add_block(mem);

	return mem;
}

void slab_destroy(mem_block_t* mem)
{
	int i;

	assert(mem && mem->blocks);

	for (i = 0; i < mem->block_num; i++){
		if (mem->blocks[i])
			free(mem->blocks[i]);
	}

	free(mem->blocks);
	free(mem);
}

void* slab_alloc(mem_block_t* mem)
{
	mem_slab_t* slab;

	assert(mem);

	if (mem->head == NULL){
		log_info("slab add 4M block\n");
		slab_add_block(mem);
	}

	slab = mem->head;
	mem->head = slab->next;

	assert(slab->magic == SLAB_IN_MAGIC);
	slab->magic = SLAB_OUT_MAGIC;

	++mem->alloc_count;

	return (void*)(slab->data);
}

void slab_free(mem_block_t* mem, void* ptr)
{
	mem_slab_t* slab;

	assert(mem && ptr);

	slab = PTR2BLOCK(ptr);

	if (slab->magic == SLAB_OUT_MAGIC){
		slab->magic = SLAB_IN_MAGIC;
		slab->next = mem->head;
		mem->head = slab;
	}

	++mem->free_count;
}

void slab_print(mem_block_t* mem)
{
	log_info("memory slab, block number = %d, alloc count = %d, free count = %d\n", mem->block_num, mem->alloc_count, mem->free_count);
}














