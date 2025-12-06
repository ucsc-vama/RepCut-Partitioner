add_library(repcut STATIC)

target_sources(
    repcut

    PRIVATE
    ClusterGraph.cpp
    ConeTrie.cpp
    DAG.cpp
    Util.cpp
    RepCut.cpp
    RepCutPartitioner.cpp
    ${CMAKE_SOURCE_DIR}/external/tiny-process-library/process.cpp
    )

if(WIN32)
    target_sources(repcut PRIVATE
        ${CMAKE_SOURCE_DIR}/external/tiny-process-library/process_win.cpp)
else()
    target_sources(repcut PRIVATE
        ${CMAKE_SOURCE_DIR}/external/tiny-process-library/process_unix.cpp)
endif()

find_package(Threads REQUIRED)
target_link_libraries(repcut Threads::Threads)

# Expose the public header on the include path both in-tree and when installed.
# Only repcut.h is the public ABI; the other headers under include/ are
# internal to the library.
target_include_directories(repcut PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

# The vendored tiny-process-library header is a private internal dependency.
target_include_directories(repcut PRIVATE
    ${CMAKE_SOURCE_DIR}/external/tiny-process-library)