//
// Created by legend on 6/13/20.
//

#pragma once

#include <limits>

#include <glm/glm.hpp>

struct AABB
{
    AABB() = default;
    AABB(glm::vec3 vec) : AABB(vec, vec) {}
    AABB(glm::vec3 min, glm::vec3 max) : min(min), max(max) {}

    glm::vec3 min
    {
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max()
    };
    glm::vec3 max
    {
        -std::numeric_limits<float>::max(),
        -std::numeric_limits<float>::max(),
        -std::numeric_limits<float>::max()
    };

    [[nodiscard]] glm::vec3 center() const { return (max + min) * 0.5f; }
    [[nodiscard]] glm::vec3 diagonal() const { return max - min; }
    
    [[nodiscard]]
    float half_area() const {
        auto d = glm::max(diagonal(), glm::vec3(0));
        return d.x * d.y + d.x * d.z + d.y * d.z;
    }

    AABB& expand(const glm::vec3& vec)
    {
        min = glm::min(min, vec);
        max = glm::max(max, vec);
        return *this;
    }
    
    AABB& expand(const AABB& aabb)
    {
        min = glm::min(min, aabb.min);
        max = glm::max(max, aabb.max);
        return *this;
    }
};

struct Sphere
{
    Sphere() = default;

    explicit Sphere(glm::vec3 origin, float radius, int material_id)
        : origin(origin),
          radius(radius),
          material_id{ material_id, 0, 0, 0 }
    {}

    glm::vec3 origin {};
    float radius {};
    glm::vec4 material_id {};

    AABB aabb() const { return AABB(origin - glm::vec3(radius), origin + glm::vec3(radius)); }
    glm::vec3 center() const { return origin; }
};

struct Triangle
{
    Triangle() = default;

    explicit Triangle(glm::vec3 vertex0, glm::vec3 vertex1, glm::vec3 vertex2, int material_id)
        : vertex0{ glm::vec3(vertex0), 0 },
          vertex1{ glm::vec3(vertex1), 0 },
          vertex2{ glm::vec3(vertex2), 0 },
          material_id{ material_id, 0, 0, 0 }
    {
        auto normal = glm::normalize(glm::cross(vertex1 - vertex0, vertex2 - vertex0));
        this->vertex0.w = normal.x;
        this->vertex1.w = normal.y;
        this->vertex2.w = normal.z;
    }

    glm::vec4 vertex0 {};
    glm::vec4 vertex1 {};
    glm::vec4 vertex2 {};
    glm::vec4 material_id {};

    AABB aabb() const
    {
        return AABB(glm::vec3(vertex0))
            .expand(glm::vec3(vertex1))
            .expand(glm::vec3(vertex2));
    }

    glm::vec3 center() const {
        return
            (glm::vec3(vertex0) +
             glm::vec3(vertex1) +
             glm::vec3(vertex2)) * (1.0f / 3.0f);
    }
};
