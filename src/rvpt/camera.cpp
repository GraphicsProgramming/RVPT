//
// Created by legend on 5/26/20.
//

#include <glm/glm/ext.hpp>
#include <iostream>
#include "camera.h"

camera::camera(float aspect) : fov(90), aspect(aspect)
{
    recalculate_values();
    matrix = glm::translate(glm::vec3(0, 0, 0));
}

camera::~camera() = default;

void camera::move(float x, float y, float z)
{
    matrix *= glm::translate(glm::vec3(x, y, z));
}

void camera::rotate(float amount, glm::vec3 axis)
{
    matrix = glm::rotate(matrix, amount, axis);
}

void camera::set_fov(float in_fov)
{
    fov = in_fov;
}

void camera::recalculate_values()
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

std::vector<glm::vec4> camera::get_data()
{
    std::vector<glm::vec4> data;
    data.emplace_back(origin, 1);
    data.emplace_back(center, 0);
    data.emplace_back(horizontal, 0);
    data.emplace_back(vertical, 0);
    for(auto& vec4 : data)
        vec4 = matrix * vec4;
    return data;
}
