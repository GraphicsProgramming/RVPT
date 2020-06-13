//
// Created by legend on 5/26/20.
//
#include "camera.h"

#include <iostream>

#include <glm/ext.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <imgui.h>

Camera::Camera(float aspect) : fov(90), aspect(aspect) { recalculate_values(); }

Camera::~Camera() = default;

void Camera::move(float x, float y, float z)
{
    matrix = glm::translate(matrix, glm::vec3(x, y, z));
}

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

void Camera::update_imgui()
{
    static bool is_active = true;
    ImGui::SetNextWindowPos({0, 160});
    ImGui::SetNextWindowSize({200, 180});

    if (ImGui::Begin("Camera Data", &is_active))
    {
        ImGui::SliderFloat("fov", &fov, 1, 179);

        ImGui::DragFloat4("mat4[0]", glm::value_ptr(matrix[0]), 0.05f);
        ImGui::DragFloat4("mat4[1]", glm::value_ptr(matrix[1]), 0.05f);
        ImGui::DragFloat4("mat4[2]", glm::value_ptr(matrix[2]), 0.05f);
        ImGui::DragFloat4("mat4[3]", glm::value_ptr(matrix[3]), 0.05f);

        ImGui::Text("Reset");
        ImGui::SameLine();
        if (ImGui::Button("Pos"))
        {
            matrix = glm::mat4{1.f};
        }
        ImGui::SameLine();
        if (ImGui::Button("Scale"))
        {
            matrix[0][0] = 1.f;
            matrix[1][1] = 1.f;
            matrix[2][2] = 1.f;
        }
        ImGui::SameLine();
        if (ImGui::Button("Rot"))
        {
            matrix[0] = glm::vec4(matrix[0][0], 0.f, 0.f, 0.f);
            matrix[1] = glm::vec4(0.f, matrix[1][1], 0.f, 0.f);
            matrix[2] = glm::vec4(0.f, 0.f, matrix[2][2], 0.f);
        }
    }
    ImGui::End();
    recalculate_values();
}
