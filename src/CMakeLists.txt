add_library(pastafarian graph.cc graph.hh parser.cc parser.hh util.cc util.hh fsm.cc fsm.hh)

target_include_directories(pastafarian PUBLIC ../extern/fmt/include ../extern/simdjson/include/)

target_link_libraries(pastafarian fmt simdjson)
