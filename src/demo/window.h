#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include <GLFW/glfw3.h>

#include <glm/glm.hpp>

namespace demo
{

class window
{
private:
    GLFWwindow *_handle;
    std::string _title;
    glm::ivec2 _size;

public:
    window(std::string_view title, uint32_t width, uint32_t height);

    void poll();

    [[nodiscard]] bool should_close() const noexcept;

    [[nodiscard]] std::string_view title() const noexcept;

    [[nodiscard]] glm::ivec2 size() const noexcept;

    [[nodiscard]] GLFWwindow *handle() const noexcept;
};

}
