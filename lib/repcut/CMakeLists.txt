add_library(repcut)

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