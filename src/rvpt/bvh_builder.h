//
// Created by legend on 21/8/2020.
//

#pragma once

#include <vector>
#include <limits>

#include "bvh_node.h"
#include "geometry.h"

class BvhBuilder
{
public:
    enum Type
    {
        BottomTop
    };

    BvhBuilder(Type type);

    BvhBuilder::Type bvh_build_type;

    BvhNode* build_global_bvh(std::vector<AABB>& bounding_boxes,
                                          const std::vector<Triangle>& triangle_primitives);
};
