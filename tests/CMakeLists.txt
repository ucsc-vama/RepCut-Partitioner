add_executable(test-library test_library.cpp)
target_link_libraries(test-library PRIVATE repcut)
add_test(NAME library-overlap
    COMMAND test-library overlap "${CMAKE_CURRENT_BINARY_DIR}/overlap")
add_executable(test-hypergraph test_hypergraph.cpp)
target_link_libraries(test-hypergraph PRIVATE repcut)
add_test(NAME hypergraph-merge
    COMMAND test-hypergraph "${CMAKE_CURRENT_BINARY_DIR}/hypergraph")
if(RCP_USE_MTKAHYPAR)
    # Inspect the actual parsed context against the pinned upstream definition.
    # Production code uses only MtKaHyPar's public C API.
    add_executable(test-mtkahypar-config test_mtkahypar_config.cpp)
    target_link_libraries(test-mtkahypar-config PRIVATE repcut MtKaHyPar-Include MtKaHyPar-BuildFlags)
    target_compile_definitions(test-mtkahypar-config PRIVATE MT_KAHYPAR_LIBRARY_MODE)
    add_test(NAME mtkahypar-low-memory-config
        COMMAND test-mtkahypar-config "${CMAKE_CURRENT_BINARY_DIR}/config with spaces")
    add_test(NAME library-repeat-1t
        COMMAND test-library repeat "${CMAKE_CURRENT_BINARY_DIR}/repeat-1t" 1)
    add_test(NAME library-repeat-2t
        COMMAND test-library repeat "${CMAKE_CURRENT_BINARY_DIR}/repeat-2t" 2)
endif()
set_tests_properties(library-overlap PROPERTIES TIMEOUT 40)
