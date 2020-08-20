//
// Created by legend on 20/8/2020.
//

#pragma once

#include <vector>
#include <unordered_map>
#include <cmath>
#include <glm/gtx/component_wise.hpp>

#include "geometry.h"

struct BvhNode
{
    std::vector<Triangle> triangles;
    AABB bounds;
    BvhNode* left = nullptr;
    BvhNode* right = nullptr;
    ~BvhNode();

    void expand(const glm::vec3& expansion);
    bool is_within(const glm::vec3& point);
    bool is_leaf() const;
};
