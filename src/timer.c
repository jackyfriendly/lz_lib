/*
 * Author: Jacky
*/

#include "timer.h"
#include "logging.h"
#include "common-utils.h"

#define TS(tv) ((((unsigned long long) tv.tv_sec) * 1000000) + (tv.tv_usec))

lz_timer_t *
lz_timer_call_after (lz_timer_registry_t *timer, struct timeval delta,
                     lz_timer_cbk_t cbk, void *data)
{
    lz_timer_registry_t *reg = NULL;
    lz_timer_t *event = NULL;
    lz_timer_t *trav = NULL;
    unsigned long long at = 0L;

    if (timer == NULL)
    {
        lz_log ("timer", LZ_LOG_ERROR, "invalid argument");
        return NULL;
    }

    reg = timer;


    event = CALLOC (1, sizeof (*event));
    if (!event) {
        lz_log ("timer", LZ_LOG_CRITICAL, "Not enough memory");
        return NULL;
    }
    gettimeofday (&event->at, NULL);
    event->at.tv_usec = ((event->at.tv_usec + delta.tv_usec) % 1000000);
    event->at.tv_sec += ((event->at.tv_usec + delta.tv_usec) / 1000000);
    event->at.tv_sec += delta.tv_sec;
    at = TS (event->at);
    event->cbk = cbk;
    event->data = data;
    pthread_mutex_lock (&reg->lock);
    {
        trav = reg->active.prev;
        while (trav != &reg->active) {
            if (TS (trav->at) < at)
                break;
            trav = trav->prev;
        }
        event->prev = trav;
        event->next = event->prev->next;
        event->prev->next = event;
        event->next->prev = event;
    }
    pthread_mutex_unlock (&reg->lock);
    return event;
}

int32_t
lz_timer_call_stale (lz_timer_registry_t *reg,
                     lz_timer_t *event)
{
    if (reg == NULL || event == NULL)
    {
        lz_log ("timer", LZ_LOG_ERROR, "invalid argument");
        return 0;
    }

    event->next->prev = event->prev;
    event->prev->next = event->next;
    event->next = &reg->stale;
    event->prev = event->next->prev;
    event->next->prev = event;
    event->prev->next = event;

    return 0;
}

int32_t
lz_timer_call_cancel (lz_timer_registry_t *timer,
                      lz_timer_t *event)
{
    lz_timer_registry_t *reg = NULL;

    if (timer == NULL || event == NULL)
    {
        lz_log ("timer", LZ_LOG_ERROR, "invalid argument");
        return 0;
    }

    reg = timer;

    pthread_mutex_lock (&reg->lock);
    {
        event->next->prev = event->prev;
        event->prev->next = event->next;
    }
    pthread_mutex_unlock (&reg->lock);

    FREE (event);
    return 0;
}

void *
lz_timer_proc (void *ctx)
{
    lz_timer_registry_t *reg = NULL;

    if (ctx == NULL)
    {
        lz_log ("timer", LZ_LOG_ERROR, "invalid argument");
        return NULL;
    }

    reg = ctx;

    while (!reg->fin) {
        unsigned long long now;
        struct timeval now_tv;
        lz_timer_t *event = NULL;

        gettimeofday (&now_tv, NULL);
        now = TS (now_tv);
        while (1) {
            unsigned long long at;
            char need_cbk = 0;

            pthread_mutex_lock (&reg->lock);
            {
                event = reg->active.next;
                at = TS (event->at);
                if (event != &reg->active && now >= at) {
                    need_cbk = 1;
                    lz_timer_call_stale (reg, event);
                }
            }
            pthread_mutex_unlock (&reg->lock);
            if (need_cbk)
                event->cbk (event->data);

            else
                break;
        }
        usleep (1000000);
    }

    pthread_mutex_lock (&reg->lock);
    {
        while (reg->active.next != &reg->active) {
            lz_timer_call_cancel (ctx, reg->active.next);
        }

        while (reg->stale.next != &reg->stale) {
            lz_timer_call_cancel (ctx, reg->stale.next);
        }
    }
    pthread_mutex_unlock (&reg->lock);
    pthread_mutex_destroy (&reg->lock);

    return NULL;
}

lz_timer_registry_t *
lz_timer_registry_init (lz_timer_registry_t **timer, void *ctx)
{
        lz_timer_registry_t *reg = NULL;
        if (timer == NULL)
        {
                lz_log ("timer", LZ_LOG_ERROR, "invalid argument");
                return NULL;
        }
        reg = *timer;
        if (!reg) {

                reg = CALLOC (1, sizeof (*reg));
                ERR_ABORT (reg);
                pthread_mutex_init (&reg->lock, NULL);
                reg->active.next = &reg->active;
                reg->active.prev = &reg->active;
                reg->stale.next = &reg->stale;
                reg->stale.prev = &reg->stale;

                pthread_create (&reg->th, NULL, lz_timer_proc, reg);
        }
        return reg;
}
