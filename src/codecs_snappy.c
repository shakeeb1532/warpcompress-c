#include "codecs.h"
#ifdef HAVE_SNAPPY
#include <snappy-c.h>
#include <string.h>

size_t wc_snappy_compress(void *dst, size_t dst_cap, const void *src, size_t src_sz) {
  size_t out_len = dst_cap;
  snappy_status st = snappy_compress((const char*)src, (size_t)src_sz, (char*)dst, &out_len);
  return (st == SNAPPY_OK) ? out_len : 0;
}

size_t wc_snappy_decompress(void *dst, size_t dst_cap, const void *src, size_t src_sz) {
  size_t out_len = dst_cap;
  snappy_status st = snappy_uncompress((const char*)src, (size_t)src_sz, (char*)dst, &out_len);
  return (st == SNAPPY_OK) ? out_len : 0;
}

size_t snappy_max_compressed_size(size_t src_sz) {
  return snappy_max_compressed_length((size_t)src_sz);
}
#endif


