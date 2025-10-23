cmake_minimum_required(VERSION 3.16)

project(warpc C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Large-file I/O (ensures off_t is 64-bit on odd 32-bit toolchains)
if (NOT MSVC)
  add_compile_definitions(_FILE_OFFSET_BITS=64)
endif()

# Sources
file(GLOB WARPC_SOURCES CONFIGURE_DEPENDS
  "src/*.c"
)

add_executable(warpc ${WARPC_SOURCES})

# -------- Zstd (optional but recommended) --------
find_path(ZSTD_INCLUDE_DIR zstd.h)
find_library(ZSTD_LIBRARY NAMES zstd)
if (ZSTD_INCLUDE_DIR AND ZSTD_LIBRARY)
  target_include_directories(warpc PRIVATE ${ZSTD_INCLUDE_DIR})
  target_link_libraries(warpc PRIVATE ${ZSTD_LIBRARY})
  target_compile_definitions(warpc PRIVATE HAVE_ZSTD=1)
else()
  message(STATUS "libzstd not found: Zstd codec will be disabled.")
endif()

# -------- LZ4 (optional but recommended) --------
find_path(LZ4_INCLUDE_DIR lz4.h)
find_library(LZ4_LIBRARY NAMES lz4)
if (LZ4_INCLUDE_DIR AND LZ4_LIBRARY)
  target_include_directories(warpc PRIVATE ${LZ4_INCLUDE_DIR})
  target_link_libraries(warpc PRIVATE ${LZ4_LIBRARY})
  target_compile_definitions(warpc PRIVATE HAVE_LZ4=1)
else()
  message(STATUS "liblz4 not found: LZ4 codec will be disabled.")
endif()

# Threads (Linux links libpthread explicitly; macOS has it in libSystem)
if(UNIX AND NOT APPLE)
  find_package(Threads REQUIRED)
  target_link_libraries(warpc PRIVATE Threads::Threads)
endif()

# Warnings
if (CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(warpc PRIVATE -Wall -Wextra -Wconversion -Wpointer-arith -Wshadow -Wformat=2)
endif()

# Install target (optional)
install(TARGETS warpc RUNTIME DESTINATION bin)

