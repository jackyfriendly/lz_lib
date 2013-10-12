/*
 * Author: Jacky
*/

#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <time.h>
#include <locale.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <fcntl.h>
#include "logging.h"
#include "common-utils.h"

typedef int32_t (*rw_op_t)(int32_t fd, char *buf, int32_t size);
typedef int32_t (*rwv_op_t)(int32_t fd, const struct iovec *buf, int32_t size);


struct dnscache6 {
	struct addrinfo *first;
	struct addrinfo *next;
};


int
log_base2 (unsigned long x)
{
        int val = 0;

        while (x > 1) {
                x /= 2;
                val++;
        }

        return val;
}


int32_t
lz_resolve_ip6 (const char *hostname, 
		uint16_t port, 
		int family, 
		void **dnscache, 
		struct addrinfo **addr_info)
{
	int32_t ret = 0;
	struct addrinfo hints;
	struct dnscache6 *cache = NULL;
	char service[NI_MAXSERV], host[NI_MAXHOST];

	if (!hostname) {
		lz_log ("resolver", LZ_LOG_WARNING, "hostname is NULL");
		return -1;
	}

	if (!*dnscache) {
		*dnscache = CALLOC (1, sizeof (struct dnscache6));
	}

	cache = *dnscache;
	if (cache->first && !cache->next) {
		freeaddrinfo(cache->first);
		cache->first = cache->next = NULL;
		lz_log ("resolver", LZ_LOG_TRACE,
			"flushing DNS cache");
	}

	if (!cache->first) {
		char *port_str = NULL;
		lz_log ("resolver", LZ_LOG_TRACE,
			"DNS cache not present, freshly probing hostname: %s",
			hostname);

		memset(&hints, 0, sizeof(hints));
		hints.ai_family   = family;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_flags    = AI_ADDRCONFIG;

		ret = asprintf (&port_str, "%d", port);
                if (-1 == ret) {
                        lz_log ("resolver", LZ_LOG_ERROR, "asprintf failed");
                        return -1;
                }
		if ((ret = getaddrinfo(hostname, port_str, &hints, &cache->first)) != 0) {
			lz_log ("resolver", LZ_LOG_ERROR,
				"getaddrinfo failed (%s)", gai_strerror (ret));

			free (*dnscache);
			*dnscache = NULL;
			free (port_str);
			return -1;
		}
		free (port_str);

		cache->next = cache->first;
	}

	if (cache->next) {
		ret = getnameinfo((struct sockaddr *)cache->next->ai_addr,
				  cache->next->ai_addrlen,
				  host, sizeof (host),
				  service, sizeof (service),
				  NI_NUMERICHOST);
		if (ret != 0) {
			lz_log ("resolver",
				LZ_LOG_ERROR,
				"getnameinfo failed (%s)", gai_strerror (ret));
			goto err;
		}

		lz_log ("resolver", LZ_LOG_TRACE,
			"returning ip-%s (port-%s) for hostname: %s and port: %d",
			host, service, hostname, port);

		*addr_info = cache->next;
	}

	cache->next = cache->next->ai_next;
	if (cache->next) {
		ret = getnameinfo((struct sockaddr *)cache->next->ai_addr,
				  cache->next->ai_addrlen,
				  host, sizeof (host),
				  service, sizeof (service),
				  NI_NUMERICHOST);
		if (ret != 0) {
			lz_log ("resolver",
				LZ_LOG_ERROR,
				"getnameinfo failed (%s)", gai_strerror (ret));
			goto err;
		}

		lz_log ("resolver", LZ_LOG_TRACE,
			"next DNS query will return: ip-%s port-%s", host, service);
	}

	return 0;

err:
	freeaddrinfo (cache->first);
	cache->first = cache->next = NULL;
	free (cache);
	*dnscache = NULL;
	return -1;
}

void
trap (void)
{

}

char *
lz_trim (char *string)
{
	register char *s, *t;
  
	if (string == NULL)
	{
		return NULL;
	}
  
	for (s = string; isspace (*s); s++)
		;
  
	if (*s == 0)
		return s;
  
	t = s + strlen (s) - 1;
	while (t > s && isspace (*t))
		t--;
	*++t = '\0';
  
	return s;
}

