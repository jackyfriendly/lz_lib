/* rsync-3.0.6/byteorder.h */

/*
 * Simple byteorder handling.
 *
 */

#undef CAREFUL_ALIGNMENT

/* We know that the x86 can handle misalignment and has the same
 * byte order (LSB-first) as the 32-bit numbers we transmit. */

#ifdef __i386__
#define CAREFUL_ALIGNMENT 0
#endif

#ifndef CAREFUL_ALIGNMENT
#define CAREFUL_ALIGNMENT 1
#endif

#define CVAL(buf,pos) (((unsigned char *)(buf))[pos])
#define UVAL(buf,pos) ((uint32_t)CVAL(buf,pos))
#define SCVAL(buf,pos,val) (CVAL(buf,pos) = (val))

#if CAREFUL_ALIGNMENT
#define PVAL(buf,pos) (UVAL(buf,pos)|UVAL(buf,(pos)+1)<<8)
#define IVAL(buf,pos) (PVAL(buf,pos)|PVAL(buf,(pos)+2)<<16)
#define SSVALX(buf,pos,val) (CVAL(buf,pos)=(val)&0xFF,CVAL(buf,pos+1)=(val)>>8)
#define SIVALX(buf,pos,val) (SSVALX(buf,pos,val&0xFFFF),SSVALX(buf,pos+2,val>>16))
#define SIVAL(buf,pos,val) SIVALX((buf),(pos),((uint32_t)(val)))
#else

/* this handles things for architectures like the 386 that can handle
   alignment errors */

/*
   WARNING: This section is dependent on the length of int32
   being correct. set CAREFUL_ALIGNMENT if it is not.
*/

#define IVAL(buf,pos) (*(uint32_t *)((char *)(buf) + (pos)))
#define SIVAL(buf,pos,val) IVAL(buf,pos)=((uint32_t)(val))
#endif

/* The include file for both the MD4 and MD5 routines. */

#define MD5_DIGEST_LEN 16
#define MAX_DIGEST_LEN MD5_DIGEST_LEN

#define CSUM_CHUNK 64

typedef struct {
	uint32_t A, B, C, D;
	uint32_t totalN;          /* bit count, lower 32 bits */
	uint32_t totalN2;         /* bit count, upper 32 bits */
	uint8_t buffer[CSUM_CHUNK];
} md_context;

void md5_begin(md_context *ctx);
void md5_update(md_context *ctx, const uint8_t *input, uint32_t length);
void md5_result(md_context *ctx, uint8_t digest[MD5_DIGEST_LEN]);

void get_md5(uint8_t digest[MD5_DIGEST_LEN], const uint8_t *input, int n);
