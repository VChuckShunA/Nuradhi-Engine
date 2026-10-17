#pragma once

#include "engine/ecs/registry.hpp"

//libs
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

namespace nrd {

	//Where an entity is. World space follows Vulkan: +x right, +y DOWN, +z away from the camera.
	struct Transform {
		glm::vec3 position{ 0.f };
		glm::vec3 rotation{ 0.f }; //radians, Tait-Bryan angles applied Y(1), X(2), Z(3)
		glm::vec3 scale{ 1.f };

		glm::mat4 mat4() const; //translate * Ry * Rx * Rz * scale
		glm::mat3 normalMatrix() const;
	};

	//Put this on one entity with a Transform to render the scene from it: the view comes from its position and
	//rotation, the projection from these settings
	struct Camera {
		enum class Projection { Orthographic, Perspective };
		Projection projection = Projection::Orthographic;
		float orthographicHeight = 2.f; //world units visible top to bottom; the width follows the window's aspect ratio
		float fovY = glm::radians(50.f); //perspective only
		float nearPlane = 0.1f;
		float farPlane = 100.f;
	};

	//Moves the entity to follow another one on the chosen axes, keeping an offset
	struct CameraFollow {
		ecs::Entity target = ecs::NullEntity;
		glm::bvec3 followAxes{ true, false, false };
		glm::vec3 offset{ 0.f };
	};

	void updateCameraFollow(ecs::Registry& registry);
}
