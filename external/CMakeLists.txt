option(BUILD_TESTING "" OFF)
option(JSON_Install "" OFF)
option(JSON_BuildTests "" OFF)
option(JSON_MultipleHeaders "" OFF)

option(GLFW_BUILD_TESTS "" OFF)
option(GLFW_BUILD_DOCS "" OFF)
option(GLFW_INSTALL "" OFF)
option(GLFW_BUILD_EXAMPLES "" OFF)

include(FetchContent)
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw
    GIT_TAG        3.3.2
)
FetchContent_Declare(
    vk_bootstrap
    GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap
    GIT_TAG     master
)
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm
    GIT_TAG        0.9.9.8
)
FetchContent_Declare(
    nlohman_json
    GIT_REPOSITORY https://github.com/nlohmann/json
    GIT_TAG        v3.7.3
)

FetchContent_MakeAvailable(glfw vk_bootstrap glm nlohman_json)

add_library(lib_imgui imgui/imgui.cpp imgui/imgui_draw.cpp imgui/imgui_widgets.cpp)
target_include_directories(lib_imgui PRIVATE imgui)
