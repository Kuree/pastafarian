add_executable(detector detector.cc)
target_link_libraries(detector pastafarian)
target_include_directories(detector PRIVATE ../src ../extern/CLI11/include)