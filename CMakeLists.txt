cmake_minimum_required(VERSION 3.16)
project(warpcompress_c C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_BUILD_TYPE Release)

# Dependencies: Zstd (you can add LZ4/Snappy later)
find_path(ZSTD_INCLUDE_DIR zstd.h)
find_library(ZSTD_LIBRARY zstd)
if (NOT ZSTD_INCLUDE_DIR OR NOT ZSTD_LIBRARY)
  message(FATAL_ERROR "zstd not found. Install libzstd-dev (Linux) or 'brew install zstd' (macOS).")
endif()

include_directories(${ZSTD_INCLUDE_DIR} include)

add_executable(warpc
  src/main.c
  src/threadpool.c
  src/codecs_zstd.c
  src/container.c
  src/util.c
)

target_link_libraries(warpc ${ZSTD_LIBRARY})
if(APPLE)
  target_link_libraries(warpc "-framework CoreServices")
endif()
