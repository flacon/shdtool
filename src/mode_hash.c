/*  mode_hash.c - hash mode module
 *  Portions copyright (C) 2000-2009  Jason Jordan <shnutils@freeshell.org>
 *  This mode module is based on the GNU MD5/SHA1 implementation.  See the
 *  original author/copyright info below.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include "mode.h"

CVSID("$Id: mode_hash.c,v 1.93 2009/03/17 17:23:05 jason Exp $")

static bool hash_main(int,char **);
static void hash_help(void);

mode_module mode_hash = {
  "hash",
  "shnhash",
  "Computes the MD5 or SHA1 fingerprint of PCM WAVE data",
  CVSIDSTR,
  FALSE,
  hash_main,
  hash_help
};

enum {
  HASH_MD5,
  HASH_SHA1
};

#define COMPOSITE "composite"

static unsigned long maxbytes;
static unsigned char audio_hash[32];

static bool composite_hash = FALSE;
static int remaining_bytes = 0;
static int num_processed = 0;
static int numfiles;
static int hash_algorithm = HASH_MD5;
static progress_info proginfo;

static wave_info **files;

/* shntool: modified GNU coreutils 5.93 md5/sha1 routines below */

/* md5.c - Functions to compute MD5 message digest of files or memory blocks
   according to the definition of MD5 in RFC 1321 from April 1992.
   Copyright (C) 1995, 1996, 2001, 2003, 2004, 2005 Free Software Foundation, Inc.
   NOTE: The canonical source of this file is maintained with the GNU C
   Library.  Bugs can be reported to bug-glibc@prep.ai.mit.edu.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 2, or (at your option) any
   later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

/* Written by Ulrich Drepper <drepper@gnu.ai.mit.edu>, 1995.  */

#include "md5.h"

#ifdef _LIBC
/* We need to keep the namespace clean so define the MD5 function
   protected using leading __ .  */
# define md5_init_ctx __md5_init_ctx
# define md5_process_block __md5_process_block
# define md5_process_bytes __md5_process_bytes
# define md5_finish_ctx __md5_finish_ctx
# define md5_read_ctx __md5_read_ctx
# define md5_stream __md5_stream
# define md5_buffer __md5_buffer
#endif

/* shntool: use different swapping macros for MD5/SHA1 */
#ifdef WORDS_BIGENDIAN
# define SWAP_MD5(n)							\
    (((n) << 24) | (((n) & 0xff00) << 8) | (((n) >> 8) & 0xff00) | ((n) >> 24))
# define SWAP_SHA1(n) (n)
#else
# define SWAP_MD5(n) (n)
# define SWAP_SHA1(n)							\
    (((n) << 24) | (((n) & 0xff00) << 8) | (((n) >> 8) & 0xff00) | ((n) >> 24))
#endif

/* shntool: begin common md5/sha1 definitions */
#define BLOCKSIZE 4096
#if BLOCKSIZE % 64 != 0
# error "invalid BLOCKSIZE"
#endif

/* This array contains the bytes used to pad the buffer to the next
   64-byte boundary.  (RFC 1321, 3.1: Step 1)  */
static const unsigned char fillbuf[64] = { 0x80, 0 /* , 0, 0, ...  */ };

char global_buffer[BLOCKSIZE + 72];
/* shntool: end common md5/sha1 definitions */

/* shntool: global md5 context */
struct md5_ctx md5_global_ctx;

/* Initialize structure containing state of computation.
   (RFC 1321, 3.3: Step 3)  */
void
md5_init_ctx (struct md5_ctx *ctx)
{
  ctx->A = 0x67452301;
  ctx->B = 0xefcdab89;
  ctx->C = 0x98badcfe;
  ctx->D = 0x10325476;

  ctx->total[0] = ctx->total[1] = 0;
  ctx->buflen = 0;
}

/* Put result from CTX in first 16 bytes following RESBUF.  The result
   must be in little endian byte order.

   IMPORTANT: On some systems it is required that RESBUF is correctly
   aligned for a 32 bits value.  */
void *
md5_read_ctx (const struct md5_ctx *ctx, void *resbuf)
{
  ((md5_uint32 *) resbuf)[0] = SWAP_MD5 (ctx->A);
  ((md5_uint32 *) resbuf)[1] = SWAP_MD5 (ctx->B);
  ((md5_uint32 *) resbuf)[2] = SWAP_MD5 (ctx->C);
  ((md5_uint32 *) resbuf)[3] = SWAP_MD5 (ctx->D);

  return resbuf;
}

/* Process the remaining bytes in the internal buffer and the usual
   prolog according to the standard and write the result to RESBUF.

   IMPORTANT: On some systems it is required that RESBUF is correctly
   aligned for a 32 bits value.  */
void *
md5_finish_ctx (struct md5_ctx *ctx, void *resbuf)
{
  /* Take yet unprocessed bytes into account.  */
  md5_uint32 bytes = ctx->buflen;
  size_t pad;

  /* Now count remaining bytes.  */
  ctx->total[0] += bytes;
  if (ctx->total[0] < bytes)
    ++ctx->total[1];

  pad = bytes >= 56 ? 64 + 56 - bytes : 56 - bytes;
  memcpy (&ctx->buffer[bytes], fillbuf, pad);

  /* Put the 64-bit file length in *bits* at the end of the buffer.  */
  *(md5_uint32 *) &ctx->buffer[bytes + pad] = SWAP_MD5 (ctx->total[0] << 3);
  *(md5_uint32 *) &ctx->buffer[bytes + pad + 4] = SWAP_MD5 ((ctx->total[1] << 3) |
							(ctx->total[0] >> 29));

  /* Process last bytes.  */
  md5_process_block (ctx->buffer, bytes + pad + 8, ctx);

  return md5_read_ctx (ctx, resbuf);
}