int 
lz_strsplit (const char *str, const char *delim, 
	     char ***tokens, int *token_count)
{
	char *_running = NULL;
	char *running = NULL;
	char *token = NULL;
	char **token_list = NULL;
	int count = 0;
	int i = 0;
	int j = 0;
  
	if (str == NULL || delim == NULL || tokens == NULL || token_count == NULL)
	{
		return -1;
	}
  
	if ((_running = strdup (str)) == NULL)
	{
		return -1;
	}
	running = _running;
  
	while ((token = strsep (&running, delim)) != NULL)
	{
		if (token[0] != '\0')
			count++;
	}
	free (_running);
  
	if ((_running = strdup (str)) == NULL)
	{
		return -1;
	}
	running = _running;
  
	if ((token_list = CALLOC (count, sizeof (char *))) == NULL)
	{
		free (_running);
		return -1;
	}
  
	while ((token = strsep (&running, delim)) != NULL)
	{
		if (token[0] == '\0')
			continue;
      
		if ((token_list[i++] = strdup (token)) == NULL)
			goto free_exit;
	}
  
	free (_running);
  
	*tokens = token_list;
	*token_count = count;
	return 0;
  
free_exit:
	free (_running);
	for (j = 0; j < i; j++)
	{
		free (token_list[j]);
	}
	free (token_list);
	return -1;
}

int 
lz_strstr (const char *str, const char *delim, const char *match)
{
        char *tmp      = NULL;
        char *save_ptr = NULL;
        char *tmp_str  = NULL;

        int  ret       = 0;

        tmp_str = strdup (str);

        if (str == NULL || delim == NULL || match == NULL || tmp_str == NULL) {
                ret = -1;
		goto out;
	}


        tmp = strtok_r (tmp_str, delim, &save_ptr);

        while (tmp) {
                ret = strcmp (tmp, match);

                if (ret == 0)
                        break;

                tmp = strtok_r (NULL, delim, &save_ptr);
        }

out:
        if (tmp_str)
                free (tmp_str);

        return ret;

}

int
lz_volume_name_validate (const char *volume_name)
{
	const char *vname = NULL;
  
	if (volume_name == NULL)
	{
		return -1;
	}
  
	if (!isalpha (volume_name[0]))
	{
		return 1;
	}
  
	for (vname = &volume_name[1]; *vname != '\0'; vname++)
	{
		if (!(isalnum (*vname) || *vname == '_'))
			return 1;
	}
  
	return 0;
}


int 
lz_string2time (const char *str, uint32_t *n)
{
	unsigned long value = 0;
	char *tail = NULL;
	int old_errno = 0;
	const char *s = NULL;
  
	if (str == NULL || n == NULL)
	{
		errno = EINVAL;
		return -1;
	}
  
	for (s = str; *s != '\0'; s++)
	{
		if (isspace (*s))
		{
			continue;
		}
		if (*s == '-')
		{
			return -1;
		}
		break;
	}
  
	old_errno = errno;
	errno = 0;
	value = strtol (str, &tail, 0);
  
	if (errno == ERANGE || errno == EINVAL)
	{
		return -1;
	}
  
	if (errno == 0)
	{
		errno = old_errno;
	}
  
	if (!((tail[0] == '\0') || 
	      ((tail[0] == 's') && (tail[1] == '\0')) ||
	      ((tail[0] == 's') && (tail[1] == 'e') && (tail[2] == 'c') && (tail[3] == '\0'))))
	{
		return -1;
	}
  
	*n = value;
  
	return 0;
}


int 
lz_string2percent (const char *str, uint32_t *n)
{
	unsigned long value = 0;
	char *tail = NULL;
	int old_errno = 0;
	const char *s = NULL;
  
	if (str == NULL || n == NULL)
	{
		errno = EINVAL;
		return -1;
	}
  
	for (s = str; *s != '\0'; s++)
	{
		if (isspace (*s))
		{
			continue;
		}
		if (*s == '-')
		{
			return -1;
		}
		break;
	}
  
	old_errno = errno;
	errno = 0;
	value = strtol (str, &tail, 0);
  
	if (errno == ERANGE || errno == EINVAL)
	{
		return -1;
	}
  
	if (errno == 0)
	{
		errno = old_errno;
	}
  
	if (!((tail[0] == '\0') || 
	      ((tail[0] == '%') && (tail[1] == '\0'))))
	{
		return -1;
	}
  
	*n = value;
  
	return 0;
}


