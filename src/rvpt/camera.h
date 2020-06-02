//
// Created by legend on 5/26/20.
//

#pragma once

#include <glm/glm.hpp>
#include <vector>

class Camera
{
public:
    explicit Camera(float aspect);
    ~Camera();

    void move(float x, float y, float z);
    void rotate(float x, float y);
    void set_fov(float fov);
    std::vector<glm::vec4> get_data();

private:
    void recalculate_values();
    float fov, aspect, x_angle{}, y_angle{};
    glm::vec3 origin, center, horizontal, vertical;
    glm::mat4 matrix;
};