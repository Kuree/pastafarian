add_library(pastafarian graph.cc graph.hh parser.cc parser.hh)

target_include_directories(pastafarian PUBLIC ../extern/slang/include ../extern/fmt/include)

target_link_libraries(pastafarian slang)