static int 
_lz_string2long (const char *str, long *n, int base)
{
	long value = 0;
	char *tail = NULL;
	int old_errno = 0;
  
	if (str == NULL || n == NULL)
	{
		errno = EINVAL;
		return -1;
	}
  
	old_errno = errno;
	errno = 0;
	value = strtol (str, &tail, base);
  
	if (errno == ERANGE || errno == EINVAL)
	{
		return -1;
	}
  
	if (errno == 0)
	{
		errno = old_errno;
	}

	if (tail[0] != '\0')
	{
		/* bala: invalid integer format */
		return -1;
	}
  
	*n = value;
  
	return 0;
}

static int 
_lz_string2ulong (const char *str, unsigned long *n, int base)
{
	unsigned long value = 0;
	char *tail = NULL;
	int old_errno = 0;
	const char *s = NULL;
  
	if (str == NULL || n == NULL)
	{
		errno = EINVAL;
		return -1;
	}
  
	for (s = str; *s != '\0'; s++)
	{
		if (isspace (*s))
		{
			continue;
		}
		if (*s == '-')
		{
			/* bala: we do not support suffixed (-) sign and 
			   invalid integer format */
			return -1;
		}
		break;
	}
  
	old_errno = errno;
	errno = 0;
	value = strtoul (str, &tail, base);
  
	if (errno == ERANGE || errno == EINVAL)
	{
		return -1;
	}
  
	if (errno == 0)
	{
		errno = old_errno;
	}
  
	if (tail[0] != '\0')
	{
		/* bala: invalid integer format */
		return -1;
	}
  
	*n = value;
  
	return 0;
}

static int 
_lz_string2uint (const char *str, unsigned int *n, int base)
{
	unsigned long value = 0;
	char *tail = NULL;
	int old_errno = 0;
	const char *s = NULL;
  
	if (str == NULL || n == NULL)
	{
		errno = EINVAL;
		return -1;
	}
  
	for (s = str; *s != '\0'; s++)
	{
		if (isspace (*s))
		{
			continue;
		}
		if (*s == '-')
		{
			/* bala: we do not support suffixed (-) sign and 
			   invalid integer format */
			return -1;
		}
		break;
	}
  
	old_errno = errno;
	errno = 0;
	value = strtoul (str, &tail, base);
  
	if (errno == ERANGE || errno == EINVAL)
	{
		return -1;
	}
  
	if (errno == 0)
	{
		errno = old_errno;
	}
  
	if (tail[0] != '\0')
	{
		/* bala: invalid integer format */
		return -1;
	}
  
	*n = (unsigned int)value;
  
	return 0;
}

static int 
_lz_string2double (const char *str, double *n)
{
	double value     = 0.0;
	char   *tail     = NULL;
	int    old_errno = 0;
  
	if (str == NULL || n == NULL) {
		errno = EINVAL;
		return -1;
	}
  
	old_errno = errno;
	errno = 0;
	value = strtod (str, &tail);
  
	if (errno == ERANGE || errno == EINVAL)	{
		return -1;
	}
  
	if (errno == 0)	{
		errno = old_errno;
	}
  
	if (tail[0] != '\0') {
		return -1;
	}
  
	*n = value;
  
	return 0;
}

static int 
_lz_string2longlong (const char *str, long long *n, int base)
{
	long long value = 0;
	char *tail = NULL;
	int old_errno = 0;
  
	if (str == NULL || n == NULL)
	{
		errno = EINVAL;
		return -1;
	}
  
	old_errno = errno;
	errno = 0;
	value = strtoll (str, &tail, base);
  
	if (errno == ERANGE || errno == EINVAL)
	{
		return -1;
	}
  
	if (errno == 0)
	{
		errno = old_errno;
	}
  
	if (tail[0] != '\0')
	{
		/* bala: invalid integer format */
		return -1;
	}
  
	*n = value;
  
	return 0;
}