/* Compute MD5 message digest for bytes read from STREAM.  The
   resulting message digest number will be written into the 16 bytes
   beginning at RESBLOCK.  */
int
md5_stream (FILE *stream, void *resblock)
{
  size_t sum;
  unsigned long totalbytes = 0;  /* shntool */

  /* shntool: */
  if (0 == maxbytes)
    return 0;

  /* Initialize the computation context.  */
  /* shntool: now done outside of this function */
/*
  md5_init_ctx (&md5_global_ctx);
*/

  /* Iterate over full file contents.  */
  /* shntool: stop after WAVE data is read */
  while (1)
    {
      /* We read the file in blocks of BLOCKSIZE bytes.  One call of the
	 computation function processes the whole buffer so that with the
	 next round of the loop another block can be read.  */
      size_t n;
      sum = 0;

      /* Read block.  Take care for partial reads.  */

      while (1)
	{
	  n = read_n_bytes(stream, (unsigned char *)(global_buffer + sum), BLOCKSIZE - sum, &proginfo);

	  sum += n;
      totalbytes += n;  /* shntool */

      /* shntool: if total bytes read exceeded the WAVE data size, discard the rest and adjust sum */
      if (totalbytes > maxbytes) {
        sum -= (totalbytes - maxbytes);
        totalbytes = maxbytes;
      }

	  if (sum == BLOCKSIZE)
	    break;

	  if (n == 0)
	    {
	      /* Check for the error flag IFF N == 0, so that we don't
		 exit the loop after a partial read due to e.g., EAGAIN
		 or EWOULDBLOCK.  */
	      if (ferror (stream))
		return 1;
	      goto md5_process_partial_block;
	    }

	  /* We've read at least one byte, so ignore errors.  But always
	     check for EOF, since feof may be true even though N > 0.
	     Otherwise, we could end up calling fread after EOF.  */
	  if (feof (stream))
	    goto md5_process_partial_block;
	}

      /* Process buffer with BLOCKSIZE bytes.  Note that
			BLOCKSIZE % 64 == 0
       */
      md5_process_block (global_buffer, BLOCKSIZE, &md5_global_ctx);
    }

 md5_process_partial_block:;

  /* Process any remaining bytes.  */
  /* shntool: now done outside this function */
/*
  if (sum > 0)
    md5_process_bytes (global_buffer, sum, &md5_global_ctx);
*/

  /* shntool: keep track of partial blocks */
  remaining_bytes = sum;

  /* Construct result in desired memory.  */
  /* shntool: now done outside this function */
/*
  md5_finish_ctx (&md5_global_ctx, resblock);
*/

  /* shntool: use custom return code */
/*
  return 0;
*/

  return (totalbytes == maxbytes) ? 0 : 2;
}

/* Compute MD5 message digest for LEN bytes beginning at BUFFER.  The
   result is always in little endian byte order, so that a byte-wise
   output yields to the wanted ASCII representation of the message
   digest.  */
void *
md5_buffer (const char *buffer, size_t len, void *resblock)
{
  struct md5_ctx ctx;

  /* Initialize the computation context.  */
  md5_init_ctx (&ctx);

  /* Process whole buffer but last len % 64 bytes.  */
  md5_process_bytes (buffer, len, &ctx);

  /* Put result in desired memory area.  */
  return md5_finish_ctx (&ctx, resblock);
}


void
md5_process_bytes (const void *buffer, size_t len, struct md5_ctx *ctx)
{
  /* When we already have some bits in our internal buffer concatenate
     both inputs first.  */
  if (ctx->buflen != 0)
    {
      size_t left_over = ctx->buflen;
      size_t add = 128 - left_over > len ? len : 128 - left_over;

      memcpy (&ctx->buffer[left_over], buffer, add);
      ctx->buflen += add;

      if (ctx->buflen > 64)
	{
	  md5_process_block (ctx->buffer, ctx->buflen & ~63, ctx);

	  ctx->buflen &= 63;
	  /* The regions in the following copy operation cannot overlap.  */
	  memcpy (ctx->buffer, &ctx->buffer[(left_over + add) & ~63],
		  ctx->buflen);
	}

      buffer = (const char *) buffer + add;
      len -= add;
    }

  /* Process available complete blocks.  */
  if (len >= 64)
    {
#if !_STRING_ARCH_unaligned
# define alignof(type) offsetof (struct { char c; type x; }, x)
# define UNALIGNED_P(p) (((size_t) p) % alignof (md5_uint32) != 0)
      if (UNALIGNED_P (buffer))
	while (len > 64)
	  {
	    md5_process_block (memcpy (ctx->buffer, buffer, 64), 64, ctx);
	    buffer = (const char *) buffer + 64;
	    len -= 64;
	  }
      else
#endif
	{
	  md5_process_block (buffer, len & ~63, ctx);
	  buffer = (const char *) buffer + (len & ~63);
	  len &= 63;
	}
    }

  /* Move remaining bytes in internal buffer.  */
  if (len > 0)
    {
      size_t left_over = ctx->buflen;

      memcpy (&ctx->buffer[left_over], buffer, len);
      left_over += len;
      if (left_over >= 64)
	{
	  md5_process_block (ctx->buffer, 64, ctx);
	  left_over -= 64;
	  memcpy (ctx->buffer, &ctx->buffer[64], left_over);
	}
      ctx->buflen = left_over;
    }
}


