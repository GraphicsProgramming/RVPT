//
// Created by legend on 20/8/2020.
//

#include "bvh.h"

void Bvh::collect_aabbs_by_depth(
    std::vector<std::vector<AABB>>& aabbs,
    size_t depth, size_t node_index) const
{
    const BvhNode& node = nodes[node_index];
    if (depth >= aabbs.size())
        aabbs.resize(depth + 1);
    aabbs[depth].push_back(node.aabb());
    if (!node.is_leaf())
    {
        collect_aabbs_by_depth(aabbs, depth + 1, node.first_child_or_primitive + 0);
        collect_aabbs_by_depth(aabbs, depth + 1, node.first_child_or_primitive + 1);
    }
}
