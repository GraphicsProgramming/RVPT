//
// Created by legend on 20/8/2020.
//

#include "bvh.h"

void Bvh::collect_aabbs_by_depth(
    std::vector<std::vector<AABB>>& aabbs,
    size_t depth, size_t node_index) const
{
    // Get the current node at the depth we're exploring
    const auto &node = nodes[node_index];

    // Reserve the stored AABB's (Helps save memory allocation cost)
    if (depth >= aabbs.size()) aabbs.resize(depth + 1);

    // Add the node to the depth
    aabbs[depth].push_back(node.aabb());

    // If we're not at a leaf, recursively go through children and add depths
    if (!node.is_leaf())
    {
        collect_aabbs_by_depth(aabbs, depth + 1, node.first_child_or_primitive + 0);
        collect_aabbs_by_depth(aabbs, depth + 1, node.first_child_or_primitive + 1);
    }
}