/* These are the four functions used in the four steps of the MD5 algorithm
   and defined in the RFC 1321.  The first function is a little bit optimized
   (as found in Colin Plumbs public domain implementation).  */
/* #define FF(b, c, d) ((b & c) | (~b & d)) */
#define FF(b, c, d) (d ^ (b & (c ^ d)))
#define FG(b, c, d) FF (d, b, c)
#define FH(b, c, d) (b ^ c ^ d)
#define FI(b, c, d) (c ^ (b | ~d))

/* Process LEN bytes of BUFFER, accumulating context into CTX.
   It is assumed that LEN % 64 == 0.  */

void
md5_process_block (const void *buffer, size_t len, struct md5_ctx *ctx)
{
  md5_uint32 correct_words[16];
  const md5_uint32 *words = buffer;
  size_t nwords = len / sizeof (md5_uint32);
  const md5_uint32 *endp = words + nwords;
  md5_uint32 A = ctx->A;
  md5_uint32 B = ctx->B;
  md5_uint32 C = ctx->C;
  md5_uint32 D = ctx->D;

  /* First increment the byte count.  RFC 1321 specifies the possible
     length of the file up to 2^64 bits.  Here we only compute the
     number of bytes.  Do a double word increment.  */
  ctx->total[0] += len;
  if (ctx->total[0] < len)
    ++ctx->total[1];

  /* Process all bytes in the buffer with 64 bytes in each round of
     the loop.  */
  while (words < endp)
    {
      md5_uint32 *cwp = correct_words;
      md5_uint32 A_save = A;
      md5_uint32 B_save = B;
      md5_uint32 C_save = C;
      md5_uint32 D_save = D;

      /* First round: using the given function, the context and a constant
	 the next context is computed.  Because the algorithms processing
	 unit is a 32-bit word and it is determined to work on words in
	 little endian byte order we perhaps have to change the byte order
	 before the computation.  To reduce the work for the next steps
	 we store the swapped words in the array CORRECT_WORDS.  */

#define OP(a, b, c, d, s, T)						\
      do								\
        {								\
	  a += FF (b, c, d) + (*cwp++ = SWAP_MD5 (*words)) + T;		\
	  ++words;							\
	  CYCLIC (a, s);						\
	  a += b;							\
        }								\
      while (0)

      /* It is unfortunate that C does not provide an operator for
	 cyclic rotation.  Hope the C compiler is smart enough.  */
#define CYCLIC(w, s) (w = (w << s) | (w >> (32 - s)))

      /* Before we start, one word to the strange constants.
	 They are defined in RFC 1321 as

	 T[i] = (int) (4294967296.0 * fabs (sin (i))), i=1..64

	 Here is an equivalent invocation using Perl:

	 perl -e 'foreach(1..64){printf "0x%08x\n", int (4294967296 * abs (sin $_))}'
       */

      /* Round 1.  */
      OP (A, B, C, D,  7, 0xd76aa478);
      OP (D, A, B, C, 12, 0xe8c7b756);
      OP (C, D, A, B, 17, 0x242070db);
      OP (B, C, D, A, 22, 0xc1bdceee);
      OP (A, B, C, D,  7, 0xf57c0faf);
      OP (D, A, B, C, 12, 0x4787c62a);
      OP (C, D, A, B, 17, 0xa8304613);
      OP (B, C, D, A, 22, 0xfd469501);
      OP (A, B, C, D,  7, 0x698098d8);
      OP (D, A, B, C, 12, 0x8b44f7af);
      OP (C, D, A, B, 17, 0xffff5bb1);
      OP (B, C, D, A, 22, 0x895cd7be);
      OP (A, B, C, D,  7, 0x6b901122);
      OP (D, A, B, C, 12, 0xfd987193);
      OP (C, D, A, B, 17, 0xa679438e);
      OP (B, C, D, A, 22, 0x49b40821);

      /* For the second to fourth round we have the possibly swapped words
	 in CORRECT_WORDS.  Redefine the macro to take an additional first
	 argument specifying the function to use.  */
#undef OP
#define OP(f, a, b, c, d, k, s, T)					\
      do								\
	{								\
	  a += f (b, c, d) + correct_words[k] + T;			\
	  CYCLIC (a, s);						\
	  a += b;							\
	}								\
      while (0)

      /* Round 2.  */
      OP (FG, A, B, C, D,  1,  5, 0xf61e2562);
      OP (FG, D, A, B, C,  6,  9, 0xc040b340);
      OP (FG, C, D, A, B, 11, 14, 0x265e5a51);
      OP (FG, B, C, D, A,  0, 20, 0xe9b6c7aa);
      OP (FG, A, B, C, D,  5,  5, 0xd62f105d);
      OP (FG, D, A, B, C, 10,  9, 0x02441453);
      OP (FG, C, D, A, B, 15, 14, 0xd8a1e681);
      OP (FG, B, C, D, A,  4, 20, 0xe7d3fbc8);
      OP (FG, A, B, C, D,  9,  5, 0x21e1cde6);
      OP (FG, D, A, B, C, 14,  9, 0xc33707d6);
      OP (FG, C, D, A, B,  3, 14, 0xf4d50d87);
      OP (FG, B, C, D, A,  8, 20, 0x455a14ed);
      OP (FG, A, B, C, D, 13,  5, 0xa9e3e905);
      OP (FG, D, A, B, C,  2,  9, 0xfcefa3f8);
      OP (FG, C, D, A, B,  7, 14, 0x676f02d9);
      OP (FG, B, C, D, A, 12, 20, 0x8d2a4c8a);

      /* Round 3.  */
      OP (FH, A, B, C, D,  5,  4, 0xfffa3942);
      OP (FH, D, A, B, C,  8, 11, 0x8771f681);
      OP (FH, C, D, A, B, 11, 16, 0x6d9d6122);
      OP (FH, B, C, D, A, 14, 23, 0xfde5380c);
      OP (FH, A, B, C, D,  1,  4, 0xa4beea44);
      OP (FH, D, A, B, C,  4, 11, 0x4bdecfa9);
      OP (FH, C, D, A, B,  7, 16, 0xf6bb4b60);
      OP (FH, B, C, D, A, 10, 23, 0xbebfbc70);
      OP (FH, A, B, C, D, 13,  4, 0x289b7ec6);
      OP (FH, D, A, B, C,  0, 11, 0xeaa127fa);
      OP (FH, C, D, A, B,  3, 16, 0xd4ef3085);
      OP (FH, B, C, D, A,  6, 23, 0x04881d05);
      OP (FH, A, B, C, D,  9,  4, 0xd9d4d039);
      OP (FH, D, A, B, C, 12, 11, 0xe6db99e5);
      OP (FH, C, D, A, B, 15, 16, 0x1fa27cf8);
      OP (FH, B, C, D, A,  2, 23, 0xc4ac5665);

      /* Round 4.  */
      OP (FI, A, B, C, D,  0,  6, 0xf4292244);
      OP (FI, D, A, B, C,  7, 10, 0x432aff97);
      OP (FI, C, D, A, B, 14, 15, 0xab9423a7);
      OP (FI, B, C, D, A,  5, 21, 0xfc93a039);
      OP (FI, A, B, C, D, 12,  6, 0x655b59c3);
      OP (FI, D, A, B, C,  3, 10, 0x8f0ccc92);
      OP (FI, C, D, A, B, 10, 15, 0xffeff47d);
      OP (FI, B, C, D, A,  1, 21, 0x85845dd1);
      OP (FI, A, B, C, D,  8,  6, 0x6fa87e4f);
      OP (FI, D, A, B, C, 15, 10, 0xfe2ce6e0);
      OP (FI, C, D, A, B,  6, 15, 0xa3014314);
      OP (FI, B, C, D, A, 13, 21, 0x4e0811a1);
      OP (FI, A, B, C, D,  4,  6, 0xf7537e82);
      OP (FI, D, A, B, C, 11, 10, 0xbd3af235);
      OP (FI, C, D, A, B,  2, 15, 0x2ad7d2bb);
      OP (FI, B, C, D, A,  9, 21, 0xeb86d391);

      /* Add the starting values of the context.  */
      A += A_save;
      B += B_save;
      C += C_save;
      D += D_save;
    }

  /* Put checksum in context given as argument.  */
  ctx->A = A;
  ctx->B = B;
  ctx->C = C;
  ctx->D = D;
}

