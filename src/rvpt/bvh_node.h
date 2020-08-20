//
// Created by legend on 20/8/2020.
//

#pragma once

#include <vector>

#include "geometry.h"

struct BvhNode
{
    std::vector<Triangle> triangles;
    AABB bounds;
    BvhNode* left = nullptr;
    BvhNode* right = nullptr;

    void split();
    void expand(const glm::vec3& expansion);
};
