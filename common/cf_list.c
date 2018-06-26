/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#include <stdlib.h>
#include <assert.h>
#include "cf_list.h"

base_list_t* create_list()
{
	base_list_t* l = (base_list_t*)malloc(sizeof(base_list_t));
	l->head = l->tailer = NULL;
	l->size = 0;

	return l;
}

void destroy_list(base_list_t* l)
{
	assert(l != NULL);

	base_list_unit_t* pos = l->head;
	while (pos != NULL){
		l->head = pos->next;
		free(pos);
		pos = l->head;
	}

	free(l);
}

void list_push(base_list_t* l, void* data)
{
	assert(l != NULL);

	base_list_unit_t* unit = (base_list_unit_t *)malloc(sizeof(base_list_unit_t));
	unit->next = NULL;
	unit->pdata = data;

	if (l->head == NULL)
		l->head = unit;

	if (l->tailer == NULL)
		l->tailer = unit;
	else{
		l->tailer->next = unit;
		l->tailer = unit;
	}

	++l->size;
}

void* list_pop(base_list_t* l)
{
	assert(l != NULL);

	if (l->size <= 0 || l->head == NULL)
		return NULL;

	void* ret = NULL;
	base_list_unit_t* u = l->head;

	ret = l->head->pdata;
	if (l->tailer == l->head)
		l->tailer = l->tailer->next;

	l->head = l->head->next;
	--l->size;

	free(u);

	return ret;
}

void* list_front(base_list_t* l)
{
	assert(l != NULL);

	if (l->size <= 0 || l->head == NULL)
		return NULL;
	else
		return l->head->pdata;
}

void* list_back(base_list_t* l)
{
	assert(l != NULL);

	if (l->size <= 0 || l->tailer == NULL)
		return NULL;
	else
		return l->tailer->pdata;
}

size_t list_size(base_list_t* l)
{
	assert(l != NULL);
	return l->size;
}









