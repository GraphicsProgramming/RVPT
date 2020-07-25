//
// Created by legend on 5/26/20.
//
#include "camera.h"

#include <iostream>

#define GLM_DEPTH_ZERO_TO_ONE
#include <glm/ext.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <imgui.h>

constexpr glm::vec3 RIGHT = glm::vec3(1, 0, 0);
constexpr glm::vec3 UP = glm::vec3(0, 1, 0);

glm::mat4 get_view_matrix(glm::vec3 translation, glm::vec3 rotation)
{
    auto mat = glm::mat4{1.f};
    mat = glm::translate(mat, translation);
    mat = glm::rotate(mat, glm::radians(rotation.x), -UP);
    mat = glm::rotate(mat, glm::radians(rotation.y), -RIGHT);
    return mat;
}

Camera::Camera(float aspect) : fov(90), aspect(aspect) { recalculate_values(); }

void Camera::move(glm::vec3 translation)
{
    translation.y *= -1.f;
    translation.z *= -1.f;
    auto translation_mat = get_view_matrix(this->translation, this->rotation);
    this->translation += glm::vec3(translation_mat * glm::vec4(translation, 0));
}

void Camera::rotate(glm::vec3 rotation) { this->rotation += rotation; }
void Camera::set_fov(float in_fov) { fov = in_fov; }

void Camera::recalculate_values()
{
    float theta = glm::radians(fov);
    float view_height = tanf(theta / 2);
    float view_width = aspect * view_height;
    displacement = glm::vec2(view_width, view_height);

    ray_matrix = get_view_matrix(this->translation, this->rotation);

    debug_matrix =
        glm::perspective(glm::radians(fov), aspect, 0.1f, 1000.f) * glm::inverse(ray_matrix);
}

std::vector<glm::vec4> Camera::get_data()
{
    recalculate_values();
    std::vector<glm::vec4> data;
    data.emplace_back(displacement, 0, 0);
    data.emplace_back(ray_matrix[0]);
    data.emplace_back(ray_matrix[1]);
    data.emplace_back(ray_matrix[2]);
    data.emplace_back(ray_matrix[3]);

    return data;
}

glm::mat4 Camera::get_ray_matrix()
{
    recalculate_values();
    return ray_matrix;
}
glm::mat4 Camera::get_debug_matrix()
{
    recalculate_values();
    return debug_matrix;
}

void Camera::update_imgui()
{
    static bool is_active = true;
    ImGui::SetNextWindowPos({0, 160});
    ImGui::SetNextWindowSize({200, 130}, ImGuiCond_Once);

    if (ImGui::Begin("Camera Data", &is_active))
    {
        ImGui::SliderFloat("fov", &fov, 1, 179);
        ImGui::DragFloat3("translation", glm::value_ptr(translation), 0.2f);
        ImGui::DragFloat3("rotation", glm::value_ptr(rotation), 0.2f);
        ImGui::Text("Reset");
        ImGui::SameLine();
        if (ImGui::Button("Pos"))
        {
            translation = {};
        }
        ImGui::SameLine();
        if (ImGui::Button("Rot"))
        {
            rotation = {};
        }
    }
    ImGui::End();
    recalculate_values();
}
