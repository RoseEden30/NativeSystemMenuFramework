# Build configuration for an SKSE plugin.
# The project name drives the name of the .dll, the .ini and the .log file.

cmake_minimum_required(VERSION 3.21)

project(NativeSystemMenuFramework VERSION 1.0.1 LANGUAGES CXX)

set(PLUGIN_AUTHOR "RoseEden" CACHE STRING "Author name baked into the plugin declaration")

# Folder created under the mods folder. Separate from the project name so it can
# carry a space without renaming the files.
set(MOD_FOLDER_NAME "${PROJECT_NAME}" CACHE STRING "Folder name under SKYRIM_MODS_FOLDER")

# Output location, picked up from the environment so this file stays portable.
# Set SKYRIM_FOLDER to a Skyrim Special Edition install, or SKYRIM_MODS_FOLDER
# to the "mods" folder of Mod Organizer 2 or Vortex.
if(DEFINED ENV{SKYRIM_FOLDER} AND IS_DIRECTORY "$ENV{SKYRIM_FOLDER}/Data")
    set(OUTPUT_FOLDER "$ENV{SKYRIM_FOLDER}/Data")
endif()

if(DEFINED ENV{SKYRIM_MODS_FOLDER} AND IS_DIRECTORY "$ENV{SKYRIM_MODS_FOLDER}")
    set(OUTPUT_FOLDER "$ENV{SKYRIM_MODS_FOLDER}/${MOD_FOLDER_NAME}")
endif()

set(BUILD_TESTS OFF CACHE BOOL "" FORCE)

# CommonLibSSE-NG is a git submodule pinned under extern/.
set(
    COMMONLIBSSE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/extern/CommonLibSSE-NG"
    CACHE PATH
    "Path to CommonLibSSE-NG"
)

add_subdirectory(
    "${COMMONLIBSSE_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}/commonlibsse-ng"
)

include("${COMMONLIBSSE_DIR}/cmake/CommonLibSSE.cmake")

# Every .cpp under src is compiled. CONFIGURE_DEPENDS re-runs the glob when a
# file is added, so a new source does not need a manual edit here.
file(GLOB_RECURSE PLUGIN_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_commonlibsse_plugin(${PROJECT_NAME} AUTHOR ${PLUGIN_AUTHOR} SOURCES ${PLUGIN_SOURCES})

# src for our own headers, include for header-only third party ones.
target_include_directories(${PROJECT_NAME} PRIVATE src include)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
target_precompile_headers(${PROJECT_NAME} PRIVATE src/PCH.h)

# Symbols in every configuration, so a crash log names a function instead of an
# offset. /Z7 keeps the debug info in the object files; /DEBUG writes the .pdb
# and switches /OPT off, so REF goes back in. ICF stays off, since folded
# functions resolve to the wrong name.
target_compile_options(${PROJECT_NAME} PRIVATE /Z7)
target_link_options(${PROJECT_NAME} PRIVATE /DEBUG /OPT:REF /OPT:NOICF)

# Deploy the plugin after every build.
if(DEFINED OUTPUT_FOLDER)
    set(DLL_FOLDER "${OUTPUT_FOLDER}/SKSE/Plugins")

    message(STATUS "SKSE plugin output folder: ${DLL_FOLDER}")

    add_custom_target(deploy_content ALL
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${DLL_FOLDER}"
        VERBATIM
    )
    add_dependencies(deploy_content "${PROJECT_NAME}")

    add_custom_command(
        TARGET deploy_content
        POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:${PROJECT_NAME}>" "${DLL_FOLDER}/$<TARGET_FILE_NAME:${PROJECT_NAME}>"
        VERBATIM
    )

    # Crash Logger SSE reads a .pdb sitting next to the plugin.
    add_custom_command(
        TARGET deploy_content
        POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_PDB_FILE:${PROJECT_NAME}>" "${DLL_FOLDER}/$<TARGET_PDB_FILE_NAME:${PROJECT_NAME}>"
        VERBATIM
    )

    # Settings file, only when the project actually ships one. Only deployed
    # if missing - never overwrites settings already tweaked in game, in dev
    # or on a player's install.
    set(INI_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/SKSE/Plugins/${PROJECT_NAME}.ini")
    if(EXISTS "${INI_SOURCE}")
        add_custom_command(
            TARGET deploy_content
            POST_BUILD
            COMMAND "${CMAKE_COMMAND}"
                "-DSRC=${INI_SOURCE}"
                "-DDST=${DLL_FOLDER}/${PROJECT_NAME}.ini"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CopyIfMissing.cmake"
            VERBATIM
        )
    endif()

    # Translation files, if the project ships any. Overwritten every build,
    # unlike the ini: these are content, not settings a player edits.
    if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Interface/Translations")
        add_custom_command(
            TARGET deploy_content
            POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/Interface/Translations"
                "${OUTPUT_FOLDER}/Interface/Translations"
            VERBATIM
        )
    endif()
endif()