static int 
_lz_string2ulonglong (const char *str, unsigned long long *n, int base)
{
	unsigned long long value = 0;
	char *tail = NULL;
	int old_errno = 0;
	const char *s = NULL;
  
	if (str == NULL || n == NULL)
	{
		errno = EINVAL;
		return -1;
	}
  
	for (s = str; *s != '\0'; s++)
	{
		if (isspace (*s))
		{
			continue;
		}
		if (*s == '-')
		{
			/* bala: we do not support suffixed (-) sign and 
			   invalid integer format */
			return -1;
		}
		break;
	}
  
	old_errno = errno;
	errno = 0;
	value = strtoull (str, &tail, base);
  
	if (errno == ERANGE || errno == EINVAL)
	{
		return -1;
	}
  
	if (errno == 0)
	{
		errno = old_errno;
	}
  
	if (tail[0] != '\0')
	{
		/* bala: invalid integer format */
		return -1;
	}
  
	*n = value;
  
	return 0;
}

int 
lz_string2long (const char *str, long *n)
{
	return _lz_string2long (str, n, 0);
}

int 
lz_string2ulong (const char *str, unsigned long *n)
{
	return _lz_string2ulong (str, n, 0);
}

int 
lz_string2int (const char *str, int *n)
{
	return _lz_string2long (str, (long *) n, 0);
}

int 
lz_string2uint (const char *str, unsigned int *n)
{
	return _lz_string2uint (str, n, 0);
}

int
lz_string2double (const char *str, double *n)
{
	return _lz_string2double (str, n);
}

int 
lz_string2longlong (const char *str, long long *n)
{
	return _lz_string2longlong (str, n, 0);
}

int 
lz_string2ulonglong (const char *str, unsigned long long *n)
{
	return _lz_string2ulonglong (str, n, 0);
}

int 
lz_string2int8 (const char *str, int8_t *n)
{
	long l = 0L;
	int rv = 0;
  
	rv = _lz_string2long (str, &l, 0);
	if (rv != 0)
		return rv;
  
	if (l >= INT8_MIN && l <= INT8_MAX)
	{
		*n = (int8_t) l;
		return 0;
	}
  
	errno = ERANGE;
	return -1;
}

int 
lz_string2int16 (const char *str, int16_t *n)
{
	long l = 0L;
	int rv = 0;
  
	rv = _lz_string2long (str, &l, 0);
	if (rv != 0)
		return rv;
  
	if (l >= INT16_MIN && l <= INT16_MAX)
	{
		*n = (int16_t) l;
		return 0;
	}
  
	errno = ERANGE;
	return -1;
}

int 
lz_string2int32 (const char *str, int32_t *n)
{
	long l = 0L;
	int rv = 0;
  
	rv = _lz_string2long (str, &l, 0);
	if (rv != 0)
		return rv;
  
	if (l >= INT32_MIN && l <= INT32_MAX)
	{
		*n = (int32_t) l;
		return 0;
	}
  
	errno = ERANGE;
	return -1;
}

int 
lz_string2int64 (const char *str, int64_t *n)
{
	long long l = 0LL;
	int rv = 0;
  
	rv = _lz_string2longlong (str, &l, 0);
	if (rv != 0)
		return rv;
  
	if (l >= INT64_MIN && l <= INT64_MAX)
	{
		*n = (int64_t) l;
		return 0;
	}
  
	errno = ERANGE;
	return -1;
}

int 
lz_string2uint8 (const char *str, uint8_t *n)
{
	unsigned long l = 0L;
	int rv = 0;
  
	rv = _lz_string2ulong (str, &l, 0);
	if (rv != 0)
		return rv;
  
	if (l >= 0 && l <= UINT8_MAX)
	{
		*n = (uint8_t) l;
		return 0;
	}
  
	errno = ERANGE;
	return -1;
}

int 
lz_string2uint16 (const char *str, uint16_t *n)
{
	unsigned long l = 0L;
	int rv = 0;
  
	rv = _lz_string2ulong (str, &l, 0);
	if (rv != 0)
		return rv;
  
	if (l >= 0 && l <= UINT16_MAX)
	{
		*n = (uint16_t) l;
		return 0;
	}
  
	errno = ERANGE;
	return -1;
}

int 
lz_string2uint32 (const char *str, uint32_t *n)
{
	unsigned long l = 0L;
	int rv = 0;
  
	rv = _lz_string2ulong (str, &l, 0);
	if (rv != 0)
		return rv;
  
	if (l >= 0 && l <= UINT32_MAX)
	{
		*n = (uint32_t) l;
		return 0;
	}
  
	errno = ERANGE;
	return -1;
}

