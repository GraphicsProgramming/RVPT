cmake_minimum_required(VERSION 3.10.2)
project(RVPT)

set(CMAKE_CXX_STANDARD 17)

find_package(Vulkan REQUIRED)
add_subdirectory(external)

add_executable(rvpt src/rvpt/main.cpp)
target_include_directories(rvpt PRIVATE ${Vulkan_INCLUDE_DIRS})
target_include_directories(rvpt PRIVATE external) # For stb_image, tinyobjloader
target_link_libraries(rvpt ${Vulkan_LIBRARIES} glfw vk-bootstrap glm nlohmann_json::nlohmann_json)
