//
// Created by legend on 6/13/20.
//

#pragma once

#include <glm/glm.hpp>

struct Sphere
{
    explicit Sphere(glm::vec3 orig, float radiu, int mat_id)
    {
        // No these aren't typos, I just couldn't think of anything else to name them...
        origin = orig;
        radius = radiu;
        material_id = glm::vec4(mat_id, 0, 0, 0);
    }
    glm::vec3 origin;
    float radius;
    glm::vec4 material_id;
};

struct Triangle
{
    explicit Triangle(glm::vec3 vert0, glm::vec3 vert1, glm::vec3 vert2, int mat_id)
    {
        glm::vec3 normal = glm::normalize(glm::cross(vert1 - vert0, vert2 - vert0));
        vertex0 = glm::vec4(vert0, normal.x);
        vertex1 = glm::vec4(vert1, normal.y);
        vertex2 = glm::vec4(vert2, normal.z);
        material_id = glm::vec4(mat_id, 0, 0, 0);
    }
    glm::vec4 vertex0{};
    glm::vec4 vertex1{};
    glm::vec4 vertex2{};
    glm::vec4 material_id{};
};

struct GpuBvhNode
{
    float min_x;
    float min_y;
    float min_z;
    float max_x;
    float max_y;
    float max_z;
    unsigned int left_index = std::numeric_limits<unsigned int>::max();
    unsigned int right_index = std::numeric_limits<unsigned int>::max();
    unsigned int parent_index = std::numeric_limits<unsigned int>::max();
    unsigned int primitive_index = std::numeric_limits<unsigned int>::max();
    unsigned int primitive_count = std::numeric_limits<unsigned int>::max();
    bool visited = false;
};

struct AABB
{
    AABB() = default;
    AABB(glm::vec3 min, glm::vec3 max) : min(min), max(max) {}

    AABB(float min_x, float min_y, float min_z, float max_x, float max_y, float max_z) : min(min_x, min_y, min_z), max(max_x, max_y, max_z)
    {
    }
    glm::vec3 min {0, 0, 0};
    glm::vec3 max {0, 0, 0};

    [[nodiscard]]
    glm::vec3 center() const
    {
        return {
            ((max.x - min.x) / 2) + min.x,
            ((max.y - min.y) / 2) + min.y,
            ((max.z - min.z) / 2) + min.z};
    }

    void expand(const glm::vec3& expansion)
    {
        min.x = fminf(min.x, expansion.x);
        min.y = fminf(min.y, expansion.y);
        min.z = fminf(min.z, expansion.z);
        max.x = fmaxf(max.x, expansion.x);
        max.y = fmaxf(max.y, expansion.y);
        max.z = fmaxf(max.z, expansion.z);
    }
};
