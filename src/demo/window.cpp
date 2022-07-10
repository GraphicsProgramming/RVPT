#include "window.h"

#include <fmt/core.h>

demo::window::window(std::string_view title, uint32_t width, uint32_t height) {
    if (!glfwInit())
        fmt::print("failed to init glfw\n");

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    _handle = glfwCreateWindow(width, height, title.data(), nullptr, nullptr);

    if (_handle == nullptr)
        fmt::print("failed to create glfw window");

    glfwSetCursorPos(_handle, 0.0f, 0.0f);
}

void demo::window::poll() {
    glfwPollEvents();
}

bool demo::window::should_close() const noexcept {
    return glfwWindowShouldClose(_handle);
}
