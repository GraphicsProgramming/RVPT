//
// Created by legend on 20/8/2020.
//

#include "bvh_node.h"

BvhNode::~BvhNode()
{
    free(left);
    free(right);
}

void BvhNode::split(std::vector<std::vector<AABB>>& bounds, int depth, int max_depth)
{
    // Bad naming I know
    bounds[depth].push_back(this->bounds);

    if (depth + 1 < max_depth)
    {
        float split_axis_half_length;
        int split_axis([&]() {
            float largest_axis_value = 0;
            int best_axis = 0;
            for (int i = 0; i < 3; i++)
            {
                float axis_size = this->bounds.max[i] - this->bounds.min[i];
                if (axis_size > largest_axis_value)
                {
                    largest_axis_value = axis_size;
                    best_axis = i;
                }
            }
            split_axis_half_length = largest_axis_value * 0.5f;
            return best_axis;
        }());

        left = new BvhNode();
        for (int i = 0; i < 3; i++)
        {
            float max_axis = i == split_axis ? split_axis_half_length + this->bounds.min[i] : this->bounds.max[i];
            left->bounds.min[i] = this->bounds.min[i];
            left->bounds.max[i] = max_axis;
        }
        left->split(bounds, depth + 1, max_depth);

        right = new BvhNode();
        for (int i = 0; i < 3; i++)
        {
            float min_axis = i == split_axis ? this->bounds.max[i] - split_axis_half_length : this->bounds.min[i];
            right->bounds.max[i] = this->bounds.max[i];
            right->bounds.min[i] = min_axis;
        }
        right->split(bounds, depth + 1, max_depth);
    }
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
