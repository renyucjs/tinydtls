/* dtls -- a very basic DTLS implementation
 *
 * Copyright (C) 2011--2012 Olaf Bergmann <bergmann@tzi.org>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "debug.h"
#include "hmac.h"

/** Aaron D. Gifford's implementation of SHA256
 *  see http://www.aarongifford.com/ */
#ifdef WITH_SHA256
#  include "sha2/sha2.h"

#define DTLS_HASH_CTX_SIZE sizeof(SHA256_CTX)

static inline void
dtls_hash_init(void *ctx) {
  SHA256_Init((SHA256_CTX *)ctx);
}

static inline void 
dtls_hash_update(void *ctx, const unsigned char *input, size_t len) {
  SHA256_Update((SHA256_CTX *)ctx, input, len);
}

static inline size_t
dtls_hash_finalize(unsigned char *buf, void *ctx) {
  SHA256_Final(buf, (SHA256_CTX *)ctx);
  return SHA256_DIGEST_LENGTH;
}
#endif /* WITH_SHA256 */

/* use malloc()/free() on platforms other than Contiki */
#ifndef WITH_CONTIKI
#define dtls_hmac_storage_init

static inline dtls_hmac_context_t *
dtls_hmac_context_new() {
  return (dtls_hmac_context_t *)malloc(DTLS_HMAC_BLOCKSIZE + DTLS_HASH_CTX_SIZE);
}

static inline void
dtls_hmac_context_free(dtls_hmac_context_t *ctx) {
  free(ctx);
}

#else /* WITH_CONTIKI */
#include "memb.h"

typedef unsigned char _hmac_context_buffer_t[DTLS_HMAC_BLOCKSIZE + DTLS_HASH_CTX_SIZE];
MEMB(hmac_context_storage, _hmac_context_buffer_t, DTLS_HASH_MAX);

void
dtls_hmac_storage_init() {
  memb_init(&hmac_context_storage);
}

static inline dtls_hmac_context_t *
dtls_hmac_context_new() {
  return (dtls_hmac_context_t *)memb_alloc(&hmac_context_storage);
}

static inline void
dtls_hmac_context_free(dtls_hmac_context_t *ctx) {
  memb_free(&hmac_context_storage, ctx);
}
#endif /* WITH_CONTIKI */


void
dtls_hmac_update(dtls_hmac_context_t *ctx,
		 const unsigned char *input, size_t ilen) {
  assert(ctx);
  dtls_hash_update(ctx->data, input, ilen);
}

void
dtls_hmac_init(dtls_hmac_context_t *ctx, unsigned char *key, size_t klen) {
  int i;

  assert(ctx);

  memset(ctx, 0, DTLS_HMAC_BLOCKSIZE + DTLS_HASH_CTX_SIZE);

  if (klen > DTLS_HMAC_BLOCKSIZE) {
    dtls_hash_init(ctx->data);
    dtls_hash_update(ctx->data, key, klen);
    dtls_hash_finalize(ctx->pad, ctx->data);
  } else
    memcpy(ctx->pad, key, klen);

  /* create ipad: */
  for (i=0; i < DTLS_HMAC_BLOCKSIZE; ++i)
    ctx->pad[i] ^= 0x36;

  dtls_hash_init(ctx->data);
  dtls_hmac_update(ctx, ctx->pad, DTLS_HMAC_BLOCKSIZE);

  /* create opad by xor-ing pad[i] with 0x36 ^ 0x5C: */
  for (i=0; i < DTLS_HMAC_BLOCKSIZE; ++i)
    ctx->pad[i] ^= 0x6A;
}

void
dtls_hmac_free(dtls_hmac_context_t *ctx) {
  ctx = dtls_hmac_context_free(ctx);
}

int
dtls_hmac_finalize(dtls_hmac_context_t *ctx, unsigned char *result) {
  unsigned char buf[DTLS_HMAC_DIGEST_SIZE];
  size_t len; 

  assert(ctx);
  assert(result);
  
  len = dtls_hash_finalize(buf, ctx->data);

  dtls_hash_init(ctx->data);
  dtls_hash_update(ctx->data, ctx->pad, DTLS_HMAC_BLOCKSIZE);
  dtls_hash_update(ctx->data, buf, len);

  len = dtls_hash_finalize(result, ctx->data);

  return len;
}

#ifdef HMAC_TEST
#include <stdio.h>

int main(int argc, char **argv) {
  static unsigned char buf[DTLS_HMAC_DIGEST_SIZE];
  size_t len, i;
  dtls_hmac_context_t *ctx;

  if (argc < 3) {
    fprintf(stderr, "usage: %s key text", argv[0]);
    return -1;
  }

  dtls_hmac_storage_init();
  ctx = dtls_hmac_new(argv[1], strlen(argv[1]));
  assert(ctx);
  dtls_hmac_update(ctx, argv[2], strlen(argv[2]));
  
  len = dtls_hmac_finalize(ctx, buf);

  for(i = 0; i < len; i++) 
    printf("%02x", buf[i]);
  printf("\n");

  dtls_hmac_free(ctx);

  return 0;
}
#endif
