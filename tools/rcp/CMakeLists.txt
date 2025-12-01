

add_executable(rcp
        main.cpp
        CommandlineOptions.cpp
)


target_link_libraries(rcp ${Boost_LIBRARIES})
target_link_libraries(rcp repcut)