/* sha1.c - Functions to compute SHA1 message digest of files or
   memory blocks according to the NIST specification FIPS-180-1.

   Copyright (C) 2000, 2001, 2003, 2004, 2005 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 2, or (at your option) any
   later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

/* Written by Scott G. Miller
   Credits:
      Robert Klep <robert@ilse.nl>  -- Expansion function fix
*/

#include "sha1.h"

/* shntool: global sha1 context */
struct sha1_ctx sha1_global_ctx;

/*
  Takes a pointer to a 160 bit block of data (five 32 bit ints) and
  intializes it to the start constants of the SHA1 algorithm.  This
  must be called before using hash in the call to sha1_hash.
*/
void
sha1_init_ctx (struct sha1_ctx *ctx)
{
  int i;

  ctx->A = 0x67452301;
  ctx->B = 0xefcdab89;
  ctx->C = 0x98badcfe;
  ctx->D = 0x10325476;
  ctx->E = 0xc3d2e1f0;

  ctx->total[0] = ctx->total[1] = 0;
  ctx->buflen = 0;

  for (i=0;i<128;i++)
    ctx->buffer[i] = '\0';
}

/* Put result from CTX in first 20 bytes following RESBUF.  The result
   must be in little endian byte order.

   IMPORTANT: On some systems it is required that RESBUF is correctly
   aligned for a 32 bits value.  */
void *
sha1_read_ctx (const struct sha1_ctx *ctx, void *resbuf)
{
  ((md5_uint32 *) resbuf)[0] = SWAP_SHA1 (ctx->A);
  ((md5_uint32 *) resbuf)[1] = SWAP_SHA1 (ctx->B);
  ((md5_uint32 *) resbuf)[2] = SWAP_SHA1 (ctx->C);
  ((md5_uint32 *) resbuf)[3] = SWAP_SHA1 (ctx->D);
  ((md5_uint32 *) resbuf)[4] = SWAP_SHA1 (ctx->E);

  return resbuf;
}

/* Process the remaining bytes in the internal buffer and the usual
   prolog according to the standard and write the result to RESBUF.

   IMPORTANT: On some systems it is required that RESBUF is correctly
   aligned for a 32 bits value.  */
void *
sha1_finish_ctx (struct sha1_ctx *ctx, void *resbuf)
{
  /* Take yet unprocessed bytes into account.  */
  md5_uint32 bytes = ctx->buflen;
  size_t pad;

  /* Now count remaining bytes.  */
  ctx->total[0] += bytes;
  if (ctx->total[0] < bytes)
    ++ctx->total[1];

  pad = bytes >= 56 ? 64 + 56 - bytes : 56 - bytes;
  memcpy (&ctx->buffer[bytes], fillbuf, pad);

  /* Put the 64-bit file length in *bits* at the end of the buffer.  */
  *(md5_uint32 *) &ctx->buffer[bytes + pad + 4] = SWAP_SHA1 (ctx->total[0] << 3);
  *(md5_uint32 *) &ctx->buffer[bytes + pad] = SWAP_SHA1 ((ctx->total[1] << 3) |
						    (ctx->total[0] >> 29));

  /* Process last bytes.  */
  sha1_process_block (ctx->buffer, bytes + pad + 8, ctx);

  return sha1_read_ctx (ctx, resbuf);
}

