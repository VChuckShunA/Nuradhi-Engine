#pragma once

#include <glm/glm.hpp>

//std
#include <optional>
#include <vector>

namespace nrd {
	//Axis aligned box in world space. Vulkan's +y points down, so min.y is the top of the box.
	struct Aabb {
		glm::vec3 min{ 0.f };
		glm::vec3 max{ 0.f };

		static Aabb fromCenterHalfExtents(const glm::vec3& center, const glm::vec3& halfExtents);

		glm::vec3 center() const { return (min + max) * 0.5f; }
		glm::vec3 halfExtents() const { return (max - min) * 0.5f; }

		//boxes that only touch faces do not overlap, so resting on a floor is not a collision
		bool overlaps(const Aabb& other) const;
	};

	//The smallest move that separates one box from another
	struct Contact {
		glm::vec3 normal{ 0.f }; //unit axis pointing out of the other box, the direction to push
		float depth = 0.f; //how far to push along normal
	};

	//Contact that pushes `box` out of `obstacle`, or nothing if they don't overlap.
	//Ties prefer the vertical axis, so landing on a corner puts you on top instead of shoving you sideways.
	std::optional<Contact> computeContact(const Aabb& box, const Aabb& obstacle);

	//Collision shape of an entity, in world units relative to the entity's position.
	//It ignores the render transform's scale and rotation, so a sprite can change size without changing its hitbox.
	struct Collider {
		glm::vec3 offset{ 0.f }; //box centre relative to the entity's position
		glm::vec3 halfExtents{ 0.5f };
		bool isStatic = true; //static colliders are obstacles; they never get pushed

		static Collider box(const glm::vec3& halfExtents, const glm::vec3& offset = glm::vec3{ 0.f }, bool isStatic = true);

		Aabb worldBounds(const glm::vec3& position) const {
			return Aabb::fromCenterHalfExtents(position + offset, halfExtents);
		}
	};
}
