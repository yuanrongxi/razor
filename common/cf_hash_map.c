#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "cf_hash_map.h"

unsigned int mur_hash(const void *key, int len)
{
	const unsigned int m = 0x5bd1e995;
	const int r = 24;
	const int seed = 97;
	unsigned int h = seed ^ len;
	// Mix 4 bytes at a time into the hash
	const unsigned char *data = (const unsigned char *)key;
	while (len >= 4)
	{
		unsigned int k = *(unsigned int *)data;
		k *= m;
		k ^= k >> r;
		k *= m;
		h *= m;
		h ^= k;
		data += 4;
		len -= 4;
	}
	// Handle the last few bytes of the input array
	switch (len)
	{
	case 3: h ^= data[2] << 16;
	case 2: h ^= data[1] << 8;
	case 1: h ^= data[0];
		h *= m;
	};
	// Do a few final mixes of the hash to ensure the last few
	// bytes are well-incorporated.
	h ^= h >> 13;
	h *= m;
	h ^= h >> 15;
	return h;
}


base_hash_map_t* create_map(uint32_t bucket_size)
{
	base_hash_map_t* m = (base_hash_map_t *)malloc(sizeof(base_hash_map_t));
	m->bucket_size = bucket_size;
	m->size = 0;
	m->buckets = (base_map_unit_t **)calloc(1, sizeof(base_map_unit_t *) * bucket_size);

	return m;
}

void destroy_map(base_hash_map_t* m)
{
	base_map_unit_t* bucket, *prev;
	size_t i;

	assert(m != NULL);

	for (i = 0; i < m->bucket_size; ++i){
		bucket = m->buckets[i];
		while (bucket != NULL){
			prev = bucket;
			bucket = bucket->next;
			free(prev);
		}
	}

	free(m->buckets);
	free(m);
}

base_map_unit_t* get_map(base_hash_map_t* m, const void* key, size_t ksize)
{
	base_map_unit_t* ret = NULL;
	uint32_t hash = mur_hash(key, ksize);

	assert(m != NULL);

	ret = m->buckets[hash % m->bucket_size];
	while (ret != NULL){
		if (ksize == ret->ksize && memcmp(ITEM_KEY(ret), key, ksize) == 0)
			break;

		ret = ret->next;
	}

	return ret;
}

static base_map_unit_t** get_hash_ptr(base_hash_map_t* m, uint32_t hash, const void* key, size_t size)
{
	base_map_unit_t** ptr = &(m->buckets[hash % m->bucket_size]);
	while ((*ptr) != NULL){
		if (size == (*ptr)->ksize && memcmp(ITEM_KEY(*ptr), key, size) == 0)
			break;

		ptr = &((*ptr)->next);
	}

	return ptr;
}

void del_map(base_hash_map_t* m, const void* key, size_t ksize)
{
	base_map_unit_t** ptr;
	base_map_unit_t* f;

	uint32_t hash = mur_hash(key, ksize);

	assert(m != NULL);

	ptr = get_hash_ptr(m, hash, key, ksize);
	if (*ptr != NULL){
		f = *ptr;
		*ptr = (*ptr)->next;

		free(f);
	}
}

void insert_map(base_hash_map_t* m, const void* key, size_t ksize, const void* value, size_t vsize)
{
	uint32_t hash = mur_hash(key, ksize);
	base_map_unit_t** ptr = get_hash_ptr(m, hash, key, ksize);
	if (*ptr != NULL){
		if ((*ptr)->vsize < vsize)
			*ptr = (base_map_unit_t*)realloc(*ptr, sizeof(base_map_unit_t)+ksize + vsize);

		memcpy(ITEM_VALUE(*ptr), value, vsize); 
		(*ptr)->vsize = vsize;

	}
	else{
		*ptr = (base_map_unit_t *)malloc(sizeof(base_map_unit_t)+ksize + vsize);
		(*ptr)->hash = hash;
		(*ptr)->ksize = ksize;
		(*ptr)->vsize = vsize;
		(*ptr)->next = NULL;

		memcpy(ITEM_KEY(*ptr), key, ksize);
		memcpy(ITEM_VALUE(*ptr), value, vsize);
	}
}



