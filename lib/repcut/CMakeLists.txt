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

if(RCP_USE_MTKAHYPAR)
    # Embed the pinned default preset so installed/static consumers do not need
    # to locate a configuration file in the source tree or install prefix.
    set(mt_preset "${PROJECT_SOURCE_DIR}/external/mt-kahypar/config/default_preset.ini")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${mt_preset}")
    file(READ "${mt_preset}" RCP_MTKAHYPAR_PRESET)
    string(APPEND RCP_MTKAHYPAR_PRESET "\np-louvain-low-memory-contraction=true\n")
    configure_file("${PROJECT_SOURCE_DIR}/cmake/MtKaHyParPreset.h.in"
        "${CMAKE_CURRENT_BINARY_DIR}/MtKaHyParPreset.h" @ONLY)
    target_include_directories(repcut PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
    target_sources(repcut PRIVATE MtKaHyPar.cpp)
    target_compile_definitions(repcut PRIVATE RCP_USE_MTKAHYPAR)
    target_link_libraries(repcut MtKaHyPar::mtkahypar)
endif()

# Expose the public header on the include path both in-tree and when installed.
# Only repcut.h is the public ABI; the other headers under include/ are
# internal to the library.
target_include_directories(repcut PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

# The vendored tiny-process-library header is a private internal dependency.
target_include_directories(repcut PRIVATE
    ${CMAKE_SOURCE_DIR}/external/tiny-process-library)
