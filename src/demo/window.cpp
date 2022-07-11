#include "window.h"

#include <fmt/core.h>

demo::window::window(std::string_view title, uint32_t width, uint32_t height) : _title(title), _size(width, height) {
    if (!glfwInit())
        fmt::print("failed to init glfw\n");

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    _handle = glfwCreateWindow(width, height, title.data(), nullptr, nullptr);

    if (_handle == nullptr)
        fmt::print("failed to create glfw window");

    glfwSetWindowUserPointer(_handle, this);
    glfwSetKeyCallback(_handle, key_callback);
    glfwSetCharCallback(_handle, char_callback);
    glfwSetMouseButtonCallback(_handle, mouse_click_callback);
    glfwSetCursorPosCallback(_handle, mouse_move_callback);
    glfwSetScrollCallback(_handle, scroll_callback);

    glfwSetCursorPos(_handle, 0.0f, 0.0f);
}

void demo::window::poll() {
    glfwPollEvents();
}

bool demo::window::should_close() const noexcept {
    return glfwWindowShouldClose(_handle);
}
std::string_view demo::window::title() const noexcept { return _title; }

glm::ivec2 demo::window::size() const noexcept { return _size; }

GLFWwindow* demo::window::handle() const noexcept { return _handle; }

void demo::window::key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    auto window_ptr = reinterpret_cast<class window*>(glfwGetWindowUserPointer(window));
    if (action == GLFW_RELEASE)
        window_ptr->key_states[key] = KeyState::released;
    else if (action == GLFW_REPEAT)
        window_ptr->key_states[key] = KeyState::repeat;
    else if (action == GLFW_PRESS)
        window_ptr->key_states[key] = KeyState::pressed;

//    ImGuiIO& io = ImGui::GetIO();
//    if (action == GLFW_PRESS) io.KeysDown[key] = true;
//    if (action == GLFW_RELEASE) io.KeysDown[key] = false;

    // Modifiers are not reliable across systems
//    io.KeyCtrl = io.KeysDown[GLFW_KEY_LEFT_CONTROL] || io.KeysDown[GLFW_KEY_RIGHT_CONTROL];
//    io.KeyShift = io.KeysDown[GLFW_KEY_LEFT_SHIFT] || io.KeysDown[GLFW_KEY_RIGHT_SHIFT];
//    io.KeyAlt = io.KeysDown[GLFW_KEY_LEFT_ALT] || io.KeysDown[GLFW_KEY_RIGHT_ALT];
//#ifdef _WIN32
//    io.KeySuper = false;
//#else
//    io.KeySuper = io.KeysDown[GLFW_KEY_LEFT_SUPER] || io.KeysDown[GLFW_KEY_RIGHT_SUPER];
//#endif
}
void demo::window::char_callback(GLFWwindow* window, uint32_t codepoint) {
//    auto window_ptr = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
//    ImGuiIO& io = ImGui::GetIO();
//    io.AddInputCharacter(codepoint);
}
void demo::window::mouse_click_callback(GLFWwindow* window, int button, int action, int mods) {
    // unused
}

void demo::window::mouse_move_callback(GLFWwindow* window, double x, double y) {}
void demo::window::scroll_callback(GLFWwindow* window, double x, double y) {}
bool demo::window::is_key_down(demo::window::KeyCode code) const noexcept {
    return key_states[static_cast<int>(code)] == KeyState::pressed;
}
bool demo::window::is_key_up(demo::window::KeyCode code) const noexcept {
    return key_states[static_cast<int>(code)] == KeyState::released;
}
bool demo::window::is_key_held(demo::window::KeyCode code) const noexcept {
    return key_states[static_cast<int>(code)] == KeyState::held ||
           key_states[static_cast<int>(code)] == KeyState::pressed;
}
