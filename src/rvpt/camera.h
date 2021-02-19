//
// Created by legend on 5/26/20.
//

#pragma once

#include <vector>
#include <glm/glm.hpp>

static const char* CameraModes[] = {"perspective", "orthographic", "spherical"};

class Camera
{
public:
    explicit Camera(float aspect);

    void translate(const glm::vec3 &in_translation);
    void rotate(const glm::vec3 &in_rotation);

    void set_fov(float fov);

    void set_scale(float scale);

    void set_camera_mode(int mode);

    // Prevent looking beyond +/- 90 degrees vertically
    void clamp_vertical_view_angle(bool clamp);

    void update_imgui();

    [[nodiscard]] float get_fov() const noexcept;

    [[nodiscard]] float get_scale() const noexcept;

    [[nodiscard]] int get_camera_mode() const noexcept;

    [[nodiscard]] std::vector<glm::vec4> get_data();
    [[nodiscard]] glm::mat4 get_camera_matrix();
    [[nodiscard]] glm::mat4 get_view_matrix();
    [[nodiscard]] glm::mat4 get_pv_matrix();

private:
    void _update_values();
    int mode = 0;
    float fov = 90.f, scale = 4.f, aspect{};
    bool vertical_view_angle_clamp = false;

    glm::vec3 translation{};
    glm::vec3 rotation{};

    glm::mat4 camera_matrix{1.f};

    /* view = inverse(camera_matrix) */
    glm::mat4 view_matrix{1.f};
    /* projection * view matrix */
    glm::mat4 pv_matrix{1.f};
};
