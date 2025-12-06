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
    )

# librepcut has no Boost dependency — only the rcp CLI tool links
# boost::program_options.  See the top-level CMakeLists.txt option
# RCP_BUILD_CLI.
target_link_libraries(repcut tiny-process-library)

# Expose the public header on the include path both in-tree and when installed.
# Only repcut.h is the public ABI; the other headers under include/ are
# internal to the library.
target_include_directories(repcut PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)