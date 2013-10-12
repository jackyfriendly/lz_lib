/*
 * Author: Jacky
*/

#ifndef _COMMON_UTILS_H
#define _COMMON_UTILS_H

#include <stdint.h>
#include <sys/uio.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <inttypes.h>

void trap (void);

#define LZ_UNIVERSAL_ANSWER 42    /* :O */
#define UNIX_PATH_MAX 108
/* To solve type punned error */
#define VOID(ptr) ((void **) ((void *) ptr))

#include "logging.h"
#include "locking.h"
#include "mem-pool.h"


#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))
#define roof(a,b) ((((a)+(b)-1)/((b)?(b):1))*(b))
#define floor(a,b) (((a)/((b)?(b):1))*(b))


#define LZ_UNIT_KB    1024ULL
#define LZ_UNIT_MB    1048576ULL
#define LZ_UNIT_GB    1073741824ULL
#define LZ_UNIT_TB    1099511627776ULL
#define LZ_UNIT_PB    1125899906842624ULL

#define LZ_UNIT_KB_STRING    "KB"
#define LZ_UNIT_MB_STRING    "MB"
#define LZ_UNIT_GB_STRING    "GB"
#define LZ_UNIT_TB_STRING    "TB"
#define LZ_UNIT_PB_STRING    "PB"


#define ERR_ABORT(ptr)				\
	if (ptr == NULL)  {			\
		abort ();			\
	}                     

enum _lz_boolean 
{
	_lz_false = 0, 
	_lz_true = 1
};

typedef enum _lz_boolean lz_boolean_t;

in_addr_t lz_resolve_ip (const char *hostname, void **dnscache);

#define VECTORSIZE(count) (count * (sizeof (struct iovec)))

#define STRLEN_0(str) (strlen(str) + 1)

#define LZ_VALIDATE_OR_GOTO(name,arg,label)   do {		\
		if (!arg) {					\
			errno = EINVAL;   			\
			lz_log (name, LZ_LOG_ERROR,		\
				"invalid argument: " #arg);	\
			goto label;				\
		}						\
	} while (0); 

#define LZ_VALIDATE_OR_GOTO_WITH_ERROR(name, arg, label, errno, error) do { \
                if (!arg) {                                                 \
                        errno = error;                                  \
                        lz_log (name, LZ_LOG_ERROR,                     \
                                "invalid argument: " #arg);             \
                        goto label;                                     \
                }                                                       \
        }while (0);

