add_library(repcut)

# find_package(Boost 1.69 REQUIRED COMPONENTS program_options log_setup log)
# #find_package(Boost 1.69 REQUIRED COMPONENTS log)

include_directories(SYSTEM ${Boost_INCLUDE_DIRS})

target_sources(
    repcut

    PRIVATE 
    ClusterGraph.cpp
    ConeTrie.cpp
    DAG.cpp
    Util.cpp
    RepCutPartitioner.cpp 
    )

# target_compile_features(repcut PUBLIC cxx_std_17)
target_link_libraries(repcut ${Boost_LIBRARIES} tiny-process-library)

# target_include_directories()