//
// Created by legend on 21/8/2020.
//

#include "bvh_builder.h"

#include <numeric>
#include <algorithm>
#include <cassert>

Bvh BinnedBvhBuilder::build_bvh(const std::vector<glm::vec3>& primitive_centers,
                                const std::vector<AABB>& bounding_boxes)
{
    assert(primitive_centers.size() == bounding_boxes.size());
    size_t primitive_count = primitive_centers.size();
    assert(primitive_count != 0);

    // Allocate enough nodes in a big, flat array. For this, we use the property:
    //
    //   number of nodes = 2 * number of leaves - 1
    //
    // This property is valid for every binary tree and easy to verify by induction.
    // Since a BVH has at most as many leaves as there are primitives (one primitive
    // only goes into one leaf), then we have an upper bound on the number of nodes.
    Bvh bvh;
    bvh.nodes.reserve(2 * primitive_count - 1);

    // Primitive indices are just a big array of indices into the primitive data.
    // Initially, that array is just 0, 1, 2, ...
    bvh.primitive_indices.resize(primitive_count);
    std::iota(bvh.primitive_indices.begin(), bvh.primitive_indices.end(), 0);

    // Initially, we set the root node to be a leaf that spans the entire list of primitives
    bvh.nodes.emplace_back(BvhNode{.first_child_or_primitive = 0,
                                   .primitive_count = static_cast<uint32_t>(primitive_count)});
    build_bvh_node(bvh, bvh.nodes.front(), primitive_centers, bounding_boxes);
    bvh.nodes.shrink_to_fit();
    return bvh;
}

size_t BinnedBvhBuilder::compute_bin_index(int axis, const glm::vec3& center,
                                           const AABB& aabb) noexcept
{
    int index =
        (center[axis] - aabb.min[axis]) * (static_cast<float>(bin_count) / aabb.diagonal()[axis]);
    return std::min(int{bin_count - 1}, std::max(0, index));
}

BinnedBvhBuilder::BestSplit BinnedBvhBuilder::find_best_split(
    size_t begin, size_t end, const AABB& node_aabb, const std::vector<uint32_t>& primitive_indices,
    const std::vector<glm::vec3>& primitive_centers,
    const std::vector<AABB>& bounding_boxes) const noexcept
{
    float min_cost = std::numeric_limits<float>::max();
    size_t min_bin = 0;
    int min_axis = -1;
    for (int axis = 0; axis < 3; ++axis)
    {
        Bin bins[bin_count];

        // Fill bins with primitives
        for (size_t i = begin; i < end; ++i)
        {
            const glm::vec3& primitive_center = primitive_centers[primitive_indices[i]];
            Bin& bin = bins[compute_bin_index(axis, primitive_center, node_aabb)];
            bin.primitive_count++;
            bin.aabb.expand(bounding_boxes[primitive_indices[i]]);
        }

        // Sweep from the left to the right in order to compute the partial cost:
        //
        //    N(Li) * SA(Li)
        //
        // where N(x) is the number of primitives in node x, and SA(x) is its surface
        // area. Li refers to the left node if we split at the bin index with index i.
        AABB left_aabb;
        size_t left_count = 0;
        for (size_t i = 0; i < bin_count; ++i)
        {
            left_aabb.expand(bins[i].aabb);
            left_count += bins[i].primitive_count;
            bins[i].cost = left_aabb.half_area() * left_count;
        }

        // Sweep from the right to the left in order to compute the full SAH cost:
        //
        // N(Ri) * SA(Ri) + N(Li) * SA(Li)
        //
        // Note that N(Li) * SA(Li) has already been computed in the step above.
        // By convention, we use the index of the first bin on the right to denote a split.
        // This means that we only need to go through bins with index 1..bin_count - 1 (0 is
        // not a valid split since there would be nothing in the left child).
        AABB right_aabb;
        size_t right_count = 0;
        for (size_t i = bin_count - 1; i > 0; --i)
        {
            right_aabb.expand(bins[i].aabb);
            right_count += bins[i].primitive_count;
            float cost = right_aabb.half_area() * right_count + bins[i - 1].cost;
            if (cost < min_cost)
            {
                min_cost = cost;
                min_axis = axis;
                min_bin = i;
            }
        }
    }

    assert(min_axis != -1);
    auto best = BestSplit();
    best.min_cost = min_cost;
    best.min_axis = min_axis;
    best.min_bin = min_bin;
    return best;
}

