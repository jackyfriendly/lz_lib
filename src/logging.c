/*
 * Author: Jacky
*/

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <locale.h>
#include <string.h>
#include <stdlib.h>
#include "list.h"
#include "logging.h"
#include "mem-pool.h"
static pthread_mutex_t  logfile_mutex;
static char            *filename = NULL;
static uint8_t          logrotate = 0;

static FILE            *logfile = NULL;
static lz_loglevel_t    loglevel = LZ_LOG_MAX;

lz_loglevel_t           lz_log_loglevel; /* extern'd */
FILE                   *lz_log_logfile;


void 
lz_log_logrotate (int signum)
{
	logrotate = 1;
}


lz_loglevel_t 
lz_log_get_loglevel (void)
{
	return loglevel;
}


void
lz_log_set_loglevel (lz_loglevel_t level)
{
	lz_log_loglevel = loglevel = level;
}


void 
lz_log_fini (void)
{
	pthread_mutex_destroy (&logfile_mutex);
}


int
lz_log_init (const char *file)
{
	if (!file){
		fprintf (stderr, "lz_log_init: no filename specified\n");
		return -1;
	}

	pthread_mutex_init (&logfile_mutex, NULL);

	filename = strdup (file);
	if (!filename) {
		fprintf (stderr, "lz_log_init: strdup error\n");
		return -1;
	}

	logfile = fopen (file, "a");
	if (!logfile){
		fprintf (stderr,
			 "lz_log_init: failed to open logfile \"%s\" (%s)\n",
			 file,
			 strerror (errno));
		return -1;
	}

	lz_log_logfile = logfile;

	return 0;
}


static int __central_log_enabled = 0;

static pthread_t logging_thread;

struct _msg_queue {
        struct list_head msgs;
};

static struct _msg_queue msg_queue;

static pthread_cond_t msg_cond;
static pthread_mutex_t msg_cond_mutex;
static pthread_mutex_t msg_queue_mutex;

struct _log_msg {
        const char *msg;
        struct list_head queue;
};

void 
lz_log_lock (void)
{
	pthread_mutex_lock (&logfile_mutex);
}


void 
lz_log_unlock (void)
{
	pthread_mutex_unlock (&logfile_mutex);
}


void
lz_log_cleanup (void)
{
	pthread_mutex_destroy (&logfile_mutex);
}


int
_lz_log (const char *domain, const char *file, const char *function, int line,
	 lz_loglevel_t level, const char *fmt, ...)
{
    const char  *basename = NULL;
    FILE        *new_logfile = NULL;
    va_list      ap;
    time_t       utime = 0;
    struct tm   *tm = NULL;
    char         timestr[256];

    char        *str1 = NULL;
    char        *str2 = NULL;
    char        *msg  = NULL;
    size_t       len  = 0;
    int          ret  = 0;

    static char *level_strings[] = {"",  /* NONE */
        "C", /* CRITICAL */
        "E", /* ERROR */
        "W", /* WARNING */
        "N", /* NORMAL */
        "D", /* DEBUG */
        "T", /* TRACE */
        ""};

    if (!domain || !file || !function || !fmt) {
        fprintf (stderr, 
                "logging: %s:%s():%d: invalid argument\n", 
                __FILE__, __PRETTY_FUNCTION__, __LINE__);
        return -1;
    }

    if (!logfile) {
        fprintf (stderr, "no logfile set\n");
        return (-1);
    }

    if (logrotate) {
        logrotate = 0;

        new_logfile = fopen (filename, "a");
        if (!new_logfile) {
            lz_log ("logrotate", LZ_LOG_CRITICAL,
                    "failed to open logfile %s (%s)",
                    filename, strerror (errno));
            goto log;
        }

        fclose (logfile);
        lz_log_logfile = logfile = new_logfile;
    }

log:
    utime = time (NULL);
    tm    = localtime (&utime);

    if (level > loglevel) {
        goto out;
    }

    pthread_mutex_lock (&logfile_mutex);
    {
        va_start (ap, fmt);

        strftime (timestr, 256, "%Y-%m-%d %H:%M:%S", tm); 

        basename = strrchr (file, '/');
        if (basename)
            basename++;
        else
            basename = file;

        ret = asprintf (&str1, "[%s] %s [%s:%d:%s] %s: ",
                timestr, level_strings[level],
                basename, line, function,
                domain);
        if (-1 == ret) {
            goto unlock;
        }

        ret = vasprintf (&str2, fmt, ap);
        if (-1 == ret) {
            goto unlock;
        }

        va_end (ap);

        len = strlen (str1);
        msg = malloc (len + strlen (str2) + 1);

        strcpy (msg, str1);
        strcpy (msg + len, str2);

        fprintf (logfile, "%s\n", msg);
        fflush (logfile);
    }
unlock:
    pthread_mutex_unlock (&logfile_mutex);

    if (msg) {
        if ((ret != -1) && __central_log_enabled &&
                ((glusterfs_central_log_flag_get ()) == 0)) {

            glusterfs_central_log_flag_set ();
            {
                lz_log_central (msg);
            }
            glusterfs_central_log_flag_unset ();
        }
        FREE (msg);
    }

    if (str1)
        FREE (str1);

    if (str2)
        FREE (str2);

out:
    return (0);
}


struct _client_log {
        char *identifier;
        FILE *file;
        struct list_head list;
};

struct _client_log *client_logs = NULL;


static void
client_log_init (struct _client_log *cl, char *identifier)
{
        int   ret = 0;
        char *path = NULL;

        cl->identifier = identifier;

        ret = asprintf (&path, "%s.client-%s", filename, identifier);
        if (-1 == ret) {
                return;
        }
        cl->file = fopen (path, "a");
        FREE (path);
        
        INIT_LIST_HEAD (&cl->list);
}


static FILE *
__logfile_for_client (char *identifier)
{
        struct _client_log *client = NULL;

        if (!client_logs) {
                client = (struct _client_log*)CALLOC (1, sizeof (*client));
                client_log_init (client, identifier);

                client_logs = client;
        }

        list_for_each_entry (client, &client_logs->list, list) {
                if (!strcmp (client->identifier, identifier))
                        break;
        }

        if (!client) {
                client = (struct _client_log*)CALLOC (1, sizeof (*client));

                client_log_init (client, identifier);

                list_add_tail (&client->list, &client_logs->list);
        }

        return client->file;
}


int
lz_log_from_client (const char *msg, char *identifier)
{
        FILE *client_log = NULL;

        client_log = __logfile_for_client (identifier);

        fprintf (client_log, "%s\n", msg);
        fflush (client_log);

        return 0;
}
