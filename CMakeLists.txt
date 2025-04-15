cmake_minimum_required(VERSION 3.13)

if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

project(rcp)
set(CMAKE_CXX_STANDARD 20)

include_directories(include)
# include_directories(lib)



find_package(Boost REQUIRED COMPONENTS program_options log_setup log filesystem graph)
#find_package(Boost 1.69 REQUIRED COMPONENTS log)

include_directories(SYSTEM ${Boost_INCLUDE_DIRS})

message(STATUS "Boost Include: ${Boost_INCLUDE_DIRS}")
message(STATUS "Boost Library Dirs: ${Boost_LIBRARY_DIRS}")
message(STATUS "Boost Libraries: ${Boost_LIBRARIES}")


add_subdirectory(lib)
add_subdirectory(tools)

add_subdirectory(external/tiny-process-library)

install(TARGETS rcp DESTINATION bin)
install(TARGETS repcut DESTINATION lib)