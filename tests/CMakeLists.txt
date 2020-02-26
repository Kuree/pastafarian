# test files
file(GLOB TEST_VECTORS ${CMAKE_CURRENT_SOURCE_DIR}/vectors/*.sv)
foreach (_test_file ${TEST_VECTORS})
    get_filename_component(_test_name ${_test_file} NAME)
    configure_file(${_test_file}
            ${CMAKE_CURRENT_BINARY_DIR}/${_test_name} COPYONLY)
endforeach ()

add_executable(test_parse test_parse.cc)
target_link_libraries(test_parse gtest pastafarian gtest_main)
gtest_discover_tests(test_parse
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/vectors)
