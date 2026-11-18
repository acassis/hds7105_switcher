cmake_minimum_required(VERSION 3.16)
project(switcher C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
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

enable_testing()
add_executable(test_proto tests/test_proto.c)
target_link_libraries(test_proto PRIVATE swcore)
add_test(NAME proto COMMAND test_proto)
