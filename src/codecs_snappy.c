#include "codecs.h"

#ifdef HAVE_SNAPPY
#include <snappy-c.h>
#include <string.h>

/* Wrapper that matches our generic codec signature.
   Returns number of bytes written, or 0 on error. */
size_t wc_snappy_compress(void *dst, size_t dst_cap, const void *src, size_t src_sz) {
  size_t out_len = dst_cap;
  snappy_status st = snappy_compress(
      (const char *)src,              /* input */
      (size_t)src_sz,                 /* input_length */
      (char *)dst,                    /* compressed buffer */
      &out_len);                      /* in/out: compressed length */
  return (st == SNAPPY_OK) ? out_len : 0;
}

size_t wc_snappy_decompress(void *dst, size_t dst_cap, const void *src, size_t src_sz) {
  size_t out_len = dst_cap;
  snappy_status st = snappy_uncompress(
      (const char *)src,              /* compressed */
      (size_t)src_sz,                 /* compressed_length */
      (char *)dst,                    /* uncompressed buffer */
      &out_len);                      /* in/out: uncompressed length */
  return (st == SNAPPY_OK) ? out_len : 0;
}
#endif /* HAVE_SNAPPY */