/* Compute SHA1 message digest for bytes read from STREAM.  The
   resulting message digest number will be written into the 16 bytes
   beginning at RESBLOCK.  */
int
sha1_stream (FILE *stream, void *resblock)
{
  size_t sum;
  unsigned long totalbytes = 0;  /* shntool */

  /* shntool: */
  if (0 == maxbytes)
    return 0;

  /* Initialize the computation context.  */
  /* shntool: now done outside of this function */
/*
  sha1_init_ctx (&sha1_global_ctx);
*/

  /* Iterate over full file contents.  */
  /* shntool: stop after WAVE data is read */
  while (1)
    {
      /* We read the file in blocks of BLOCKSIZE bytes.  One call of the
	 computation function processes the whole buffer so that with the
	 next round of the loop another block can be read.  */
      size_t n;
      sum = 0;

      /* Read block.  Take care for partial reads.  */
      while (1)
	{
	  n = read_n_bytes(stream, (unsigned char *)(global_buffer + sum), BLOCKSIZE - sum, &proginfo);

	  sum += n;
      totalbytes += n;  /* shntool */

      /* shntool: if total bytes read exceeded the WAVE data size, discard the rest and adjust sum */
      if (totalbytes > maxbytes) {
        sum -= (totalbytes - maxbytes);
        totalbytes = maxbytes;
      }

	  if (sum == BLOCKSIZE)
	    break;

	  if (n == 0)
	    {
	      /* Check for the error flag IFF N == 0, so that we don't
		 exit the loop after a partial read due to e.g., EAGAIN
		 or EWOULDBLOCK.  */
	      if (ferror (stream))
		return 1;
	      goto sha1_process_partial_block;
	    }

	  /* We've read at least one byte, so ignore errors.  But always
	     check for EOF, since feof may be true even though N > 0.
	     Otherwise, we could end up calling fread after EOF.  */
	  if (feof (stream))
	    goto sha1_process_partial_block;
	}

      /* Process buffer with BLOCKSIZE bytes.  Note that
			BLOCKSIZE % 64 == 0
       */
      sha1_process_block (global_buffer, BLOCKSIZE, &sha1_global_ctx);
    }

 sha1_process_partial_block:;

  /* Process any remaining bytes.  */
  /* shntool: now done outside this function */
/*
  if (sum > 0)
    sha1_process_bytes (global_buffer, sum, &sha1_global_ctx);
*/

  /* shntool: keep track of partial blocks */
  remaining_bytes = sum;

  /* Construct result in desired memory.  */
  /* shntool: now done outside this function */
/*
  sha1_finish_ctx (&sha1_global_ctx, resblock);
*/

  /* shntool: use custom return code */
/*
  return 0;
*/

  return (totalbytes == maxbytes) ? 0 : 2;
}

/* Compute MD5 message digest for LEN bytes beginning at BUFFER.  The
   result is always in little endian byte order, so that a byte-wise
   output yields to the wanted ASCII representation of the message
   digest.  */
void *
sha1_buffer (const char *buffer, size_t len, void *resblock)
{
  struct sha1_ctx ctx;

  /* Initialize the computation context.  */
  sha1_init_ctx (&ctx);

  /* Process whole buffer but last len % 64 bytes.  */
  sha1_process_bytes (buffer, len, &ctx);

  /* Put result in desired memory area.  */
  return sha1_finish_ctx (&ctx, resblock);
}

void
sha1_process_bytes (const void *buffer, size_t len, struct sha1_ctx *ctx)
{
  /* When we already have some bits in our internal buffer concatenate
     both inputs first.  */
  if (ctx->buflen != 0)
    {
      size_t left_over = ctx->buflen;
      size_t add = 128 - left_over > len ? len : 128 - left_over;

      memcpy (&ctx->buffer[left_over], buffer, add);
      ctx->buflen += add;

      if (ctx->buflen > 64)
	{
	  sha1_process_block (ctx->buffer, ctx->buflen & ~63, ctx);

	  ctx->buflen &= 63;
	  /* The regions in the following copy operation cannot overlap.  */
	  memcpy (ctx->buffer, &ctx->buffer[(left_over + add) & ~63],
		  ctx->buflen);
	}

      buffer = (const char *) buffer + add;
      len -= add;
    }

  /* Process available complete blocks.  */
  if (len >= 64)
    {
#if !_STRING_ARCH_unaligned
# define alignof(type) offsetof (struct { char c; type x; }, x)
# define UNALIGNED_P(p) (((size_t) p) % alignof (md5_uint32) != 0)
      if (UNALIGNED_P (buffer))
	while (len > 64)
	  {
	    sha1_process_block (memcpy (ctx->buffer, buffer, 64), 64, ctx);
	    buffer = (const char *) buffer + 64;
	    len -= 64;
	  }
      else
#endif
	{
	  sha1_process_block (buffer, len & ~63, ctx);
	  buffer = (const char *) buffer + (len & ~63);
	  len &= 63;
	}
    }

  /* Move remaining bytes in internal buffer.  */
  if (len > 0)
    {
      size_t left_over = ctx->buflen;

      memcpy (&ctx->buffer[left_over], buffer, len);
      left_over += len;
      if (left_over >= 64)
	{
	  sha1_process_block (ctx->buffer, 64, ctx);
	  left_over -= 64;
	  memcpy (ctx->buffer, &ctx->buffer[64], left_over);
	}
      ctx->buflen = left_over;
    }
}

