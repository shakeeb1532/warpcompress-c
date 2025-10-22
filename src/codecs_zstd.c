// src/codecs_zstd.c
#include "codecs.h"

#ifdef HAVE_ZSTD
  #include <zstd.h>
#endif

#include <string.h> // for memcpy when stubbing (if you want)

size_t zstd_compress(void *dst, size_t dst_cap,
                     const void *src, size_t src_sz,
                     int level)
{
#ifdef HAVE_ZSTD
  size_t r = ZSTD_compress(dst, dst_cap, src, src_sz, level > 0 ? level : 1);
  return ZSTD_isError(r) ? 0 : r;
#else
  (void)level; (void)dst; (void)dst_cap; (void)src; (void)src_sz;
  return 0; // no zstd available -> report failure
#endif
}

size_t zstd_decompress(void *dst, size_t dst_cap,
                       const void *src, size_t src_sz)
{
#ifdef HAVE_ZSTD
  size_t r = ZSTD_decompress(dst, dst_cap, src, src_sz);
  return ZSTD_isError(r) ? 0 : r;
#else
  (void)dst; (void)dst_cap; (void)src; (void)src_sz;
  return 0; // no zstd available -> report failure
#endif
}

size_t zstd_max_compressed_size(size_t src_sz)
{
#ifdef HAVE_ZSTD
  return ZSTD_compressBound(src_sz);
#else
  // Provide a conservative bound so callers can still size buffers if needed.
  // (Not used when zstd is actually disabled, since compress() returns 0.)
  return src_sz + (src_sz >> 3) + 64;
#endif
}

