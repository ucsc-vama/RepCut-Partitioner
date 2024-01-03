add_library(repcut)

# find_package(Boost 1.69 REQUIRED COMPONENTS program_options log_setup log)
# #find_package(Boost 1.69 REQUIRED COMPONENTS log)

include_directories(SYSTEM ${Boost_INCLUDE_DIRS})

target_sources(
    repcut

    PRIVATE 
    cluster_graph.cpp
    dag.cpp
    hyper_graph.cpp
    partition_stat.cpp
    rcp_util.cpp
    reconstructor.cpp 
    rep_cut_partitioner.cpp 
    SBitSet.cpp 
    )

target_compile_features(repcut PUBLIC cxx_std_17)
target_link_libraries(repcut ${Boost_LIBRARIES})

# target_include_directories()