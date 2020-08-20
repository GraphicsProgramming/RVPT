//
// Created by legend on 20/8/2020.
//

#pragma once

#include <vector>
#include <unordered_map>

#include "geometry.h"

struct BvhNode
{
    std::vector<Triangle> triangles;
    AABB bounds;
    BvhNode* left = nullptr;
    BvhNode* right = nullptr;

    ~BvhNode();

    void split(std::vector<std::vector<AABB>>& bounds, int depth, int max_depth);
    void expand(const glm::vec3& expansion);
};