int 
lz_string2uint64 (const char *str, uint64_t *n)
{
	unsigned long long l = 0ULL;
	int rv = 0;
  
	rv = _lz_string2ulonglong (str, &l, 0);
	if (rv != 0)
		return rv;
  
	if (l >= 0 && l <= UINT64_MAX)
	{
		*n = (uint64_t) l;
		return 0;
	}
  
	errno = ERANGE;
	return -1;
}

int 
lz_string2ulong_base10 (const char *str, unsigned long *n)
{
	return _lz_string2ulong (str, n, 10);
}

int 
lz_string2uint_base10 (const char *str, unsigned int *n)
{
	return _lz_string2uint (str,  n, 10);
}

int 
lz_string2uint8_base10 (const char *str, uint8_t *n)
{
	unsigned long l = 0L;
	int rv = 0;
  
	rv = _lz_string2ulong (str, &l, 10);
	if (rv != 0)
		return rv;
  
	if (l >= 0 && l <= UINT8_MAX)
	{
		*n = (uint8_t) l;
		return 0;
	}
  
	errno = ERANGE;
	return -1;
}

int 
lz_string2uint16_base10 (const char *str, uint16_t *n)
{
	unsigned long l = 0L;
	int rv = 0;
  
	rv = _lz_string2ulong (str, &l, 10);
	if (rv != 0)
		return rv;
  
	if (l >= 0 && l <= UINT16_MAX)
	{
		*n = (uint16_t) l;
		return 0;
	}
  
	errno = ERANGE;
	return -1;
}

int 
lz_string2uint32_base10 (const char *str, uint32_t *n)
{
	unsigned long l = 0L;
	int rv = 0;
  
	rv = _lz_string2ulong (str, &l, 10);
	if (rv != 0)
		return rv;
  
	if (l >= 0 && l <= UINT32_MAX)
	{
		*n = (uint32_t) l;
		return 0;
	}
  
	errno = ERANGE;
	return -1;
}

int 
lz_string2uint64_base10 (const char *str, uint64_t *n)
{
	unsigned long long l = 0ULL;
	int rv = 0;
  
	rv = _lz_string2ulonglong (str, &l, 10);
	if (rv != 0)
		return rv;
  
	if (l >= 0 && l <= UINT64_MAX)
	{
		*n = (uint64_t) l;
		return 0;
	}
  
	errno = ERANGE;
	return -1;
}

int 
lz_string2bytesize (const char *str, uint64_t *n)
{
	uint64_t value = 0ULL;
	char *tail = NULL;
	int old_errno = 0;
	const char *s = NULL;
  
	if (str == NULL || n == NULL)
	{
		errno = EINVAL;
		return -1;
	}
  
	for (s = str; *s != '\0'; s++)
	{
		if (isspace (*s))
		{
			continue;
		}
		if (*s == '-')
		{
			/* bala: we do not support suffixed (-) sign and 
			   invalid integer format */
			return -1;
		}
		break;
	}
  
	old_errno = errno;
	errno = 0;
	value = strtoull (str, &tail, 10);
  
	if (errno == ERANGE || errno == EINVAL)
	{
		return -1;
	}
  
	if (errno == 0)
	{
		errno = old_errno;
	}
  
	if (tail[0] != '\0')
	{
		if (strcasecmp (tail, LZ_UNIT_KB_STRING) == 0)
		{
			value *= LZ_UNIT_KB;
		}
		else if (strcasecmp (tail, LZ_UNIT_MB_STRING) == 0)
		{
			value *= LZ_UNIT_MB;
		}
		else if (strcasecmp (tail, LZ_UNIT_GB_STRING) == 0)
		{
			value *= LZ_UNIT_GB;
		}
		else if (strcasecmp (tail, LZ_UNIT_TB_STRING) == 0)
		{
			value *= LZ_UNIT_TB;
		}
		else if (strcasecmp (tail, LZ_UNIT_PB_STRING) == 0)
		{
			value *= LZ_UNIT_PB;
		}
		else 
		{
			/* bala: invalid integer format */
			return -1;
		}
	}
  
	*n = value;
  
	return 0;
}

