#pragma once

#include <cstdint>
#include <string_view>

#include <GLFW/glfw3.h>

namespace demo
{

class window
{
private:
    GLFWwindow *_handle;
public:
    window(std::string_view title, uint32_t width, uint32_t height);

    void poll();

    [[nodiscard]] bool should_close() const noexcept;
};

}
