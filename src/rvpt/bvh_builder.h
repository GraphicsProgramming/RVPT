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
    enum BvhType
    {
        BottomTop
    };

    BvhBuilder(BvhType type);

    BvhBuilder::BvhType bvh_build_type;

    BvhNode* build_global_bvh(std::vector<AABB>& bounding_boxes_no_depth, std::vector<std::vector<AABB>>& bounding_boxes_with_depth,
                                          const std::vector<Triangle>& triangle_primitives);
};
