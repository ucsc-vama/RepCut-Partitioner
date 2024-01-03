

add_executable(rcp
        main.cpp
        commandline_options.cpp
)


target_link_libraries(rcp ${Boost_LIBRARIES})
target_link_libraries(rcp repcut)