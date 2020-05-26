//
// Created by legend on 5/26/20.
//

#pragma once

#include <glm/glm.hpp>
#include <vector>

class camera
{
   public:
    explicit camera(float aspect);
    ~camera();

    void move(float x, float y, float z);
    void rotate(float amount, glm::vec3 axis);
    void set_fov(float fov);
    std::vector<glm::vec4> get_data();

   private:
    void recalculate_values();
    float fov, aspect;
    glm::vec3 origin, center, horizontal, vertical;
    glm::mat4 matrix;
};
