//
// Created by legend on 5/26/20.
//

#pragma once

#include <vector>
#include <glm/glm.hpp>

class Camera
{
public:
    explicit Camera(float aspect);

    void move(glm::vec3 translation);
    void rotate(glm::vec3 rotation);
    void set_fov(float fov);
    std::vector<glm::vec4> get_data();
    glm::mat4 get_camera_matrix();
    glm::mat4 get_view_matrix();
    glm::mat4 get_pv_matrix();
    void update_imgui();

private:
    void recalculate_values();
    float fov{}, aspect{};

    glm::vec3 translation{};

    glm::vec3 rotation{};
    glm::vec2 displacement{};

    glm::mat4 camera_matrix{1.f};

    /* view = inverse(camera_matrix) */
    glm::mat4 view_matrix{1.f};
    /* projection * view matrix */
    glm::mat4 pv_matrix{1.f};
};