/* --- Code below is the primary difference between md5.c and sha1.c --- */

/* SHA1 round constants */
#define K1 0x5a827999L
#define K2 0x6ed9eba1L
#define K3 0x8f1bbcdcL
#define K4 0xca62c1d6L

/* Round functions.  Note that F2 is the same as F4.  */
#define F1(B,C,D) ( D ^ ( B & ( C ^ D ) ) )
#define F2(B,C,D) (B ^ C ^ D)
#define F3(B,C,D) ( ( B & C ) | ( D & ( B | C ) ) )
#define F4(B,C,D) (B ^ C ^ D)

/* Process LEN bytes of BUFFER, accumulating context into CTX.
   It is assumed that LEN % 64 == 0.
   Most of this code comes from GnuPG's cipher/sha1.c.  */

void
sha1_process_block (const void *buffer, size_t len, struct sha1_ctx *ctx)
{
  const md5_uint32 *words = buffer;
  size_t nwords = len / sizeof (md5_uint32);
  const md5_uint32 *endp = words + nwords;
  md5_uint32 x[16];
  md5_uint32 a = ctx->A;
  md5_uint32 b = ctx->B;
  md5_uint32 c = ctx->C;
  md5_uint32 d = ctx->D;
  md5_uint32 e = ctx->E;

  /* First increment the byte count.  RFC 1321 specifies the possible
     length of the file up to 2^64 bits.  Here we only compute the
     number of bytes.  Do a double word increment.  */
  ctx->total[0] += len;
  if (ctx->total[0] < len)
    ++ctx->total[1];

#define rol(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

#define M(I) ( tm =   x[I&0x0f] ^ x[(I-14)&0x0f] \
		    ^ x[(I-8)&0x0f] ^ x[(I-3)&0x0f] \
	       , (x[I&0x0f] = rol(tm, 1)) )

#define R(A,B,C,D,E,F,K,M)  do { E += rol( A, 5 )     \
				      + F( B, C, D )  \
				      + K	      \
				      + M;	      \
				 B = rol( B, 30 );    \
			       } while(0)

  while (words < endp)
    {
      md5_uint32 tm;
      int t;
      for (t = 0; t < 16; t++)
	{
	  x[t] = SWAP_SHA1 (*words);
	  words++;
	}

      R( a, b, c, d, e, F1, K1, x[ 0] );
      R( e, a, b, c, d, F1, K1, x[ 1] );
      R( d, e, a, b, c, F1, K1, x[ 2] );
      R( c, d, e, a, b, F1, K1, x[ 3] );
      R( b, c, d, e, a, F1, K1, x[ 4] );
      R( a, b, c, d, e, F1, K1, x[ 5] );
      R( e, a, b, c, d, F1, K1, x[ 6] );
      R( d, e, a, b, c, F1, K1, x[ 7] );
      R( c, d, e, a, b, F1, K1, x[ 8] );
      R( b, c, d, e, a, F1, K1, x[ 9] );
      R( a, b, c, d, e, F1, K1, x[10] );
      R( e, a, b, c, d, F1, K1, x[11] );
      R( d, e, a, b, c, F1, K1, x[12] );
      R( c, d, e, a, b, F1, K1, x[13] );
      R( b, c, d, e, a, F1, K1, x[14] );
      R( a, b, c, d, e, F1, K1, x[15] );
      R( e, a, b, c, d, F1, K1, M(16) );
      R( d, e, a, b, c, F1, K1, M(17) );
      R( c, d, e, a, b, F1, K1, M(18) );
      R( b, c, d, e, a, F1, K1, M(19) );
      R( a, b, c, d, e, F2, K2, M(20) );
      R( e, a, b, c, d, F2, K2, M(21) );
      R( d, e, a, b, c, F2, K2, M(22) );
      R( c, d, e, a, b, F2, K2, M(23) );
      R( b, c, d, e, a, F2, K2, M(24) );
      R( a, b, c, d, e, F2, K2, M(25) );
      R( e, a, b, c, d, F2, K2, M(26) );
      R( d, e, a, b, c, F2, K2, M(27) );
      R( c, d, e, a, b, F2, K2, M(28) );
      R( b, c, d, e, a, F2, K2, M(29) );
      R( a, b, c, d, e, F2, K2, M(30) );
      R( e, a, b, c, d, F2, K2, M(31) );
      R( d, e, a, b, c, F2, K2, M(32) );
      R( c, d, e, a, b, F2, K2, M(33) );
      R( b, c, d, e, a, F2, K2, M(34) );
      R( a, b, c, d, e, F2, K2, M(35) );
      R( e, a, b, c, d, F2, K2, M(36) );
      R( d, e, a, b, c, F2, K2, M(37) );
      R( c, d, e, a, b, F2, K2, M(38) );
      R( b, c, d, e, a, F2, K2, M(39) );
      R( a, b, c, d, e, F3, K3, M(40) );
      R( e, a, b, c, d, F3, K3, M(41) );
      R( d, e, a, b, c, F3, K3, M(42) );
      R( c, d, e, a, b, F3, K3, M(43) );
      R( b, c, d, e, a, F3, K3, M(44) );
      R( a, b, c, d, e, F3, K3, M(45) );
      R( e, a, b, c, d, F3, K3, M(46) );
      R( d, e, a, b, c, F3, K3, M(47) );
      R( c, d, e, a, b, F3, K3, M(48) );
      R( b, c, d, e, a, F3, K3, M(49) );
      R( a, b, c, d, e, F3, K3, M(50) );
      R( e, a, b, c, d, F3, K3, M(51) );
      R( d, e, a, b, c, F3, K3, M(52) );
      R( c, d, e, a, b, F3, K3, M(53) );
      R( b, c, d, e, a, F3, K3, M(54) );
      R( a, b, c, d, e, F3, K3, M(55) );
      R( e, a, b, c, d, F3, K3, M(56) );
      R( d, e, a, b, c, F3, K3, M(57) );
      R( c, d, e, a, b, F3, K3, M(58) );
      R( b, c, d, e, a, F3, K3, M(59) );
      R( a, b, c, d, e, F4, K4, M(60) );
      R( e, a, b, c, d, F4, K4, M(61) );
      R( d, e, a, b, c, F4, K4, M(62) );
      R( c, d, e, a, b, F4, K4, M(63) );
      R( b, c, d, e, a, F4, K4, M(64) );
      R( a, b, c, d, e, F4, K4, M(65) );
      R( e, a, b, c, d, F4, K4, M(66) );
      R( d, e, a, b, c, F4, K4, M(67) );
      R( c, d, e, a, b, F4, K4, M(68) );
      R( b, c, d, e, a, F4, K4, M(69) );
      R( a, b, c, d, e, F4, K4, M(70) );
      R( e, a, b, c, d, F4, K4, M(71) );
      R( d, e, a, b, c, F4, K4, M(72) );
      R( c, d, e, a, b, F4, K4, M(73) );
      R( b, c, d, e, a, F4, K4, M(74) );
      R( a, b, c, d, e, F4, K4, M(75) );
      R( e, a, b, c, d, F4, K4, M(76) );
      R( d, e, a, b, c, F4, K4, M(77) );
      R( c, d, e, a, b, F4, K4, M(78) );
      R( b, c, d, e, a, F4, K4, M(79) );

      a = ctx->A += a;
      b = ctx->B += b;
      c = ctx->C += c;
      d = ctx->D += d;
      e = ctx->E += e;
    }
}

