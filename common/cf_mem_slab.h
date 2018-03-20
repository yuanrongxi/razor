#ifndef __cf_mem_slab_h_
#define __cf_mem_slab_h_

#include <stdint.h>
#include <stddef.h>

struct mem_slab_s;
typedef struct mem_slab_s mem_slab_t;

struct mem_slab_s
{
	mem_slab_t*		next;
	uint32_t		magic;				/*检验魔法字*/

	char			data[0];
};

typedef struct
{
	mem_slab_t*			head;

	void**				blocks;		/*整个内存块的数组，每一个block块大小是4M，切分成N个1K大小的wb_mem_slab_t，意味着每次我们向系统都是以4M大小来申请*/
	int					blocks_size;/*blocks的长度*/
	int					block_num;  /*block的数量*/

	int					slab_size;	/*最大不超过1M，默认是1024*/

	/*状态信息*/
	uint32_t			usable_size;
	uint32_t			alloc_count;
	uint32_t			free_count;
}mem_block_t;

#define PTR2BLOCK(ptr)	((mem_slab_t*)((char*)(ptr) - offsetof(mem_slab_t, data)))


mem_block_t*			slab_create(int slab_size);
void					slab_destroy(mem_block_t* mem);

void*					slab_alloc(mem_block_t* mem);
void					slab_free(mem_block_t* mem, void* ptr);
void					slab_print(mem_block_t* mem);

#endif


