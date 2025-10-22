#include "codecs.h"
#ifdef HAVE_SNAPPY
#  include <snappy-c.h>
#endif

size_t snappy_max_compressed_size(size_t src_sz) {
#ifdef HAVE_SNAPPY
  return (size_t)snappy_max_compressed_length(src_sz);
#else
  (void)src_sz; return 0;
#endif
}

size_t wc_snappy_compress(void *dst, size_t dst_cap, const void *src, size_t src_sz) {
#ifdef HAVE_SNAPPY
  size_t out_len = dst_cap;
  snappy_status st = snappy_compress((const char*)src, src_sz, (char*)dst, &out_len);
  return st == SNAPPY_OK ? out_len : 0;
#else
  (void)dst; (void)dst_cap; (void)src; (void)src_sz; return 0;
#endif
}

size_t wc_snappy_decompress(void *dst, size_t dst_cap, const void *src, size_t src_sz) {
#ifdef HAVE_SNAPPY
  size_t out_len = dst_cap;
  snappy_status st = snappy_uncompress((const char*)src, src_sz, (char*)dst, &out_len);
  return st == SNAPPY_OK ? out_len : 0;
#else
  (void)dst; (void)dst_cap; (void)src; (void)src_sz; return 0;
#endif
}



