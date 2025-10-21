/* Reserved for future helpers (prefetch, posix_fadvise, large file hints). */
#include "util.h"
#include "warp.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/* for writev + struct iovec on macOS & Linux */
#include <sys/uio.h>

/* ---------- basic p{read,write} wrappers ---------- */
ssize_t wc_pread(int fd, void *buf, size_t len, uint64_t off) {
  return pread(fd, buf, len, (off_t)off);
}

ssize_t wc_pwrite(int fd, const void *buf, size_t len, uint64_t off) {
  return pwrite(fd, buf, len, (off_t)off);
}

/* Truncate via FILE* (used to pre-size outputs) */
int wc_ftruncate_file(FILE *f, uint64_t size) {
  return ftruncate(fileno(f), (off_t)size);
}

/* Advise sequential (best effort; no-op if unsupported) */
void wc_advise_sequential(int fd) {
#ifdef __linux__
  (void)fd; /* could call posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL); */
#else
  (void)fd;
#endif
}

/* writev wrapper (returns bytes written or -1) */
ssize_t wc_writev(int fd, const wc_iovec_t *iov, int iovcnt) {
  /* Cast our lightweight wc_iovec_t to the system struct iovec */
  return writev(fd, (const struct iovec*)iov, iovcnt);
}

/* ---------- Chunk-size policy (implementation only) ----------
   Declaration is in warp.h; do NOT duplicate it there. */
uint32_t warp_pick_chunk_size(size_t bytes) {
  if (bytes <= (size_t)(  64ull<<20))  return 1u<<20;   /* 1 MiB  */
  if (bytes <= (size_t)( 256ull<<20))  return 2u<<20;   /* 2 MiB  */
  if (bytes <= (size_t)(   1ull<<30))  return 4u<<20;   /* 4 MiB  */
  if (bytes <= (size_t)(   5ull<<30))  return 8u<<20;   /* 8 MiB  */
  if (bytes <= (size_t)(  10ull<<30))  return 16u<<20;  /* 16 MiB */
  if (bytes <= (size_t)(  50ull<<30))  return 32u<<20;  /* 32 MiB */
  if (bytes <= (size_t)( 100ull<<30))  return 64u<<20;  /* 64 MiB */
  if (bytes <= (size_t)( 500ull<<30))  return 128u<<20; /* 128 MiB */
  return 256u<<20;                                      /* 256 MiB */
}

/* ---------- Default CLI options used by main.c ---------- */
warp_opts_t warp_opts_default(void) {
  warp_opts_t o;
  o.algo        = 0;                      /* auto */
  o.auto_mode   = WARP_AUTO_BALANCED;     /* 0=throughput,1=balanced,2=ratio */
  o.auto_lock   = 4;                      /* warm-up chunks before lock */
  o.level       = 1;                      /* zstd level baseline */
  o.threads     = 0;                      /* 0 = auto (use logical cores) */
  o.chunk_bytes = 0;                      /* 0 = policy */
  o.do_index    = 1;                      /* write WIX trailer */
  o.chk_kind    = WARP_CHK_NONE;          /* optional XXH64 via flag */
  o.verify      = 0;                      /* verify on decompress if set */
  o.verbose     = 0;
  return o;
}

