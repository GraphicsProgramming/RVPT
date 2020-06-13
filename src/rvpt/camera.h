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
    ~Camera();

    void move(float x, float y, float z);
    void rotate(float x, float y);
    void set_fov(float fov);
    std::vector<glm::vec4> get_data();
    void update_imgui();

private:
    void recalculate_values();
    float fov{}, aspect{}, x_angle{}, y_angle{};
    glm::vec3 origin{}, center{}, horizontal{}, vertical{};
    glm::mat4 matrix{1.f};
};
