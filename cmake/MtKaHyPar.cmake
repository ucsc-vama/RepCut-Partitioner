# Build the pinned dependency in a local scope so its options do not change
# the parent project's library type or optional frontends.
function(rcp_add_mtkahypar)
    set(mt_source "${PROJECT_SOURCE_DIR}/external/mt-kahypar")
    set(mt_binary "${PROJECT_BINARY_DIR}/external/mt-kahypar")
    if(NOT EXISTS "${mt_source}/CMakeLists.txt")
        message(FATAL_ERROR
            "MtKaHyPar submodule is missing. Run: git submodule update --init --recursive")
    endif()

    set(BUILD_SHARED_LIBS OFF)
    set(KAHYPAR_ENABLE_TESTING OFF)
    set(KAHYPAR_PYTHON OFF)
    set(KAHYPAR_INSTALL_CLI OFF)
    set(KAHYPAR_DOWNLOAD_TBB OFF)
    set(KAHYPAR_STATIC_LINK_DEPENDENCIES OFF)
    set(KAHYPAR_STATIC_LINK_TBB OFF)
    add_subdirectory("${mt_source}" "${mt_binary}" EXCLUDE_FROM_ALL)

    # The archive contains upstream's object library. Installed consumers need
    # only its system link dependencies, not the internal build targets.
    get_target_property(mt_links mtkahypar INTERFACE_LINK_LIBRARIES)
    set_property(TARGET mtkahypar PROPERTY INTERFACE_LINK_LIBRARIES
        "$<BUILD_INTERFACE:${mt_links}>"
        "$<INSTALL_INTERFACE:TBB::tbb;TBB::tbbmalloc>")
    get_target_property(mt_flag_links MtKaHyPar-BuildFlags INTERFACE_LINK_LIBRARIES)
    if(mt_flag_links)
        target_link_libraries(mtkahypar INTERFACE "$<INSTALL_INTERFACE:${mt_flag_links}>")
    endif()
    # Static consumers also need sanitizer/coverage runtimes when enabled.
    # Keep the builder's optional linker choice out of the installed package.
    get_target_property(mt_link_options MtKaHyPar-BuildFlags INTERFACE_LINK_OPTIONS)
    if(mt_link_options)
        list(REMOVE_ITEM mt_link_options "-fuse-ld=lld" "/LINKER:LLD")
        target_link_options(mtkahypar INTERFACE ${mt_link_options})
    endif()
    if(NOT KAHYPAR_DISABLE_HWLOC)
        set_property(TARGET mtkahypar APPEND PROPERTY INTERFACE_LINK_LIBRARIES
            "$<INSTALL_INTERFACE:repcut::hwloc>")
        set(RCP_MTKAHYPAR_USE_HWLOC ON PARENT_SCOPE)
    endif()

    # Upstream installs only when built as the top-level project.
    install(TARGETS mtkahypar EXPORT repcut-targets
            ARCHIVE DESTINATION lib)
    install(FILES "${mt_source}/include/mtkahypar.h" "${mt_source}/include/mtkahypartypes.h"
            DESTINATION include)

    # These header dependencies are compiled into the bundled library.
    install(FILES "${mt_source}/LICENSE"
            DESTINATION share/licenses/repcut RENAME MtKaHyPar-LICENSE)
    foreach(dependency kahypar-shared-resources WHFC parlay)
        if(EXISTS "${mt_binary}/external_tools/${dependency}/LICENSE")
            install(FILES "${mt_binary}/external_tools/${dependency}/LICENSE"
                    DESTINATION share/licenses/repcut RENAME "${dependency}-LICENSE")
        endif()
    endforeach()
    include(FetchContent)
    FetchContent_GetProperties(cli11 SOURCE_DIR cli11_source)
    install(FILES "${cli11_source}/LICENSE"
            DESTINATION share/licenses/repcut RENAME CLI11-LICENSE)
endfunction()

rcp_add_mtkahypar()
