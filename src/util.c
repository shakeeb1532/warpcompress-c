#define _XOPEN_SOURCE 700
#include "warpc/util.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

int read_all(int fd, void* buf, size_t n) {
  size_t off = 0;
  while (off < n) {
    ssize_t r = read(fd, (char*)buf + off, n - off);
    if (r == 0) return -1; /* EOF early */
    if (r < 0) { if (errno == EINTR) continue; return -1; }
    off += (size_t)r;
  }
  return 0;
}
int write_all(int fd, const void* buf, size_t n) {
  size_t off = 0;
  while (off < n) {
    ssize_t w = write(fd, (const char*)buf + off, n - off);
    if (w < 0) { if (errno == EINTR) continue; return -1; }
    off += (size_t)w;
  }
  return 0;
}
int pread_all(int fd, void* buf, size_t n, off_t off) {
  size_t done = 0;
  while (done < n) {
    ssize_t r = pread(fd, (char*)buf + done, n - done, off + (off_t)done);
    if (r == 0) return -1;
    if (r < 0) { if (errno == EINTR) continue; return -1; }
    done += (size_t)r;
  }
  return 0;
}
int pwrite_all(int fd, const void* buf, size_t n, off_t off) {
  size_t done = 0;
  while (done < n) {
    ssize_t w = pwrite(fd, (const char*)buf + done, n - done, off + (off_t)done);
    if (w < 0) { if (errno == EINTR) continue; return -1; }
    done += (size_t)w;
  }
  return 0;
}
int file_stat_size(const char* path, uint64_t* out) {
  struct stat st;
  if (stat(path, &st) != 0) return -1;
  *out = (uint64_t)st.st_size;
  return 0;
}
int file_open_rd(const char* path)    { return open(path, O_RDONLY); }
int file_open_wr(const char* path)    { return open(path, O_WRONLY); }
int file_open_trunc(const char* path) { return open(path, O_CREAT|O_TRUNC|O_WRONLY, 0644); }

uint64_t fnv1a64_update(uint64_t h, const void* data, size_t n) {
  const unsigned char* p = (const unsigned char*)data;
  h = (h == 0) ? 0xcbf29ce484222325ULL : h;
  for (size_t i = 0; i < n; ++i) {
    h ^= (uint64_t)p[i];
    h *= 0x100000001b3ULL;
  }
  return h;
}
uint64_t fnv1a64_file(const char* path, size_t chunk) {
  int fd = file_open_rd(path);
  if (fd < 0) return 0;
  void* buf = malloc(chunk);
  if (!buf) { close(fd); return 0; }
  uint64_t h = 0;
  for (;;) {
    ssize_t r = read(fd, buf, chunk);
    if (r < 0) { if (errno == EINTR) continue; h = 0; break; }
    if (r == 0) break;
    h = fnv1a64_update(h, buf, (size_t)r);
  }
  free(buf);
  close(fd);
  return h;
}
