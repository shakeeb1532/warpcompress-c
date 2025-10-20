/* Reserved for future helpers (prefetch, posix_fadvise, large file hints). */
#include "util.h"
#include <unistd.h>     /* pread, pwrite, ftruncate, fileno */
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
