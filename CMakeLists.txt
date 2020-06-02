cmake_minimum_required(VERSION 3.10.2)
project(RVPT)

set(CMAKE_CXX_STANDARD 17)

find_package(Vulkan REQUIRED)
add_subdirectory(external)

add_executable(rvpt src/rvpt/main.cpp src/rvpt/rvpt.cpp src/rvpt/window.cpp src/rvpt/vk_util.cpp
    assets/shaders/fullscreen_tri.vert assets/shaders/tex_sample.frag src/rvpt/camera.cpp src/rvpt/timer.cpp)
target_include_directories(rvpt PRIVATE ${Vulkan_INCLUDE_DIRS})
target_include_directories(rvpt PRIVATE external) # For stb_image, tinyobjloader
target_link_libraries(rvpt ${Vulkan_LIBRARIES} glfw vk-bootstrap glm nlohmann_json::nlohmann_json)

#compile shaders in assets/shaders
if(WIN32)
add_custom_target(compile-shaders ALL
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/assets/shaders/
    COMMAND cmd /c ${CMAKE_SOURCE_DIR}/scripts/compile_shaders.bat
    DEPENDS rvpt)
else(UNIX)
add_custom_target(compile-shaders ALL
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/assets/shaders/
    COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/compile_shaders.sh
    DEPENDS rvpt)
endif()

# copies files to the build folder
add_custom_target(copy-asset-files ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets ${CMAKE_BINARY_DIR}/assets
    DEPENDS compile-shaders rvpt)

# sets up path to source directory, useful for shader hot reloading
configure_file (
    "${PROJECT_SOURCE_DIR}/scripts/project_configuration.json.in"
    "${PROJECT_BINARY_DIR}/project_configuration.json"
)
