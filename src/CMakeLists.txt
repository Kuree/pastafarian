# SIMD json libary
add_library(json ../extern/simdjson/singleheader/simdjson.cpp ../extern/simdjson/singleheader/simdjson.h)

add_library(pastafarian graph.cc graph.hh parser.cc parser.hh util.cc util.hh)

target_include_directories(pastafarian PUBLIC ../extern/fmt/include ../extern/simdjson/singleheader/)

target_link_libraries(pastafarian fmt json)
