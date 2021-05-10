//
// Created by legend on 21/8/2020.
//

#pragma once

#include <vector>
#include <limits>
#include <tuple>

#include "bvh.h"
#include "geometry.h"

class BvhBuilder
{
public:
    template <typename Primitive>
    Bvh build_bvh(const std::vector<Primitive>& primitives)
    {
        std::vector<AABB> bounding_boxes(primitives.size());
        std::vector<glm::vec3> primitive_centers(primitives.size());
        for (size_t i = 0, n = primitives.size(); i < n; ++i)
        {
            bounding_boxes[i] = primitives[i].aabb();
            primitive_centers[i] = primitives[i].center();
        }
        return build_bvh(primitive_centers, bounding_boxes);
    }

    virtual Bvh build_bvh(
        const std::vector<glm::vec3>& primitive_centers,
        const std::vector<AABB>& bounding_boxes) = 0;
};

class BinnedBvhBuilder : public BvhBuilder
{
public:
    using BvhBuilder::build_bvh;

    Bvh build_bvh(
        const std::vector<glm::vec3>& primitive_centers,
        const std::vector<AABB>& bounding_boxes) override;

private:
    // Threshold below which nodes are no longer split
    static constexpr size_t min_primitives_per_leaf = 4;
    // Threshold above which nodes that are not split with binning are still split with the fallback strategy.
    static constexpr size_t max_primitives_per_leaf = 8;
    // Number of bins used to approximate the SAH. Higher = More accuracy, but slower.
    static constexpr size_t bin_count = 16;

    static_assert(min_primitives_per_leaf < max_primitives_per_leaf);

    struct Bin
    {
        AABB aabb;
        size_t primitive_count = 0;
        float cost;
    };

    void build_bvh_node(
        Bvh& bvh, BvhNode& node_to_build,
        const std::vector<glm::vec3>& primitive_centers,
        const std::vector<AABB>& bounding_boxes);

    [[nodiscard]] static size_t compute_bin_index(
        int axis, const glm::vec3& center, const AABB& centers_aabb) noexcept;

    struct BestSplit
    {
        float min_cost;
        int min_axis;
        size_t min_bin;
    };
    [[nodiscard]] BestSplit find_best_split(
        size_t begin, size_t end,
        const AABB& node_aabb,
        const std::vector<uint32_t>& primitive_indices,
        const std::vector<glm::vec3>& primitive_centers,
        const std::vector<AABB>& bounding_boxes) const noexcept;
};