#define LZ_VALIDATE_ABSOLUTE_PATH_OR_GOTO(name,arg,label)       \
        do {                                                    \
                LZ_VALIDATE_OR_GOTO (name, arg, label);         \
                if ((arg[0]) != '/') {                          \
                        errno = EINVAL;                         \
			lz_log (name, LZ_LOG_ERROR,	        \
				"invalid argument: " #arg);	\
                        goto label;                             \
                }                                               \
	} while (0);

#define LZ_FILE_CONTENT_REQUESTED(_xattr_req,_content_limit) \
	(dict_get_uint64 (_xattr_req, "glusterfs.content", _content_limit) == 0)

static inline void
iov_free (struct iovec *vector, int count)
{
	int i;

	for (i = 0; i < count; i++)
		FREE (vector[i].iov_base);

	FREE (vector);
}


static inline int
iov_length (const struct iovec *vector, int count)
{
	int     i = 0;
	size_t  size = 0;

	for (i = 0; i < count; i++)
		size += vector[i].iov_len;
	return size;
}


static inline struct iovec *
iov_dup (struct iovec *vector, int count)
{
	int           bytecount = 0;
	int           i;
	struct iovec *newvec = NULL;

	bytecount = (count * sizeof (struct iovec));
	newvec = MALLOC (bytecount);
	if (!newvec)
		return NULL;

	for (i = 0; i < count; i++) {
		newvec[i].iov_len  = vector[i].iov_len;
		newvec[i].iov_base = vector[i].iov_base;
	}

	return newvec;
}


static inline int
iov_subset (struct iovec *orig, int orig_count,
	    off_t src_offset, off_t dst_offset,
	    struct iovec *new)
{
	int    new_count = 0;
	int    i;
	off_t  offset = 0;
	size_t start_offset = 0;
	size_t end_offset = 0;


	for (i = 0; i < orig_count; i++) {
		if ((offset + orig[i].iov_len < src_offset)
		    || (offset > dst_offset)) {
			goto not_subset;
		}

		if (!new) {
			goto count_only;
		}

		start_offset = 0;
		end_offset = orig[i].iov_len;

		if (src_offset >= offset) {
			start_offset = (src_offset - offset);
		}

		if (dst_offset <= (offset + orig[i].iov_len)) {
			end_offset = (dst_offset - offset);
		}

		new[new_count].iov_base = orig[i].iov_base + start_offset;
		new[new_count].iov_len = end_offset - start_offset;

	count_only:
		new_count++;

	not_subset:
		offset += orig[i].iov_len;
	}

	return new_count;
}


static inline void
iov_unload (char *buf, const struct iovec *vector, int count)
{
	int i;
	int copied = 0;

	for (i = 0; i < count; i++) {
		memcpy (buf + copied, vector[i].iov_base, vector[i].iov_len);
		copied += vector[i].iov_len;
	}
}


static inline int
mem_0filled (const char *buf, size_t size)
{
	int i = 0;
	int ret = 0;

	for (i = 0; i < size; i++) {
		ret = buf[i];
		if (ret)
			break;
	}

	return ret;
}


static inline int
iov_0filled (struct iovec *vector, int count)
{
	int i = 0;
	int ret = 0;

	for (i = 0; i < count; i++) {
		ret = mem_0filled (vector[i].iov_base, vector[i].iov_len);
		if (ret)
			break;
	}

	return ret;
}


static inline void *
memdup (const void *ptr, size_t size)
{
	void *newptr = NULL;

	newptr = MALLOC (size);
	if (!newptr)
		return NULL;

	memcpy (newptr, ptr, size);
	return newptr;
}


char *lz_trim (char *string);
int lz_strsplit (const char *str, const char *delim, 
		 char ***tokens, int *token_count);
int lz_volume_name_validate (const char *volume_name);

int lz_string2long (const char *str, long *n);
int lz_string2ulong (const char *str, unsigned long *n);
int lz_string2int (const char *str, int *n);
int lz_string2uint (const char *str, unsigned int *n);
int lz_string2double (const char *str, double *n);
int lz_string2longlong (const char *str, long long *n);
int lz_string2ulonglong (const char *str, unsigned long long *n);

int lz_string2int8 (const char *str, int8_t *n);
int lz_string2int16 (const char *str, int16_t *n);
int lz_string2int32 (const char *str, int32_t *n);
int lz_string2int64 (const char *str, int64_t *n);
int lz_string2uint8 (const char *str, uint8_t *n);
int lz_string2uint16 (const char *str, uint16_t *n);
int lz_string2uint32 (const char *str, uint32_t *n);
int lz_string2uint64 (const char *str, uint64_t *n);

int lz_strstr (const char *str, const char *delim, const char *match);

int lz_string2ulong_base10 (const char *str, unsigned long *n);
int lz_string2uint_base10 (const char *str, unsigned int *n);
int lz_string2uint8_base10 (const char *str, uint8_t *n);
int lz_string2uint16_base10 (const char *str, uint16_t *n);
int lz_string2uint32_base10 (const char *str, uint32_t *n);
int lz_string2uint64_base10 (const char *str, uint64_t *n);

int lz_string2bytesize (const char *str, uint64_t *n);

int lz_string2boolean (const char *str, lz_boolean_t *b);
int lz_string2percent (const char *str, uint32_t *n);
int lz_string2time (const char *str, uint32_t *n);
int lz_buffer2string(char* buffer,int32_t len);
int lz_lockfd (int fd);
int lz_unlockfd (int fd);

int get_checksum_for_file (int fd, uint32_t *checksum);
int log_base2 (unsigned long x);

#endif /* _COMMON_UTILS_H */

