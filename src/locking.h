/*
 * Author: Jacky
*/

#ifndef _LOCKING_H
#define _LOCKING_H

#include <pthread.h>

#define LOCK_INIT(x)    pthread_mutex_init (x, 0)
#define LOCK(x)         pthread_mutex_lock (x)
#define TRY_LOCK(x)     pthread_mutex_trylock (x)
#define UNLOCK(x)       pthread_mutex_unlock (x)
#define LOCK_DESTROY(x) pthread_mutex_destroy (x)

typedef pthread_mutex_t lz_lock_t;

#endif /* _LOCKING_H */
