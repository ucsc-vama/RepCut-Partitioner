cmake_minimum_required(VERSION 3.13)

if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

project(rcp)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# By default the rcp command-line tool is built, which requires
# boost::program_options.  Disable to build and install only librepcut, which
# has zero Boost dependency.
option(RCP_BUILD_CLI "Build the rcp command-line tool (requires boost::program_options)" ON)

# All installable targets go into the `repcut-targets` export set so that
# downstream consumers can `find_package(repcut)` and link against the
# imported target `repcut::repcut`.
add_subdirectory(lib)

if(RCP_BUILD_CLI)
    find_package(Boost REQUIRED COMPONENTS program_options)
    add_subdirectory(tools)
    install(TARGETS rcp DESTINATION bin)
endif()

install(TARGETS repcut
        EXPORT repcut-targets
        DESTINATION lib)
install(FILES include/repcut.h DESTINATION include)

# Generate and install the package config so consumers can do:
#   find_package(repcut CONFIG REQUIRED)
#   target_link_libraries(my_app PRIVATE repcut::repcut)
include(CMakePackageConfigHelpers)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/repcut-config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/repcut-config.cmake"
    INSTALL_DESTINATION lib/cmake/repcut)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/repcut-config-version.cmake"
    VERSION 0.1.0
    COMPATIBILITY SameMajorVersion)

install(EXPORT repcut-targets
        FILE repcut-targets.cmake
        NAMESPACE repcut::
        DESTINATION lib/cmake/repcut)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/repcut-config.cmake"
              "${CMAKE_CURRENT_BINARY_DIR}/repcut-config-version.cmake"
        DESTINATION lib/cmake/repcut)