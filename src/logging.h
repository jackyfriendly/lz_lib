/*
 * Author: Jacky
*/


#ifndef __LOGGING_H__
#define __LOGGING_H__


#include <stdint.h>
#include <stdio.h>  
#include <stdarg.h>

#define LZ_PRI_FSBLK       PRId64
#define LZ_PRI_BLKSIZE     "ld"
#  if __WORDSIZE == 64
#    define LZ_PRI_SIZET   "lu"
#    define LZ_PRI_NLINK   "lu"
#  else
#    define LZ_PRI_SIZET   "u"
#    define LZ_PRI_NLINK   "u"
#  endif /* __WORDSIZE */

#define LZ_PRI_DEV         LZ_PRI_FSBLK

typedef enum {
	LZ_LOG_NONE,
	LZ_LOG_CRITICAL,   /* fatal errors */
	LZ_LOG_ERROR,      /* major failures (not necessarily fatal) */
	LZ_LOG_WARNING,    /* info about normal operation */
	LZ_LOG_INFO,       /* Normal information */
#define LZ_LOG_NORMAL LZ_LOG_INFO
	LZ_LOG_DEBUG,      /* internal errors */
    LZ_LOG_TRACE,      /* full trace of operation */
} lz_loglevel_t;

#define LZ_LOG_MAX LZ_LOG_DEBUG

extern lz_loglevel_t lz_log_loglevel;

#define lz_log(dom, levl, fmt...) do {					\
		if (levl <= lz_log_loglevel)				\
			_lz_log (dom, __FILE__, __FUNCTION__, __LINE__, \
				 levl, ##fmt);				\
		if (0) {						\
			printf (fmt);					\
		}							\
} while (0)

/* Log once in LZ_UNIVERSAL_ANSWER times */
#define LZ_LOG_OCCASIONALLY(var, args...) if (!(var++%LZ_UNIVERSAL_ANSWER)) { \
                lz_log (args);                                                \
        }

			
void 
lz_log_logrotate (int signum);

int lz_log_init (const char *filename);
void lz_log_cleanup (void);

int
_lz_log (const char *domain, const char *file, const char *function,
	 int32_t line, lz_loglevel_t level, const char *fmt, ...);

int
lz_log_from_client (const char *msg, char *identifier);

void lz_log_lock (void);
void lz_log_unlock (void);

lz_loglevel_t 
lz_log_get_loglevel (void);
void 
lz_log_set_loglevel (lz_loglevel_t level);

#define LZ_DEBUG(xl, format, args...) \
	lz_log ((xl)->name, LZ_LOG_DEBUG, format, ##args)
#define LZ_INFO(xl, format, args...) \
	lz_log ((xl)->name, LZ_LOG_INFO, format, ##args)
#define LZ_WARNING(xl, format, args...) \
	lz_log ((xl)->name, LZ_LOG_WARNING, format, ##args)
#define LZ_ERROR(xl, format, args...) \
	lz_log ((xl)->name, LZ_LOG_ERROR, format, ##args)

#endif /* __LOGGING_H__ */
