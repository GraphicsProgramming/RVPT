#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <array>

namespace demo
{

class window
{
public:
    enum class KeyCode
    {
        SPACE = 32,  // I can't wait to write all of these out
        APOSTROPHE = 39,
        COMMA = 44,
        MINUS = 45,
        PERIOD = 46,
        SLASH = 47,
        KEY_0 = 48,
        KEY_1 = 49,
        KEY_2 = 50,
        KEY_3 = 51,
        KEY_4 = 52,
        KEY_5 = 53,
        KEY_6 = 54,
        KEY_7 = 55,
        KEY_8 = 56,
        KEY_9 = 57,  // This is starting to get really annoying
        SEMICOLON = 59,
        EQUAL = 61,
        KEY_A = 65,
        KEY_B = 66,
        KEY_C = 67,
        KEY_D = 68,
        KEY_E = 69,
        KEY_F = 70,
        KEY_G = 71,
        KEY_H = 72,
        KEY_I = 73,
        KEY_J = 74,
        KEY_K = 75,
        KEY_L = 76,
        KEY_M = 77,
        KEY_N = 78,
        KEY_O = 79,
        KEY_P = 80,
        KEY_Q = 81,
        KEY_R = 82,
        KEY_S = 83,
        KEY_T = 84,
        KEY_U = 85,
        KEY_V = 86,
        KEY_W = 87,
        KEY_X = 88,
        KEY_Y = 89,
        KEY_Z = 90,
        KEY_LEFT_BRACKET = 91,  /* [ */
        KEY_BACKSLASH = 92,     /* \ */
        KEY_RIGHT_BRACKET = 93, /* ] */
        KEY_GRAVE_ACCENT = 96,  /* ` */
        KEY_WORLD_1 = 161,      /* non-US #1 */
        KEY_WORLD_2 = 162,      /* non-US #2 */
        KEY_ESCAPE = 256,
        KEY_ENTER = 257,
        KEY_TAB = 258,
        KEY_BACKSPACE = 259,
        KEY_INSERT = 260,
        KEY_DELETE = 261,
        KEY_RIGHT = 262,
        KEY_LEFT = 263,
        KEY_DOWN = 264,
        KEY_UP = 265,
        KEY_PAGE_UP = 266,
        KEY_PAGE_DOWN = 267,
        KEY_HOME = 268,
        KEY_END = 269,
        KEY_CAPS_LOCK = 280,
        KEY_SCROLL_LOCK = 281,
        KEY_NUM_LOCK = 282,
        KEY_PRINT_SCREEN = 283,
        KEY_PAUSE = 284,
        KEY_F1 = 290,
        KEY_F2 = 291,
        KEY_F3 = 292,
        KEY_F4 = 293,
        KEY_F5 = 294,
        KEY_F6 = 295,
        KEY_F7 = 296,
        KEY_F8 = 297,
        KEY_F9 = 298,
        KEY_F10 = 299,
        KEY_F11 = 300,
        KEY_F12 = 301,
        KEY_F13 = 302,
        KEY_F14 = 303,
        KEY_F15 = 304,
        KEY_F16 = 305,
        KEY_F17 = 306,
        KEY_F18 = 307,
        KEY_F19 = 308,
        KEY_F20 = 309,
        KEY_F21 = 310,
        KEY_F22 = 311,
        KEY_F23 = 312,
        KEY_F24 = 313,
        KEY_F25 = 314,
        KEY_KP_0 = 320,
        KEY_KP_1 = 321,
        KEY_KP_2 = 322,
        KEY_KP_3 = 323,
        KEY_KP_4 = 324,
        KEY_KP_5 = 325,
        KEY_KP_6 = 326,
        KEY_KP_7 = 327,
        KEY_KP_8 = 328,
        KEY_KP_9 = 329,
        KEY_KP_DECIMAL = 330,
        KEY_KP_DIVIDE = 331,
        KEY_KP_MULTIPLY = 332,
        KEY_KP_SUBTRACT = 333,
        KEY_KP_ADD = 334,
        KEY_KP_ENTER = 335,
        KEY_KP_EQUAL = 336,
        KEY_LEFT_SHIFT = 340,
        KEY_LEFT_CONTROL = 341,
        KEY_LEFT_ALT = 342,
        KEY_LEFT_SUPER = 343,
        KEY_RIGHT_SHIFT = 344,
        KEY_RIGHT_CONTROL = 345,
        KEY_RIGHT_ALT = 346,
        KEY_RIGHT_SUPER = 347,
        KEY_MENU = 348,
        MAX_KEY = 1024,
    };

    enum class Action
    {
        RELEASE,
        PRESS,
        REPEAT,
        UNKNOWN
    };

    enum class Mouse
    {
        LEFT,
        RIGHT,
        MIDDLE,
        OTHER
    };

private:
    GLFWwindow *_handle;
    std::string _title;
    glm::ivec2 _size;

    enum class KeyState
    {
        none,     // no input
        pressed,  // set on frame key is pressed
        held,     // set on frame after key is pressed while not released
        repeat,   // equivalent to held but OS triggered
        released  // set on frame key is released, cleared after one frame
    };

    std::array<KeyState, static_cast<size_t>(KeyCode::MAX_KEY)> key_states;

    static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void char_callback(GLFWwindow* window, uint32_t codepoint);
    static void mouse_click_callback(GLFWwindow* window, int button, int action, int mods);
    static void mouse_move_callback(GLFWwindow* window, double x, double y);
    static void scroll_callback(GLFWwindow* window, double x, double y);
public:
    window(std::string_view title, uint32_t width, uint32_t height);

    void poll();

    [[nodiscard]] bool should_close() const noexcept;

    [[nodiscard]] std::string_view title() const noexcept;

    [[nodiscard]] glm::ivec2 size() const noexcept;

    [[nodiscard]] GLFWwindow *handle() const noexcept;

    [[nodiscard]] bool is_key_down(KeyCode code) const noexcept;

    [[nodiscard]] bool is_key_up(KeyCode code) const noexcept;

    [[nodiscard]] bool is_key_held(KeyCode code) const noexcept;
};

}
