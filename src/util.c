/* Reserved for future helpers (prefetch, posix_fadvise, large file hints). */
#include "util.h"
#include "warp.h"
#include <unistd.h>
#include <errno.h>

ssize_t wc_pread(int fd, void *buf, size_t len, uint64_t off) {
  return pread(fd, buf, len, (off_t)off);
}
ssize_t wc_pwrite(int fd, const void *buf, size_t len, uint64_t off) {
  return pwrite(fd, buf, len, (off_t)off);
}
int wc_truncate_file(FILE *f, uint64_t size) {
  if (!f) return -1;
  return ftruncate(fileno(f), (off_t)size);
}

/* Simple, fast cross-file-size chunk policy */
uint32_t warp_pick_chunk_size(size_t total) {
  if (total <= (size_t)256<<20) return 1<<20;         /* 1 MiB */
  if (total <= (size_t)1<<30)   return 2<<20;         /* 2 MiB */
  if (total <= (size_t)5<<30)   return 8<<20;         /* 8 MiB */
  if (total <= (size_t)10<<30)  return 16<<20;        /* 16 MiB */
  if (total <= (size_t)50<<30)  return 32<<20;        /* 32 MiB */
  if (total <= (size_t)100<<30) return 64<<20;        /* 64 MiB */
  if (total <= (size_t)500<<30) return 128<<20;       /* 128 MiB */
  return 256<<20;                                     /* 256 MiB cap */
}

