# warpcompress-c (MVP)

Blazing-fast C rewrite of WarpCompress using Zstd and a chunked container (.warp v3).
Multithreaded (pthreads), large I/O, adaptive chunk policy. Extensible to LZ4/Snappy.

## Build

**macOS**
```bash
brew install zstd cmake
cmake -S . -B build
cmake --build build -j
