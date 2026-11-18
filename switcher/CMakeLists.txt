cmake_minimum_required(VERSION 3.16)
project(switcher C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# raylib: use an installed copy if present, else the vendored 5.5 tree.
find_package(raylib 5.5 QUIET)
if(NOT raylib_FOUND)
  set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
  set(CUSTOMIZE_BUILD OFF CACHE BOOL "" FORCE)
  add_subdirectory(third_party/raylib EXCLUDE_FROM_ALL)
endif()

# Protocol core: no GUI dependency, unit-testable on its own.
add_library(swcore STATIC
  src/proto.c
  src/net.c
  src/devices.c
  src/session.c
  src/hds7105.c
  src/hds7106.c)
target_include_directories(swcore PUBLIC src)
target_compile_options(swcore PRIVATE -Wall -Wextra -Wpedantic)

# Widget kit on raylib.
add_library(swui STATIC src/ui.c)
target_include_directories(swui PUBLIC src)
target_link_libraries(swui PUBLIC raylib)
target_compile_options(swui PRIVATE -Wall -Wextra)

add_executable(switcher
  src/main.c
  src/testhook.c
  src/screen_search.c
  src/screen_7105.c
  src/screen_7106.c)
target_link_libraries(switcher PRIVATE swcore swui raylib m)
target_compile_options(switcher PRIVATE -Wall -Wextra)

enable_testing()
add_executable(test_proto tests/test_proto.c)
target_link_libraries(test_proto PRIVATE swcore)
add_test(NAME proto COMMAND test_proto)

install(TARGETS switcher RUNTIME DESTINATION bin)