/* shntool-specific stuff starts here */

static void hash_help()
{
  st_info("Usage: %s [OPTIONS] [files]\n",st_progname());
  st_info("\n");
  st_info("Mode-specific options:\n");
  st_info("\n");
  st_info("  -c      generate composite fingerprint from input files\n");
  st_info("  -h      show this help screen\n");
  st_info("  -m      generate MD5 fingerprints (default)\n");
  st_info("  -s      generate SHA1 fingerprints\n");
  st_info("\n");
}

static void parse(int argc,char **argv,int *first_arg)
{
  int c;

  while ((c = st_getopt(argc,argv,"cms")) != -1) {
    switch (c) {
      case 'c':
        composite_hash = TRUE;
        break;
      case 'm':
        hash_algorithm = HASH_MD5;
        break;
      case 's':
        hash_algorithm = HASH_SHA1;
        break;
    }
  }

  *first_arg = optind;
}

void hash_init_ctx()
{
  switch (hash_algorithm) {
    case HASH_MD5:
      md5_init_ctx(&md5_global_ctx);
      break;
    case HASH_SHA1:
      sha1_init_ctx(&sha1_global_ctx);
      break;
  }
}

int hash_stream(FILE *stream)
{
  switch (hash_algorithm) {
    case HASH_MD5:
      return md5_stream(stream,audio_hash);
      break;
    case HASH_SHA1:
      return sha1_stream(stream,audio_hash);
      break;
  }

  return 3;
}

void hash_process_bytes()
{
  switch (hash_algorithm) {
    case HASH_MD5:
      md5_process_bytes(global_buffer,remaining_bytes,&md5_global_ctx);
      break;
    case HASH_SHA1:
      sha1_process_bytes(global_buffer,remaining_bytes,&sha1_global_ctx);
      break;
  }
}

void hash_finish_ctx()
{
  switch (hash_algorithm) {
    case HASH_MD5:
      md5_finish_ctx(&md5_global_ctx,audio_hash);
      break;
    case HASH_SHA1:
      sha1_finish_ctx(&sha1_global_ctx,audio_hash);
      break;
  }
}

void hash_process_block()
{
  switch (hash_algorithm) {
    case HASH_MD5:
      md5_process_block(global_buffer,BLOCKSIZE,&md5_global_ctx);
      break;
    case HASH_SHA1:
      sha1_process_block(global_buffer,BLOCKSIZE,&sha1_global_ctx);
      break;
  }
}

static void print_audio_hash(char *filename)
{
  int i,hash_hex_bytes = 0;

  switch (hash_algorithm) {
    case HASH_MD5:
      hash_hex_bytes = 32;
      break;
    case HASH_SHA1:
      hash_hex_bytes = 40;
      break;
  }

  for (i=0;i<(hash_hex_bytes/2);i++)
    st_output("%02x",audio_hash[i]);

  st_output("  [shntool]  %s\n",filename);
}

