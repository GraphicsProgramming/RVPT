//
// Created by legend on 20/8/2020.
//

#pragma once

#include <cstdint>
#include <vector>

#include "geometry.h"

struct BvhNode
{
    // We only need one index to the first BVH node child.
    // The other child is located at index `first_child_or_primitive + 1`.
    uint32_t first_child_or_primitive;
    uint32_t primitive_count;
    float bounds[6];

    // Helper to transparently set the AABB of a node
    struct AABBProxy {
        BvhNode& node;

        explicit AABBProxy(BvhNode& node)
            : node(node)
        {}

        [[nodiscard]]
        AABB aabb() const noexcept
        {
            return AABB(
                glm::vec3(node.bounds[0], node.bounds[2], node.bounds[4]),
                glm::vec3(node.bounds[1], node.bounds[3], node.bounds[5]));
        }

        operator AABB () const noexcept { return aabb(); }

        AABBProxy& operator = (const AABB& aabb)
        {
            node.bounds[0] = aabb.min.x;
            node.bounds[1] = aabb.max.x;
            node.bounds[2] = aabb.min.y;
            node.bounds[3] = aabb.max.y;
            node.bounds[4] = aabb.min.z;
            node.bounds[5] = aabb.max.z;
            return *this;
        }

        AABBProxy& expand(const AABB& aabb) { return *this = this->aabb().expand(aabb); }
        AABBProxy& expand(const glm::vec3& vec) { return *this = aabb().expand(vec); }
        [[nodiscard]] float half_area() const { return aabb().half_area(); }
    };

    AABBProxy aabb() { return AABBProxy(*this); }
    [[nodiscard]] AABB aabb() const { return AABBProxy(const_cast<BvhNode&>(*this)); }

    [[nodiscard]] bool is_leaf() const { return primitive_count != 0; }
};

struct Bvh {
    // By convention, the root node is located at index 0 in this array
    std::vector<BvhNode> nodes;
    std::vector<uint32_t> primitive_indices;

    [[nodiscard]] std::vector<std::vector<AABB>> collect_aabbs_by_depth() const
    {
        std::vector<std::vector<AABB>> aabbs;
        collect_aabbs_by_depth(aabbs, 0, 0);
        return aabbs;
    }

    template <typename Primitive>
    std::vector<Primitive> permute_primitives(const std::vector<Primitive>& primitives) const
    {
        std::vector<Primitive> permuted_primitives(primitives.size());
        for (size_t i = 0; i < primitive_indices.size(); ++i)
            permuted_primitives[i] = primitives[primitive_indices[i]];
        return permuted_primitives;
    }

private:
    void collect_aabbs_by_depth(
        std::vector<std::vector<AABB>>& aabbs,
        size_t depth, size_t node_index) const;
};

