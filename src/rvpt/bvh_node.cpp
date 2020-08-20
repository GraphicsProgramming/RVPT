//
// Created by legend on 20/8/2020.
//

#include "bvh_node.h"

BvhNode::~BvhNode()
{
    free(left);
    free(right);
}

void BvhNode::expand(const glm::vec3 &expansion)
{
    bounds.expand(expansion);
}

bool BvhNode::is_within(const glm::vec3& point)
{
    return
        bounds.min.x < point.x && point.x < bounds.max.x &&
        bounds.min.y < point.y && point.y < bounds.max.y &&
        bounds.min.z < point.z && point.z < bounds.max.z;
}

bool BvhNode::is_leaf() const
{
    return left == nullptr && right == nullptr;
}
