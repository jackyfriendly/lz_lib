/*
 * Author: Jacky
*/

#ifndef __CHECKSUM_H__
#define __CHECKSUM_H__

uint32_t
rsync_weak_checksum (char *buf, int32_t len);

void
rsync_strong_checksum (char *buf, int32_t len, uint8_t *sum);

#endif /* __CHECKSUM_H__ */
