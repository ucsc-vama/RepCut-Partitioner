add_executable(rcp
        main.cpp
        CommandlineOptions.cpp
)

# Boost is scoped to just the rcp CLI target — librepcut itself is Boost-free.
target_link_libraries(rcp PRIVATE Boost::program_options repcut)