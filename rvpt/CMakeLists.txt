# sets up path to shaders directory, necessary for shader hot reloading
configure_file (
        "rvpt_config.h.in"
        "${CMAKE_CURRENT_BINARY_DIR}/rvpt_config.h"
)

set(rvpt_source_files
    src/rvpt.cpp
    src/camera.cpp
    src/bvh.cpp
    src/bvh_builder.cpp)

add_library(rvpt ${rvpt_source_files})
target_include_directories(rvpt PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>/include)
target_include_directories(rvpt PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>)

target_link_libraries(rvpt PRIVATE util_rvpt rvpt_compiler_warnings)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT rvpt)

if (DEBUG)
    if (WIN32)
        target_compile_options(rvpt PUBLIC /fsanitize=address)
    else()
        target_compile_options(rvpt PUBLIC -fsanitize=address)
    endif()
endif()


set (shader_files
    shaders/camera.glsl
    shaders/compute_pass.comp
    shaders/debug_vis.frag
    shaders/debug_vis.vert
    shaders/distance_functions.glsl
    shaders/fullscreen_tri.vert
    shaders/integrators.glsl
    shaders/intersection.glsl
    shaders/material.glsl
    shaders/samples_mapping.glsl
    shaders/structs.glsl
    shaders/tex_sample.frag
    shaders/util.glsl
)

#compile shaders in assets/shaders
if(WIN32)
    add_custom_target(compile-shaders ALL
            WORKING_DIRECTORY $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>/shaders
            COMMAND cmd /c $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>/scripts/compile_shaders.bat
            DEPENDS ${shader_files})
elseif(UNIX)
    add_custom_target(compile-shaders ALL
            WORKING_DIRECTORY $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>/shaders
            COMMAND bash $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>/scripts/compile_shaders.sh
            DEPENDS ${shader_files})
else()
    message(FATAL_ERROR "Unsupported Platform!")
endif()

# copies shaders to the build folder
add_custom_target(copy-shader-files ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/shaders ${CMAKE_BINARY_DIR}/shaders
        DEPENDS compile-shaders rvpt)

add_dependencies(rvpt compile-shaders)