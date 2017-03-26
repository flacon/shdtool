cmake_minimum_required(VERSION 2.6)

project(shdtool "C")

set(MAJOR_VERSION 4)
set(MINOR_VERSION 0)
set(PATCH_VERSION 0)
set(${PROJECT_NAME}_VERSION ${MAJOR_VERSION}.${MINOR_VERSION}.${PATCH_VERSION})

add_definitions(-D${PROJECT_NAME}_MAJOR_VERSION=\"${MAJOR_VERSION}\")
add_definitions(-D${PROJECT_NAME}_MINOR_VERSION=\"${MINOR_VERSION}\")
add_definitions(-D${PROJECT_NAME}_PATCH_VERSION=\"${PATCH_VERSION}\")
add_definitions(-D${PROJECT_NAME}_VERSION=\"${FLACON_VERSION}\")


set(HEADERS
    include/binary.h
    include/config.h
    include/convert.h
    include/core.h
    include/fileio.h
    include/format.h
    include/format-types.h
    include/md5.h
    include/mode.h
    include/mode-types.h
    include/module.h
    include/module-types.h
    include/output.h
    include/sha1.h
    include/shntool.h
    include/stamp-h1
    include/wave.h
)

set(SOURCES
    src/core_convert.c
    src/core_fileio.c
    src/core_format.c
    src/core_mode.c
    src/core_module.c
    src/core_output.c
    src/core_shntool.c
    src/core_wave.c

    src/format_aiff.c
    src/format_alac.c
    src/format_als.c
    src/format_ape.c
    src/format_bonk.c
    src/format_cust.c
    src/format_flac.c
    src/format_kxs.c
    src/format_la.c
    src/format_lpac.c
    src/format_mkw.c
    src/format_null.c
    src/format_ofr.c
    src/format_shn.c
    src/format_tak.c
    src/format_term.c
    src/format_tta.c
    src/format_wav.c
    src/format_wv.c

    src/mode_len.c
    src/mode_fix.c
    src/mode_hash.c
    src/mode_pad.c
    src/mode_join.c
    src/mode_split.c
    src/mode_cat.c
    src/mode_cmp.c
    src/mode_cue.c
    src/mode_conv.c
    src/mode_info.c
    src/mode_strip.c
    src/mode_gen.c
    src/mode_trim.c
)

set(LIBRARIES
)

include("funcs.cmake")

set(MODES )
foreach(F ${SOURCES})
    get_filename_component(F ${F} NAME_WE)
    string(FIND ${F} "mode_" MATCH)

    if( ${MATCH} EQUAL 0)
        string(REPLACE "mode_" "" M ${F})
        set(MODES ${MODES} ${M})
    endif()
endforeach()


if(CMAKE_COMPILER_IS_GNUCXX)
    add_definitions (-Wall -pedantic -Wno-long-long -Werror=format-security)
endif()

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src

    ${CMAKE_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)



# Generate glue_modes.c
set(GLUE_MODULES "${CMAKE_CURRENT_BINARY_DIR}/glue_modes.c")
create_glue_modules(${GLUE_MODULES})

# Generate glue_formats.c
set(GLUE_FORMATS "${CMAKE_CURRENT_BINARY_DIR}/glue_formats.c")
create_glue_formats(${GLUE_FORMATS})


#*******************************************
add_executable(${PROJECT_NAME} ${HEADERS} ${SOURCES} ${GLUE_MODULES} ${GLUE_FORMATS})
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
target_link_libraries(${PROJECT_NAME} ${LIBRARIES})


# Create and install symlinks **************
foreach(MODE ${MODES})
    add_custom_command(TARGET ${PROJECT_NAME}
        COMMAND ${CMAKE_COMMAND} -E create_symlink
        "${PROJECT_NAME}"
        "shd${MODE}"
    )
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/shd${MODE}" DESTINATION bin)
endforeach()

# Man pages ********************************
create_man("shntool.1" "man/shntool.1")

