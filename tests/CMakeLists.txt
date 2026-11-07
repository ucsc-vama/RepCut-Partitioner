add_executable(test-library test_library.cpp)
target_link_libraries(test-library PRIVATE repcut)
add_test(NAME library-overlap
    COMMAND test-library overlap "${CMAKE_CURRENT_BINARY_DIR}/overlap")
if(RCP_USE_MTKAHYPAR)
    add_test(NAME library-repeat-1t
        COMMAND test-library repeat "${CMAKE_CURRENT_BINARY_DIR}/repeat-1t" 1)
    add_test(NAME library-repeat-2t
        COMMAND test-library repeat "${CMAKE_CURRENT_BINARY_DIR}/repeat-2t" 2)
endif()
set_tests_properties(library-overlap PROPERTIES TIMEOUT 40)
