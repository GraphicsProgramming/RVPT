//
// Created by legend on 21/8/2020.
//

#include "bvh_builder.h"

BvhBuilder::BvhBuilder(BvhType type) : bvh_build_type(type) {}

BvhNode* BvhBuilder::build_global_bvh(std::vector<AABB> &bounding_boxes_no_depth, std::vector<std::vector<AABB>>& bounding_boxes_with_depth,
                                                  const std::vector<Triangle> &triangle_primitives)
{
    BvhNode* ret;
    switch (bvh_build_type)
    {
            /*
             * Todo: Add more Bvh Build types?
             */
        case BottomTop:

            /*
             * Bottom -> Top BVH
             */

            std::vector<AABB> base_bounding_boxes;
            base_bounding_boxes.resize(triangle_primitives.size());
            for (size_t i = 0; i < triangle_primitives.size(); i++)
            {
                const Triangle &triangle = triangle_primitives[i];
                base_bounding_boxes[i] =
                    AABB(fminf(fminf(triangle.vertex0.x, triangle.vertex1.x), triangle.vertex2.x),
                         fminf(fminf(triangle.vertex0.y, triangle.vertex1.y), triangle.vertex2.y),
                         fminf(fminf(triangle.vertex0.z, triangle.vertex1.z), triangle.vertex2.z),
                         fmaxf(fmaxf(triangle.vertex0.x, triangle.vertex1.x), triangle.vertex2.x),
                         fmaxf(fmaxf(triangle.vertex0.y, triangle.vertex1.y), triangle.vertex2.y),
                         fmaxf(fmaxf(triangle.vertex0.z, triangle.vertex1.z), triangle.vertex2.z));
            }
            bounding_boxes_with_depth.push_back(base_bounding_boxes);

            std::vector<BvhNode*> current_nodes;
            std::vector<BvhNode*> next_nodes;
            current_nodes.resize(base_bounding_boxes.size());
            for (size_t i = 0; i < base_bounding_boxes.size(); i++)
            {
                current_nodes[i] = new BvhNode();
                current_nodes[i]->bounds = base_bounding_boxes[i];
                current_nodes[i]->triangles.push_back(triangle_primitives[i]);
            }

            while (current_nodes.size() > 1)
            {
                // Get the previous power of 2
                size_t currently_hittable = 1;
                while (currently_hittable < current_nodes.size())
                    currently_hittable <<= 1;
                currently_hittable /= 2;

                if (currently_hittable == 1) break;

                std::vector<AABB> layer_bounds;
                for (size_t i = 0; i < current_nodes.size(); i++)
                {
                    // This is meant as a check to see if we can use this in the current bvh depth
                    if (i < currently_hittable)
                    {
                        size_t next_node_index = i;
                        if (i % 2 != 0) next_node_index--;
                        next_node_index /= 2;
                        if (next_nodes.size() < next_node_index + 1) next_nodes.push_back(new BvhNode());
                        if (i % 2 == 0)
                        {
                            next_nodes[next_node_index]->left = current_nodes[i];
                            next_nodes[next_node_index]->bounds.expand(
                                next_nodes[next_node_index]->left->bounds.min);
                            next_nodes[next_node_index]->bounds.expand(
                                next_nodes[next_node_index]->left->bounds.max);
                        }
                        else
                        {
                            // In this case, we can assume the past one exists and we want to get the BvH node
                            // That is the closest.

                            // Find the closest node in world space, and swap it with our current node
                            size_t closest_node_index = -1;
                            float closest_node_distance_sq = std::numeric_limits<float>::infinity();
                            glm::vec3 left_node_center = next_nodes[next_node_index]->left->bounds.center();
                            for (int a = i; a < current_nodes.size(); a++)
                            {
                                glm::vec3 node_center = current_nodes[a]->bounds.center();
                                float dx = fabsf(left_node_center.x - node_center.x);
                                float dy = fabsf(left_node_center.y - node_center.y);
                                float dz = fabsf(left_node_center.z - node_center.z);
                                float distance_to_node_sq = dx * dx + dy * dy + dz * dz;
                                if (distance_to_node_sq < closest_node_distance_sq)
                                {
                                    closest_node_index = a;
                                    closest_node_distance_sq = distance_to_node_sq;
                                }
                            }
                            std::swap(current_nodes[i], current_nodes[closest_node_index]);

                            next_nodes[next_node_index]->right = current_nodes[i];
                            next_nodes[next_node_index]->bounds.expand(
                                next_nodes[next_node_index]->right->bounds.min);
                            next_nodes[next_node_index]->bounds.expand(
                                next_nodes[next_node_index]->right->bounds.max);
                            layer_bounds.push_back(next_nodes[next_node_index]->bounds);
                        }
                    }
                    else // In the case it isn't, just pass it directly to the higher level of the tree
                    {
                        next_nodes.push_back(current_nodes[i]);
                    }
                }

                bounding_boxes_with_depth.insert(bounding_boxes_with_depth.begin(), layer_bounds);
                layer_bounds.clear();
                current_nodes = next_nodes;
                next_nodes.clear();
            }

            ret = current_nodes[0];

            break;
    }

    return ret;
}
