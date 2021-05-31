
configure_file (
    "rvpt_demo_config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/rvpt_demo_config.h"
)

set(rvpt_demo_src
    src/main.cpp
    src/demo.cpp
    src/imgui_impl.cpp
    src/window.cpp)

add_executable(rvpt-demo ${rvpt_demo_src})
target_include_directories(rvpt-demo  PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(rvpt-demo rvpt_compiler_warnings rvpt util_rvpt glfw glm nlohmann_json::nlohmann_json fmt lib_imgui rvpt_external_header_dependencies)

# copies shaders to the build folder
add_custom_target(copy-asset-files ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/assets ${CMAKE_CURRENT_BINARY_DIR}/assets
        DEPENDS rvpt-demo)

add_dependencies(rvpt-demo compile-shaders)