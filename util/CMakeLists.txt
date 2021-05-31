set(util_files
    src/timer.cpp
    src/vk_util.cpp
)

add_library(util_rvpt STATIC ${util_files})
target_include_directories(util_rvpt PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>/include)
target_link_libraries(util_rvpt PRIVATE rvpt_compiler_warnings PUBLIC Vulkan::Vulkan vk-bootstrap glm fmt lib_imgui)