int64_t 
lz_str_to_long_long (const char *number)
{
	int64_t unit = 1;
	int64_t ret = 0;
	char *endptr = NULL ;
	if (!number)
		return 0;

	ret = strtoll (number, &endptr, 0);

	if (endptr) {
		switch (*endptr) {
		case 'G':
		case 'g':
			if ((* (endptr + 1) == 'B') ||(* (endptr + 1) == 'b'))
				unit = 1024 * 1024 * 1024;
			break;
		case 'M':
		case 'm':
			if ((* (endptr + 1) == 'B') ||(* (endptr + 1) == 'b'))
				unit = 1024 * 1024;
			break;
		case 'K':
		case 'k':
			if ((* (endptr + 1) == 'B') ||(* (endptr + 1) == 'b'))
				unit = 1024;
			break;
		case '%':
			unit = 1;
			break;
		default:
			unit = 1;
			break;
		}
	}
	return ret * unit;
}

int 
lz_string2boolean (const char *str, lz_boolean_t *b)
{
	if (str == NULL) {
		return -1;
	}
	
	if ((strcasecmp (str, "1") == 0) || 
	    (strcasecmp (str, "on") == 0) || 
	    (strcasecmp (str, "yes") == 0) || 
	    (strcasecmp (str, "true") == 0) || 
	    (strcasecmp (str, "enable") == 0)) {
		*b = _lz_true;
		return 0;
	}
	
	if ((strcasecmp (str, "0") == 0) || 
	    (strcasecmp (str, "off") == 0) || 
	    (strcasecmp (str, "no") == 0) || 
	    (strcasecmp (str, "false") == 0) || 
	    (strcasecmp (str, "disable") == 0)) {
		*b = _lz_false;
		return 0;
	}
	
	return -1;
}

int lz_buffer2string(char* buffer,int32_t len)
{
    int i;
    if(buffer==NULL)
    {
        lz_log("common-utils",LZ_LOG_DEBUG,"parameter is fail");
        return(-1);
    }
    for(i=0;i<len;i++)
    {
        if(i%16 == 0)
        {
            fprintf(stderr,"\n");
        }
        fprintf(stderr,"[%02x]", (unsigned char)buffer[i]);
    }
    fprintf(stderr,"\n");
    return(0);
}

int 
lz_lockfd (int fd)
{
	struct flock fl;
	
	fl.l_type = F_WRLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = 0;
	fl.l_len = 0;
	
	return fcntl (fd, F_SETLK, &fl);
}


int 
lz_unlockfd (int fd)
{
	struct flock fl;
	
	fl.l_type = F_UNLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = 0;
	fl.l_len = 0;
	
	return fcntl (fd, F_SETLK, &fl);
}
  
static void
compute_checksum (char *buf, size_t size, uint32_t *checksum)
{
        int  ret = -1;
        char *checksum_buf = NULL;

        checksum_buf = (char *)(checksum);

        if (!(*checksum)) {
                checksum_buf [0] = 0xba;
                checksum_buf [1] = 0xbe;
                checksum_buf [2] = 0xb0;
                checksum_buf [3] = 0x0b;                
        }

        for (ret = 0; ret < (size - 4); ret += 4) {
                checksum_buf[0] ^= (buf[ret]);
                checksum_buf[1] ^= (buf[ret + 1] << 1) ;
                checksum_buf[2] ^= (buf[ret + 2] << 2);
                checksum_buf[3] ^= (buf[ret + 3] << 3);
        }

        for (ret = 0; ret <= (size % 4); ret++) {
                checksum_buf[ret] ^= (buf[(size - 4) + ret] << ret);
        }
        
        return;
}

#define LZ_CHECKSUM_BUF_SIZE 1024

int
get_checksum_for_file (int fd, uint32_t *checksum) 
{
        int ret = -1;
        char buf[LZ_CHECKSUM_BUF_SIZE] = {0,};

        /* goto first place */
        lseek (fd, 0L, SEEK_SET);
        do {
                ret = read (fd, &buf, LZ_CHECKSUM_BUF_SIZE);
                if (ret > 0)
                        compute_checksum (buf, LZ_CHECKSUM_BUF_SIZE, 
                                          checksum);
        } while (ret > 0);

        /* set it back */
        lseek (fd, 0L, SEEK_SET);

        return ret;
}
