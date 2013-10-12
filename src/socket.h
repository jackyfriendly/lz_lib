/*
 * Author: Jacky
*/

#ifndef _SOCKET_H
#define _SOCKET_H


#include "event.h"
#include "transport.h"
#include "logging.h"
#include "dict.h"
#include "mem-pool.h"
#include "iobuf.h"
#ifndef MAX_IOVEC
#define MAX_IOVEC 16
#endif /* MAX_IOVEC */

#define DEFAULT_SOCKET_LISTEN_PORT 6996

/* This is the size set through setsockopt for
 * both the TCP receive window size and the
 * send buffer size.
 * Till the time iobuf size becomes configurable, this size is set to include
 * two iobufs + the GlusterFS protocol headers.
 * Linux allows us to over-ride the max values for the system.
 * Should we over-ride them? Because if we set a value larger than the default
 * setsockopt will fail. Having larger values might be beneficial for
 * IB links.
 */
#define DEFAULT_SOCKET_WINDOW_SIZE   (512 * UNIT_KB)
#define MAX_SOCKET_WINDOW_SIZE       (1 * UNIT_MB)
#define MIN_SOCKET_WINDOW_SIZE       (128 * UNIT_KB)

typedef enum {
    SOCKET_PROTO_STATE_NADA = 0,
    SOCKET_PROTO_STATE_HEADER_COMING,
    SOCKET_PROTO_STATE_HEADER_CAME,
    SOCKET_PROTO_STATE_DATA_COMING,
    SOCKET_PROTO_STATE_DATA_CAME,
    SOCKET_PROTO_STATE_COMPLETE,
} socket_proto_state_t;

struct socket_header {
    char     colonO[3];
    uint32_t size1;
    uint32_t size2;
    char     version;
} __attribute__((packed));


struct ioq {
    union {
        struct list_head list;
        struct {
            struct ioq    *next;
            struct ioq    *prev;
        };
    };
    struct socket_header  header;
    struct iovec       vector[MAX_IOVEC];
    int                count;
    struct iovec      *pending_vector;
    int                pending_count;
    char              *buf;
    struct iobref     *iobref;
};


typedef struct {
    int32_t                sock;
    int32_t                idx;
    unsigned char          connected; // -1 = not connected. 0 = in progress. 1 = connected
    char                   bio;
    char                   connect_finish_log;
    char                   submit_log;
    union {
        struct list_head     ioq;
        struct {
            struct ioq        *ioq_next;
            struct ioq        *ioq_prev;
        };
    };
    struct {
        int                  state;
        struct socket_header header;
        char                *hdr_p;
        size_t               hdrlen;
        struct iobuf        *iobuf;
        char                *buf_p;
        size_t               buflen;
        struct iovec         vector[2];
        int                  count;
        struct iovec        *pending_vector;
        int                  pending_count;
    } incoming;
    pthread_mutex_t        lock;
    int                    windowsize;
    char                   lowlat;
    char                   nodelay;
} socket_private_t;

int socket_connect (transport_t *this);

int socket_listen (transport_t *this);

int socket_disconnect (transport_t *this);

int socket_submit (transport_t *this, char *buf, int len, \
        struct iovec *vector, int count, struct iobref *iobref);

int socket_receive (transport_t *this, char **hdr_p, size_t *hdrlen_p, \
        struct iobuf **iobuf_p);

void socket_fini (transport_t *this);

int32_t socket_init (transport_t *this);

struct transport_ops epoll_ops = {
    .listen     = socket_listen,
    .connect    = socket_connect,
    .disconnect = socket_disconnect,
    .submit     = socket_submit,
    .receive    = socket_receive
};

#endif
