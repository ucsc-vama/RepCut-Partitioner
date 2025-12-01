cmake_minimum_required(VERSION 3.13)

if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

project(rcp)
set(CMAKE_CXX_STANDARD 20)

include_directories(include)

# By default the rcp command-line tool is built, which requires
# boost::program_options.  Disable to build and install only librepcut, which
# has zero Boost dependency.
option(RCP_BUILD_CLI "Build the rcp command-line tool (requires boost::program_options)" ON)

add_subdirectory(lib)
add_subdirectory(external/tiny-process-library)

if(RCP_BUILD_CLI)
    find_package(Boost REQUIRED COMPONENTS program_options)
    include_directories(SYSTEM ${Boost_INCLUDE_DIRS})
    message(STATUS "Boost Include: ${Boost_INCLUDE_DIRS}")
    message(STATUS "Boost Libraries: ${Boost_LIBRARIES}")
    add_subdirectory(tools)
    install(TARGETS rcp DESTINATION bin)
endif()

install(TARGETS repcut DESTINATION lib)