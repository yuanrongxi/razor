#ifndef __CF_HASH_MAP_H_
#define __CF_HASH_MAP_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ITEM_KEY(k)				((uint8_t*)((k)->data))
#define ITEM_VALUE(k)			((uint8_t*)((k)->data + (k)->ksize))

typedef struct base_map_unit_t
{
	struct base_map_unit_t* next;
	uint32_t				hash;
	size_t					ksize;			
	size_t					vsize;
	char					data[1];
}base_map_unit_t;

typedef struct
{
	uint32_t				bucket_size;
	base_map_unit_t**		buckets;

	size_t					size;			
}base_hash_map_t;

unsigned int				mur_hash(const void *key, int len);

base_hash_map_t*			create_map(uint32_t bucket_size);
void						destroy_map(base_hash_map_t* m);

base_map_unit_t*			get_map(base_hash_map_t* m, const void* key, size_t ksize);
void						del_map(base_hash_map_t* m , const void* key, size_t ksize);
void						insert_map(base_hash_map_t* m, const void* key, size_t ksize, const void* value, size_t vsize);

#define FOREACH_MAP(m, p, i)				\
for ((i) = 0; (i) < m->bucket_size; ++(i)){	\
	p = m->buckets[i];						\
while (p != NULL){

#define FOREACH_MAP_END(m, p)			(p) = (p)->next;}}

#ifdef __cplusplus
}
#endif

#endif



