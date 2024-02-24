#include <stdio.h>
#include "cf_skiplist.h"


static void test_skiplist_bound_for() 
{
	skiplist_item_t key;
	skiplist_t* sl;
	skiplist_iter_t* iter;

	sl = skiplist_create(id32_compare, NULL, NULL);

	for (int i = 0; i < 10; i++) {
		key.i32 = i * 2;
		skiplist_insert(sl, key, key);
	}

	printf("skiplist foreach:");
	SKIPLIST_FOREACH(sl, iter) {
		printf("%d ", iter->val.i32);
	}
	printf("\n");


	printf("skiplist bound for(5):");
	key.i32 = 5;
	SKIPLIST_BOUND_FOR(sl, iter, key) {
		printf("%d ", iter->val.i32);
	}
	printf("\n");

	printf("skiplist bound for(10):");
	key.i32 = 10;
	SKIPLIST_BOUND_FOR(sl, iter, key) {
		printf("%d ", iter->val.i32);
	}
	printf("\n");

	printf("skiplist bound for(-1):");
	key.i32 = -1;
	SKIPLIST_BOUND_FOR(sl, iter, key) {
		printf("%d ", iter->val.i32);
	}
	printf("\n");

	printf("skiplist bound for(20):");
	key.i32 = 20;
	SKIPLIST_BOUND_FOR(sl, iter, key) {
		printf("%d ", iter->val.i32);
	}
	printf("\n");

	printf("skiplist bound for(30):");
	key.i32 = 30;
	SKIPLIST_BOUND_FOR(sl, iter, key) {
		printf("%d ", iter->val.i32);
	}
	printf("\n");
}

void test_skiplist() {
	test_skiplist_bound_for();
}


