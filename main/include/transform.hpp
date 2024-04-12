#pragma once
#include <gtc/quaternion.hpp>

#include "vec3.hpp"

struct Transform
{
    glm::vec3 translation{ 0.0f };
    glm::vec3 scale{ 1.0f };
    glm::quat rotation{ glm::identity<glm::quat>() };
};
