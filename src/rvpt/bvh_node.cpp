//
// Created by legend on 20/8/2020.
//

#include "bvh_node.h"

void BvhNode::split()
{

}

void BvhNode::expand(const glm::vec3 &expansion)
{
    // Simd maybe?
    bounds.min.x = fminf(bounds.min.x, expansion.x);
    bounds.min.y = fminf(bounds.min.y, expansion.y);
    bounds.min.z = fminf(bounds.min.z, expansion.z);
    bounds.max.x = fmaxf(bounds.max.x, expansion.x);
    bounds.max.y = fmaxf(bounds.max.y, expansion.y);
    bounds.max.z = fmaxf(bounds.max.z, expansion.z);
}
