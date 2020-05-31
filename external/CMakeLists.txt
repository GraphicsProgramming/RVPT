option(BUILD_TESTING "" OFF)
option(JSON_Install "" OFF)
option(JSON_BuildTests "" OFF)
option(JSON_MultipleHeaders "" OFF)

option(GLFW_BUILD_TESTS "" OFF)
option(GLFW_BUILD_DOCS "" OFF)
option(GLFW_INSTALL "" OFF)
option(GLFW_BUILD_EXAMPLES "" OFF)

add_subdirectory(glfw)
add_subdirectory(vk-bootstrap)
add_subdirectory(glm)
add_subdirectory(nlohmann-json)

add_library(lib_imgui imgui/imgui.cpp imgui/imgui_draw.cpp imgui/imgui_widgets.cpp)
target_include_directories(lib_imgui PRIVATE imgui)
