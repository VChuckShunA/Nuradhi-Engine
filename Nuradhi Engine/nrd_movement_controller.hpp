#pragma once

#include "lve_game_object.hpp"
#include "lve_window.hpp"
#include "nrd_player.hpp"
namespace nrd {
   class NrdPlayer;
   class NrdMovementController {
    public:

        NrdMovementController(NrdPlayer* player); // Constructor
            struct KeyMappings {
                int moveLeft = GLFW_KEY_A;
                int moveRight = GLFW_KEY_D;
                int moveForward = GLFW_KEY_W;
                int moveBackward = GLFW_KEY_S;
                int vKey = GLFW_KEY_V;
                int spaceKey = GLFW_KEY_SPACE;
            };

        NrdPlayer* nrdPlayer;

        float floatDial(GLFWwindow* window, float value);
        KeyMappings keys{};
        float moveSpeed{};
        void moveInPlaneXZ(GLFWwindow* window, float dt, lve::LveGameObject* player);
    };
}