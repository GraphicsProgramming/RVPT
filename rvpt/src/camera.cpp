//
// Created by legend on 5/26/20.
//
#include "rvpt/camera.h"

#define GLM_DEPTH_ZERO_TO_ONE
#include <glm/ext.hpp>
#include <glm/gtx/euler_angles.hpp>

constexpr glm::vec3 RIGHT = glm::vec3(1, 0, 0);
constexpr glm::vec3 UP = glm::vec3(0, 1, 0);
constexpr glm::vec3 FORWARD = glm::vec3(0, 0, 1);

glm::mat4 construct_camera_matrix(glm::vec3 translation, glm::vec3 rotation)
{
    auto mat = glm::mat4{1.f};
    mat = glm::translate(mat, translation);
    mat = glm::rotate(mat, glm::radians(rotation.x), UP);
    mat = glm::rotate(mat, glm::radians(rotation.y), RIGHT);
    mat = glm::rotate(mat, glm::radians(rotation.z), FORWARD);
    return mat;
}

Camera::Camera(float aspect) : aspect(aspect) { _update_values(); }

void Camera::translate(const glm::vec3 &in_translation)
{
    const auto translation_mat = construct_camera_matrix(translation, rotation);
    translation += glm::vec3(translation_mat * glm::vec4(in_translation, 0));
}

void Camera::rotate(const glm::vec3 &in_rotation)
{
    rotation += in_rotation;
    if (vertical_view_angle_clamp) rotation.y = glm::clamp(rotation.y, -90.f, 90.f);
}

void Camera::set_fov(float in_fov) { fov = in_fov; }

void Camera::set_scale(float in_scale) { scale = in_scale; }

void Camera::set_camera_mode(int in_mode) { mode = in_mode; }

void Camera::clamp_vertical_view_angle(bool clamp) { vertical_view_angle_clamp = clamp; }

float Camera::get_fov() const noexcept { return fov; }

float Camera::get_scale() const noexcept { return scale; }

int Camera::get_camera_mode() const noexcept { return mode; }

std::vector<glm::vec4> Camera::get_data()
{
    _update_values();
    std::vector<glm::vec4> data;
    data.emplace_back(camera_matrix[0]);
    data.emplace_back(camera_matrix[1]);
    data.emplace_back(camera_matrix[2]);
    data.emplace_back(camera_matrix[3]);
    data.emplace_back(aspect, glm::radians(fov), scale, 0);

    return data;
}

glm::mat4 Camera::get_camera_matrix()
{
    _update_values();
    return camera_matrix;
}

glm::mat4 Camera::get_view_matrix()
{
    _update_values();
    return view_matrix;
}

glm::mat4 Camera::get_pv_matrix()
{
    _update_values();
    return pv_matrix;
}

void Camera::_update_values()
{
    const auto theta = glm::radians(fov);
    const auto view_height = tanf(theta / 2);
    const auto view_width = aspect * view_height;

    camera_matrix = construct_camera_matrix(translation, rotation);

    view_matrix = glm::inverse(camera_matrix);

    switch (mode)
    {
        case 0:
            pv_matrix =
                glm::perspectiveLH_ZO(glm::radians(fov), aspect, 0.1f, 1000.f) * view_matrix;
            break;

        case 1:
            pv_matrix = glm::orthoLH_ZO(-view_width * scale, view_width * scale,
                                        -view_height * scale, view_height * scale, 0.1f, 1000.f) *
                        view_matrix;
            break;
    }
}
