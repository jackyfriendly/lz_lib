/*
 * Authory: Jacky
*/

#ifndef __HASHFN_H__
#define __HASHFN_H__

#include <sys/types.h>
#include <stdint.h>

uint32_t SuperFastHash (const char * data, int32_t len);

uint32_t dm_hashfn (const char *msg, int len);

uint32_t
ReallySimpleHash (char *path, int len);
#endif /* __HASHFN_H__ */
