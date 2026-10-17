#include "engine/physics/collision.hpp"

namespace nrd {

	Aabb Aabb::fromCenterHalfExtents(const glm::vec3& center, const glm::vec3& halfExtents)
	{
		const glm::vec3 extents = glm::abs(halfExtents);
		return Aabb{ center - extents, center + extents };
	}

	bool Aabb::overlaps(const Aabb& other) const
	{
		return min.x < other.max.x && max.x > other.min.x &&
			min.y < other.max.y && max.y > other.min.y &&
			min.z < other.max.z && max.z > other.min.z;
	}

	std::optional<Contact> computeContact(const Aabb& box, const Aabb& obstacle)
	{
		if (!box.overlaps(obstacle)) return std::nullopt;

		//on each axis, push towards whichever side of the obstacle is closer
		glm::vec3 depth{};
		glm::vec3 direction{};
		for (int axis = 0; axis < 3; axis++) {
			const float pushNegative = box.max[axis] - obstacle.min[axis];
			const float pushPositive = obstacle.max[axis] - box.min[axis];
			if (pushNegative < pushPositive) {
				depth[axis] = pushNegative;
				direction[axis] = -1.f;
			}
			else {
				depth[axis] = pushPositive;
				direction[axis] = 1.f;
			}
		}

		//shallowest axis wins; checking y first makes it win ties
		int axis = 1;
		if (depth.x < depth[axis]) axis = 0;
		if (depth.z < depth[axis]) axis = 2;

		Contact contact{};
		contact.normal[axis] = direction[axis];
		contact.depth = depth[axis];
		return contact;
	}

	Collider Collider::box(const glm::vec3& halfExtents, const glm::vec3& offset, bool isStatic)
	{
		Collider collider{};
		collider.halfExtents = halfExtents;
		collider.offset = offset;
		collider.isStatic = isStatic;
		return collider;
	}
}
