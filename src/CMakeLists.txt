add_library(pastafarian graph.cc graph.hh parser.cc parser.hh)

target_include_directories(pastafarian PUBLIC ../extern/fmt/include ../extern/rapidjson/include)

target_link_libraries(pastafarian fmt)
