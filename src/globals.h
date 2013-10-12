#ifndef _GLOBALS_H
#define _GLOBALS_H
struct _globals_ctx {
    void            *timer;
    void            *event_pool;
    void            *iobuf_pool;
    void            *options;
    size_t          page_size;
};
typedef struct _globals_ctx globals_ctx_t;

int globals_ctx_init(void);

globals_ctx_t * get_globals_ctx();

#endif
