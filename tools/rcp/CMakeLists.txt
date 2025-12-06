add_executable(rcp
        main.cpp
        CommandlineOptions.cpp
)

target_link_libraries(rcp PRIVATE Boost::program_options repcut)