static bool generate_audio_hash_single(wave_info *info)
{
  unsigned char *header;
  int retval;
  bool success;

  success = FALSE;

  proginfo.initialized = FALSE;
  proginfo.filename2 = info->filename;
  proginfo.filedesc2 = info->m_ss;
  proginfo.bytes_total = info->data_size;

  prog_update(&proginfo);

  if (!open_input_stream(info)) {
    st_warning("could not reopen input file: [%s]",info->filename);
    return FALSE;
  }

  if (NULL == (header = malloc(info->header_size * sizeof(unsigned char)))) {
    prog_error(&proginfo);
    st_warning("could not allocate %d-byte WAVE header",info->header_size);
    goto cleanup_single1;
  }

  if (read_n_bytes(info->input,header,info->header_size,NULL) != info->header_size) {
    prog_error(&proginfo);
    st_warning("error while discarding %d-byte WAVE header from file: [%s]",info->header_size,info->filename);
    goto cleanup_single2;
  }

  maxbytes = info->data_size;
  remaining_bytes = 0;

  /* Initialize the computation context.  */
  hash_init_ctx();

  retval = hash_stream(info->input);

  /* Add the last bytes if necessary.  */
  if (remaining_bytes > 0)
    hash_process_bytes();

  /* Construct result in desired memory.  */
  hash_finish_ctx();

  if (retval) {
    prog_error(&proginfo);
    st_warning("possibly truncated and/or corrupt file: [%s]",info->filename);
    goto cleanup_single2;
  }

  success = TRUE;

  prog_success(&proginfo);

  print_audio_hash(info->filename);

cleanup_single2:
  st_free(header);

cleanup_single1:
  close_input_stream(info);

  return success;
}

static bool generate_audio_hash_composite(wave_info *info)
{
  unsigned char *header;
  int retval,bytes_to_read;
  bool success;

  success = FALSE;

  if (!open_input_stream(info))
    st_error("could not reopen input file: [%s]",info->filename);

  if (NULL == (header = malloc(info->header_size * sizeof(unsigned char))))
    st_error("could not allocate %d-byte WAVE header",info->header_size);

  if (read_n_bytes(info->input,header,info->header_size,NULL) != info->header_size)
    st_error("error while discarding %d-byte WAVE header from file: [%s]",info->header_size,info->filename);

  bytes_to_read = 0;

  /* in order to generate the correct MD5/SHA1 signature, all data (modulus the
   * final bytes) must be processed in blocks.  if the previous file left
   * extra bytes that didn't fill out a full block, then try to read enough
   * from this file to fill it out and process it.
   */
  if (remaining_bytes > 0) {
    bytes_to_read = min(info->data_size,BLOCKSIZE - remaining_bytes);

    if (read_n_bytes(info->input,(unsigned char *)(global_buffer + remaining_bytes),bytes_to_read,NULL) != bytes_to_read)
      st_error("error while filling out block with data from file: [%s]",info->filename);

    remaining_bytes += bytes_to_read;

    if (BLOCKSIZE == remaining_bytes) {
      hash_process_block();
      remaining_bytes = 0;
    }
  }

  maxbytes = info->data_size - bytes_to_read;

  retval = hash_stream(info->input);

  if (retval)
    st_error("possibly truncated and/or corrupt file: [%s]",info->filename);

  success = TRUE;

  st_free(header);

  close_input_stream(info);

  return success;
}

static bool process_file(char *filename)
{
  wave_info *info;
  bool success;

  if (NULL == (info = new_wave_info(filename))) {
    if (composite_hash)
      st_error("all files must be valid to generate a composite fingerprint");
    return FALSE;
  }

  if (composite_hash)
    success = generate_audio_hash_composite(info);
  else
    success = generate_audio_hash_single(info);

  num_processed++;

  st_free(info);

  return success;
}

static void composite_init(wlong total)
{
  if (!composite_hash)
    return;

  /* Initialize the computation context.  */
  hash_init_ctx();

  remaining_bytes = 0;

  proginfo.initialized = FALSE;
  proginfo.filename2 = COMPOSITE;
  proginfo.filedesc2 = NULL;
  proginfo.bytes_total = total;

  prog_update(&proginfo);
}

static void composite_finish()
{
  if (!composite_hash)
    return;

  if (0 == num_processed)
    st_error("need at least one valid file to process");

  /* Add the last bytes if necessary.  */
  if (remaining_bytes > 0)
    hash_process_bytes();

  /* Construct result in desired memory.  */
  hash_finish_ctx();

  prog_success(&proginfo);

  print_audio_hash(COMPOSITE);
}

static bool process(int argc,char **argv,int start)
{
  int i,j = 0,badfiles = 0;
  char *filename;
  wlong total = 0;
  bool success;

  success = TRUE;

  input_init(start,argc,argv);
  input_read_all_files();
  numfiles = input_get_file_count();

  if (NULL == (files = malloc((numfiles + 1) * sizeof(wave_info *))))
    st_error("could not allocate memory for file info array");

  for (i=0;i<numfiles;i++) {
    filename = input_get_filename();
    if (NULL == (files[j] = new_wave_info(filename))) {
      if (composite_hash)
        st_error("all files must be valid to generate a composite fingerprint");
      success = FALSE;
      badfiles++;
    }
    else {
      total += files[j]->data_size;
      j++;
    }
  }

  numfiles -= badfiles;

  files[numfiles] = NULL;

  reorder_files(files,numfiles);

  proginfo.prefix = "Hashing";
  proginfo.clause = NULL;
  proginfo.filename1 = NULL;
  proginfo.filedesc1 = NULL;

  composite_init(total);

  input_init(start,argc,argv);

  while ((filename = input_get_filename())) {
    success = (process_file(filename) && success);
  }

  composite_finish();

  for (i=0;i<numfiles;i++)
    st_free(files[i]);

  st_free(files);

  return success;
}

static bool hash_main(int argc,char **argv)
{
  int first_arg;

  parse(argc,argv,&first_arg);

  return process(argc,argv,first_arg);
}
