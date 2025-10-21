cmake_minimum_required(VERSION 3.16)
project(warpc C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

if(APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
  # GitHub macos-14 runner is arm64; this is safe if already set
  set(CMAKE_OSX_ARCHITECTURES arm64)
endif()

# Sources
set(WARPC_SOURCES
  src/main.c
  src/container.c
  src/codecs_zstd.c
  src/codecs_lz4.c
  src/codecs_snappy.c
  src/threadpool.c
  src/util.c
  src/bufpool.c
)

add_executable(warpc ${WARPC_SOURCES})
target_include_directories(warpc PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# --- Required: Zstd ---
find_path(ZSTD_INCLUDE_DIR zstd.h)
find_library(ZSTD_LIBRARY NAMES zstd)
if(NOT ZSTD_INCLUDE_DIR OR NOT ZSTD_LIBRARY)
  message(FATAL_ERROR "zstd (libzstd) not found. Ubuntu: apt-get install libzstd-dev. macOS: brew install zstd.")
endif()
target_include_directories(warpc PRIVATE ${ZSTD_INCLUDE_DIR})
target_link_libraries(warpc PRIVATE ${ZSTD_LIBRARY})

# --- Optional: LZ4 ---
find_path(LZ4_INCLUDE_DIR lz4.h)
find_library(LZ4_LIBRARY NAMES lz4)
if(LZ4_INCLUDE_DIR AND LZ4_LIBRARY)
  target_include_directories(warpc PRIVATE ${LZ4_INCLUDE_DIR})
  target_link_libraries(warpc PRIVATE ${LZ4_LIBRARY})
  target_compile_definitions(warpc PRIVATE HAVE_LZ4=1)
endif()

# --- Optional: Snappy ---
find_path(SNAPPY_INCLUDE_DIR snappy-c.h)
find_library(SNAPPY_LIBRARY NAMES snappy)
if(SNAPPY_INCLUDE_DIR AND SNAPPY_LIBRARY)
  target_include_directories(warpc PRIVATE ${SNAPPY_INCLUDE_DIR})
  target_link_libraries(warpc PRIVATE ${SNAPPY_LIBRARY})
  target_compile_definitions(warpc PRIVATE HAVE_SNAPPY=1)
endif()

# --- Optional: xxHash (checksums) ---
find_path(XXHASH_INCLUDE_DIR xxhash.h)
find_library(XXHASH_LIBRARY NAMES xxhash)
if(XXHASH_INCLUDE_DIR AND XXHASH_LIBRARY)
  target_include_directories(warpc PRIVATE ${XXHASH_INCLUDE_DIR})
  target_link_libraries(warpc PRIVATE ${XXHASH_LIBRARY})
  target_compile_definitions(warpc PRIVATE HAVE_XXHASH=1)
endif()

# Threads
if(UNIX AND NOT APPLE)
  find_package(Threads REQUIRED)
  target_link_libraries(warpc PRIVATE Threads::Threads)
endif()

# Warnings (no -Werror for portability)
if(MSVC)
  target_compile_options(warpc PRIVATE /W3)
else()
  target_compile_options(warpc PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