void BinnedBvhBuilder::build_bvh_node(Bvh& bvh, BvhNode& node_to_build,
                                      const std::vector<glm::vec3>& primitive_centers,
                                      const std::vector<AABB>& bounding_boxes)
{
    assert(node_to_build.is_leaf());
    const size_t primitives_begin = node_to_build.first_child_or_primitive;
    const size_t primitives_end = primitives_begin + node_to_build.primitive_count;

    // Compute the bounding box of this node
    node_to_build.aabb() = AABB();
    for (size_t i = primitives_begin; i < primitives_end; ++i)
        node_to_build.aabb().expand(bounding_boxes[bvh.primitive_indices[i]]);

    // If the node has too few primitives, keep it a leaf
    if (node_to_build.primitive_count < min_primitives_per_leaf) return;

    auto [min_cost, min_axis, min_bin] =
        find_best_split(primitives_begin, primitives_end, node_to_build.aabb(),
                        bvh.primitive_indices, primitive_centers, bounding_boxes);

    float no_split_cost = node_to_build.aabb().half_area() * node_to_build.primitive_count;
    size_t right_partition_begin = 0;
    if (min_cost >= no_split_cost)
    {
        // If the split returned by binning is not better than not splitting, we have 2
        // possibilities:
        // - The number of primitives is low, so having a leaf here is fine,
        // - The number of primitives is too high and we need a fallback strategy.
        if (node_to_build.primitive_count <= max_primitives_per_leaf) return;

        // The fallback strategy here is just to sort primitives along the split axis and pick the
        // median. This ensures that, even if this split is not useful, we have a chance of making
        // good splits in the two children.
        std::sort(bvh.primitive_indices.begin() + primitives_begin,
                  bvh.primitive_indices.begin() + primitives_end,
                  [&primitive_centers, min_axis = min_axis](size_t i, size_t j) {
                      return primitive_centers[i][min_axis] < primitive_centers[j][min_axis];
                  });
        right_partition_begin = primitives_begin + node_to_build.primitive_count / 2;
    }
    else
    {
        // This split is good, we just need to partition the primitives accordingly
        right_partition_begin =
            std::partition(bvh.primitive_indices.begin() + primitives_begin,
                           bvh.primitive_indices.begin() + primitives_end,
                           [&primitive_centers, &node_to_build, min_axis = min_axis,
                            min_bin = min_bin](size_t i) {
                               size_t bin_index = compute_bin_index(min_axis, primitive_centers[i],
                                                                    node_to_build.aabb());
                               return bin_index < min_bin;
                           }) -
            bvh.primitive_indices.begin();
    }
    assert(right_partition_begin > primitives_begin && right_partition_begin < primitives_end);

    // Allocate children nodes and recurse
    size_t first_child_index = bvh.nodes.size();
    auto& left_child = bvh.nodes.emplace_back();
    auto& right_child = bvh.nodes.emplace_back();
    node_to_build.primitive_count = 0;
    node_to_build.first_child_or_primitive = first_child_index;

    left_child.primitive_count = right_partition_begin - primitives_begin;
    left_child.first_child_or_primitive = primitives_begin;
    right_child.primitive_count = primitives_end - right_partition_begin;
    right_child.first_child_or_primitive = right_partition_begin;

    build_bvh_node(bvh, left_child, primitive_centers, bounding_boxes);
    build_bvh_node(bvh, right_child, primitive_centers, bounding_boxes);
}
