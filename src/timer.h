/*
 * Author: Jacky
*/

#ifndef _TIMER_H
#define _TIMER_H

#include <sys/time.h>
#include <pthread.h>
#include <sys/types.h>

#include "globals.h"
typedef void (*lz_timer_cbk_t) (void *);

struct _lz_timer {
  struct _lz_timer *next, *prev;
  struct timeval at;
  lz_timer_cbk_t cbk;
  void *data;
};

struct _lz_timer_registry {
  pthread_t th;
  char fin;
  struct _lz_timer stale;
  struct _lz_timer active;
  pthread_mutex_t lock;
};

typedef struct _lz_timer lz_timer_t;
typedef struct _lz_timer_registry lz_timer_registry_t;

lz_timer_t *lz_timer_call_after (lz_timer_registry_t *timer, \
		     struct timeval delta, lz_timer_cbk_t cbk, void *data);

int32_t lz_timer_call_cancel(lz_timer_registry_t *timer, \
        lz_timer_t *event);

void *lz_timer_proc (void *data);

lz_timer_registry_t *lz_timer_registry_init (lz_timer_registry_t **timer, void *ctx);

#endif /* _TIMER_H */
