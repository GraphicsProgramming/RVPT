cmake_minimum_required(VERSION 3.11.4)
project(RVPT)

set(CMAKE_CXX_STANDARD 17)

find_package(Vulkan REQUIRED)
add_subdirectory(external)

set(source_files
        src/rvpt/main.cpp
        src/rvpt/rvpt.cpp
        src/rvpt/window.cpp
        src/rvpt/imgui_impl.cpp
        ##assets/shaders/fullscreen_tri.vert
        ##assets/shaders/tex_sample.frag
        src/rvpt/bvh.cpp
        src/rvpt/bvh_builder.cpp)

set(header_files
        src/rvpt/rvpt.h
        src/rvpt/window.h
        src/rvpt/imgui_impl.h
        src/rvpt/geometry.h
        src/rvpt/bvh.h
        src/rvpt/bvh_builder.h
        )

set (shader_files
    assets/shaders/camera.glsl
    assets/shaders/compute_pass.comp
    assets/shaders/debug_vis.frag
    assets/shaders/debug_vis.vert
    assets/shaders/distance_functions.glsl
    assets/shaders/fullscreen_tri.vert
    assets/shaders/integrators.glsl
    assets/shaders/intersection.glsl
    assets/shaders/material.glsl
    assets/shaders/samples_mapping.glsl
    assets/shaders/structs.glsl
    assets/shaders/tex_sample.frag
    assets/shaders/util.glsl
)

add_library(rvpt-common
        src/common/vk_util.cpp
        src/common/vk_util.h
        src/common/camera.cpp
        src/common/camera.h
        src/common/timer.cpp
        src/common/timer.h)

target_include_directories(rvpt-common PRIVATE ${Vulkan_INCLUDE_DIRS})
target_include_directories(rvpt-common PRIVATE external src/common) # For stb_image, tinyobjloader
target_link_libraries(rvpt-common ${Vulkan_LIBRARIES} glfw vk-bootstrap glm nlohmann_json::nlohmann_json fmt lib_imgui)

add_executable(rvpt ${source_files} ${header_files})

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT rvpt)

target_include_directories(rvpt PRIVATE ${Vulkan_INCLUDE_DIRS})
target_include_directories(rvpt PRIVATE external src/common) # For stb_image, tinyobjloader
target_link_libraries(rvpt ${Vulkan_LIBRARIES} rvpt-common glfw vk-bootstrap glm nlohmann_json::nlohmann_json fmt lib_imgui)

#add_executable(rvpt-demo ${DEMO_SOURCE_FILES})
add_executable(rvpt-demo
        src/demo/main.cpp

        src/demo/window.cpp
        src/demo/window.h
        src/demo/engine.cpp
        src/demo/engine.h
        )

target_include_directories(rvpt-demo PRIVATE ${Vulkan_INCLUDE_DIRS})
target_include_directories(rvpt-demo PRIVATE external src/common) # For stb_image, tinyobjloader
target_link_libraries(rvpt-demo ${Vulkan_LIBRARIES} rvpt-common glfw vk-bootstrap glm nlohmann_json::nlohmann_json fmt lib_imgui)
target_compile_definitions(rvpt-demo PRIVATE -DGLFW_INCLUDE_NONE)

if (DEBUG)
    if (WIN32)
        target_compile_options(rvpt PUBLIC /fsanitize=address)
    else()
        target_compile_options(rvpt PUBLIC -fsanitize=address)
    endif()
endif()

# Determine whether we're compiling with clang++
string(FIND "${CMAKE_CXX_COMPILER}" "clang++" RVPT_COMPILER_CLANGPP)
if(RVPT_COMPILER_CLANGPP GREATER -1)
    set(RVPT_COMPILER_CLANGPP 1)
else()
    set(RVPT_COMPILER_CLANGPP 0)
endif()

target_compile_options(rvpt
        PUBLIC
        $<$<OR:$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>,${RVPT_COMPILER_CLANGPP}>:
        -Wall
        -Wextra
        -pedantic-errors
        -Wconversion
        -Wsign-conversion>
        $<$<CXX_COMPILER_ID:MSVC>:
        /WX
        /W4
        /EHsc>
        )

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(fmt PUBLIC /EHsc)
endif()


#compile shaders in assets/shaders
if(WIN32)
    add_custom_target(compile-shaders ALL
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/assets/shaders/
            COMMAND cmd /c ${CMAKE_SOURCE_DIR}/scripts/compile_shaders.bat
            DEPENDS ${shader_files})
else(UNIX)
    add_custom_target(compile-shaders ALL
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/assets/shaders/
            COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/compile_shaders.sh
            DEPENDS ${shader_files})
endif()

# copies files to the build folder
add_custom_target(copy-asset-files ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets ${CMAKE_BINARY_DIR}/assets
        DEPENDS compile-shaders rvpt)

add_dependencies(rvpt compile-shaders)
add_dependencies(rvpt-demo compile-shaders)

# sets up path to source directory, useful for shader hot reloading
configure_file (
        "${PROJECT_SOURCE_DIR}/scripts/project_configuration.json.in"
        "${PROJECT_BINARY_DIR}/project_configuration.json"
)
