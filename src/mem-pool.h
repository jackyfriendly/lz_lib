/*
 * Author: Jacky
*/

#ifndef _MEM_POOL_H_
#define _MEM_POOL_H_

#include "list.h"
#include "locking.h"
#include <stdlib.h>


#define MALLOC(size) malloc(size)
#define CALLOC(cnt,size) calloc(cnt,size)
#define REALLOC(ptr,size) realloc(ptr,size)

#define FREE(ptr)				\
	if (ptr != NULL) {			\
		free ((void *)ptr);		\
		ptr = (void *)0xeeeeeeee;	\
	}                      

struct mem_pool {
	struct list_head  list;
	int               hot_count;
	int               cold_count;
	lz_lock_t         lock;
	unsigned long     padded_sizeof_type;
	void             *pool;
	void             *pool_end;
    int               real_sizeof_type;
};

struct mem_pool *
mem_pool_new_fn (unsigned long sizeof_type, unsigned long count);

#define mem_pool_new(type,count) mem_pool_new_fn (sizeof(type), count)

void mem_put (struct mem_pool *pool, void *ptr);
void *mem_get (struct mem_pool *pool);

void mem_pool_destroy (struct mem_pool *pool);

#endif /* _MEM_POOL_H */
