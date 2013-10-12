#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "iobuf.h"
#include "event.h"
#include "mem-pool.h"
#include "globals.h"

static globals_ctx_t *global_ctx;

int globals_ctx_init()
{
    int ret = 0;
    return ret;
}

globals_ctx_t *get_globals_ctx()
{
    return global_ctx;
}
