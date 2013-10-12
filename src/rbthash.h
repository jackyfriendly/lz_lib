/*
 * Author: Jacky
*/

#ifndef __RBTHASH_TABLE_H_
#define __RBTHASH_TABLE_H_
#include "rb.h"
#include "locking.h"
#include "mem-pool.h"
#include "logging.h"
#include "common-utils.h"

#include <pthread.h>

#define LZ_RBTHASH_MEMPOOL      16384 //1048576
#define LZ_RBTHASH              "rbthash"

struct rbthash_bucket {
        struct rb_table *bucket;
        lz_lock_t       bucketlock;
};

typedef struct rbthash_entry {
        void            *data;
        void            *key;
        int             keylen;
        uint32_t        keyhash;
} rbthash_entry_t;

typedef uint32_t (*rbt_hasher_t) (void *data, int len);
typedef void (*rbt_data_destroyer_t) (void *data);

typedef struct rbthash_table {
        int                     size;
        int                     numbuckets;
        struct mem_pool         *entrypool;
        lz_lock_t               tablelock;
        struct rbthash_bucket   *buckets;
        rbt_hasher_t            hashfunc;
        rbt_data_destroyer_t    dfunc;
        lz_boolean_t            pool_alloced;
} rbthash_table_t;

extern rbthash_table_t *
rbthash_table_init (int buckets, rbt_hasher_t hfunc,
                    rbt_data_destroyer_t dfunc, unsigned long expected_entries,
                    struct mem_pool *entrypool);

extern int
rbthash_insert (rbthash_table_t *tbl, void *data, void *key, int keylen);

extern void *
rbthash_get (rbthash_table_t *tbl, void *key, int keylen);

extern void *
rbthash_remove (rbthash_table_t *tbl, void *key, int keylen);

extern void *
rbthash_replace (rbthash_table_t *tbl, void *key, int keylen, void *newdata);

extern void
rbthash_table_destroy (rbthash_table_t *tbl);
#endif
