add_library(pastafarian graph.cc graph.hh parser.cc parser.hh util.cc util.hh fsm.cc fsm.hh codegen.cc codegen.hh
        source.cc source.hh)

target_include_directories(pastafarian PUBLIC ../extern/fmt/include ../extern/simdjson/include/ ../extern/cxxpool/src
        ../extern/tqdm ../extern/cpp-subprocess)

target_link_libraries(pastafarian PRIVATE fmt simdjson ${CMAKE_THREAD_LIBS_INIT} stdc++fs)
