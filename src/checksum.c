/*
 * Author: Jacky
*/

#include <inttypes.h>

#include "md5.h"
#include "checksum.h"


/*
 * The "weak" checksum required for the rsync algorithm,
 * adapted from the rsync source code. The following comment
 * appears there:
 *
 * "a simple 32 bit checksum that can be upadted from either end
 *  (inspired by Mark Adler's Adler-32 checksum)"
 */

uint32_t
rsync_weak_checksum (char *buf1, int32_t len)
{
        int32_t i;
        uint32_t s1, s2;

        signed char *buf = (signed char *) buf1;
        uint32_t csum;

        s1 = s2 = 0;
        for (i = 0; i < (len-4); i+=4) {
                s2 += 4*(s1 + buf[i]) + 3*buf[i+1] + 2*buf[i+2] + buf[i+3];

                s1 += buf[i+0] + buf[i+1] + buf[i+2] + buf[i+3];
        }

        for (; i < len; i++) {
                s1 += buf[i];
                s2 += s1;
        }

        csum = (s1 & 0xffff) + (s2 << 16);

        return csum;
}


/*
 * The "strong" checksum required for the rsync algorithm,
 * adapted from the rsync source code.
 */

void
rsync_strong_checksum (char *buf, int32_t len, uint8_t *sum)
{
        md_context m;

        md5_begin (&m);
        md5_update (&m, (unsigned char *) buf, len);
        md5_result (&m, (unsigned char *) sum);

        return;
}
