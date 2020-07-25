//
// Created by legend on 6/15/20.
//

#pragma once

#include <glm/glm.hpp>

struct Material
{
    enum class Type
    {
        GLASS,
        LAMBERT,
        DYNAMIC
    };
    explicit Material(glm::vec4 albedo, glm::vec4 emission, Type type)
        : albedo(albedo), emission(emission)
    {
        data = glm::vec4();
        switch (type)
        {
            case Type::LAMBERT:
                data.x = 0;
                break;
            case Type::GLASS:
                data.x = 1;
                break;
            case Type::DYNAMIC:
                data.x = 2;
                break;
            default:
                data.x = 0;
        }
    }
    glm::vec4 albedo{};
    glm::vec4 emission{};
    glm::vec4 data{};
};
