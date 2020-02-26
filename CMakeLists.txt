cmake_minimum_required(VERSION 3.12)
project(pastafarian VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

# adding deps
add_subdirectory(extern/slang)
add_subdirectory(extern/googletest)

# adding src
add_subdirectory(src)

include(GoogleTest)
include (CTest)


add_subdirectory(tests)