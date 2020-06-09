//
// Created by legend on 5/26/20.
//

#include <glm/ext.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <iostream>
#include "camera.h"

Camera::Camera(float aspect) : fov(90), aspect(aspect)
{
    recalculate_values();
    matrix = glm::mat4();
}

Camera::~Camera() = default;

void Camera::move(float x, float y, float z) { glm::translate(matrix, glm::vec3(x, y, z)); }

void Camera::rotate(float x, float y)
{
    matrix /= glm::eulerAngleYXZ(y_angle, x_angle, 0.f);
    x_angle += x;
    y_angle += y;
    matrix *= glm::eulerAngleYXZ(y_angle, x_angle, 0.f);
}

void Camera::set_fov(float in_fov) { fov = in_fov; }

void Camera::recalculate_values()
{
    float theta = fov * 3.1415f / 180;
    float half_height = tanf(theta / 2);
    float half_width = aspect * half_height;
    glm::vec3 w = glm::vec3(0, 0, -1);
    glm::vec3 u = glm::normalize(glm::cross(glm::vec3(0, 1, 0), w));
    glm::vec3 v = glm::cross(w, u);
    center = origin + w;
    horizontal = u * half_width;
    vertical = v * half_height;
}

std::vector<glm::vec4> Camera::get_data()
{
    std::vector<glm::vec4> data;
    data.emplace_back(origin, 0);
    data.emplace_back(center, 0);
    data.emplace_back(horizontal, 0);
    data.emplace_back(vertical, 0);
    data.emplace_back(matrix[0]);
    data.emplace_back(matrix[1]);
    data.emplace_back(matrix[2]);
    data.emplace_back(matrix[3]);

    return data;
}