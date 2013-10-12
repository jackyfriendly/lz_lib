/*
 * Author: Jacky
*/

#ifndef _EVENT_H_
#define _EVENT_H_

#include <pthread.h>

struct event_pool;
struct event_ops;
struct event_data {
  int fd;
  int idx;
} __attribute__ ((__packed__));


typedef int (*event_handler_t) (int fd, int idx, void *data,
				int poll_in, int poll_out, int poll_err);
typedef enum {       
  LZ_EVENT_POLLIN = 1,
  LZ_EVENT_POLLOUT,      
  LZ_EVENT_POLLERR,      
  LZ_EVENT_TRANSPORT_CLEANUP,
  LZ_EVENT_TRANSPORT_CONNECTED,
} lz_event_t;

struct event_pool {
  struct event_ops *ops;

  int fd;
  int breaker[2];

  int count;
  struct {
    int fd;
    int events;
    void *data;
    event_handler_t handler;
  } *reg;

  int used;
  int idx_cache;
  int changed;

  pthread_mutex_t mutex;
  pthread_cond_t cond;

  void *evcache;
  int evcache_size;
};

struct event_ops {
  struct event_pool * (*new) (int count);

  int (*event_register) (struct event_pool *event_pool, int fd,
			 event_handler_t handler,
			 void *data, int poll_in, int poll_out);

  int (*event_select_on) (struct event_pool *event_pool, int fd, int idx,
			  int poll_in, int poll_out);

  int (*event_unregister) (struct event_pool *event_pool, int fd, int idx);

  int (*event_dispatch) (struct event_pool *event_pool);
};

struct event_pool * event_pool_new (int count);
int event_select_on (struct event_pool *event_pool, int fd, int idx,
		     int poll_in, int poll_out);
int event_register (struct event_pool *event_pool, int fd,
		    event_handler_t handler,
		    void *data, int poll_in, int poll_out);
int event_unregister (struct event_pool *event_pool, int fd, int idx);
int event_dispatch (struct event_pool *event_pool);

#endif /* _EVENT_H_ */
