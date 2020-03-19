cmake_minimum_required(VERSION 3.12)
project(pastafarian VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

# adding deps
add_subdirectory(extern/simdjson)
add_subdirectory(extern/googletest)
add_subdirectory(extern/fmt)

# strict compile options
if (MSVC)
    add_compile_options(/W4 /WX)
else ()
    add_compile_options(-Wall -Wextra -Werror)
endif ()


# adding src
add_subdirectory(src)
# tools
add_subdirectory(tools)

include(GoogleTest)
include(CTest)


add_subdirectory(tests)
