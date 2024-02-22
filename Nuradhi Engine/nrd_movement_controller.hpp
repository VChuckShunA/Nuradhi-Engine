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
                int moveLeft = GLFW_KEY_J;
                int moveRight = GLFW_KEY_L;
                int moveForward = GLFW_KEY_I;
                int moveBackward = GLFW_KEY_K;
                int moveUp = GLFW_KEY_U;
                int moveDown = GLFW_KEY_O;
            };

        NrdPlayer* nrdPlayer;


        KeyMappings keys{};
        float moveSpeed{};
        void moveInPlaneXZ(GLFWwindow* window, float dt, lve::LveGameObject& player);
    };
}