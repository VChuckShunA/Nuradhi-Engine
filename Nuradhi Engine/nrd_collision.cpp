#include "nrd_collision.hpp"
#include "nrd_entity.hpp"

//std
#include <algorithm>
#include <cassert>

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

	void CollisionWorld::add(NrdEntity& entity)
	{
		assert(std::find(bodies.begin(), bodies.end(), &entity) == bodies.end() && "Entity is already in the collision world");
		bodies.push_back(&entity);
	}

	void CollisionWorld::remove(NrdEntity& entity)
	{
		bodies.erase(std::remove(bodies.begin(), bodies.end(), &entity), bodies.end());
	}

	CollisionResult CollisionWorld::resolve(NrdEntity& body) const
	{
		CollisionResult result{};
		for (int pass = 0; pass < MAX_RESOLVE_PASSES; pass++) {
			bool moved = false;
			for (const NrdEntity* other : bodies) {
				if (other == &body || !other->collider.isStatic) continue;

				const auto contact = computeContact(body.worldBounds(), other->worldBounds());
				if (!contact) continue;

				body.transform.translation += contact->normal * contact->depth;
				moved = true;
				result.contactCount++;
				//-y is up, so being pushed towards -y means we're standing on the obstacle
				if (contact->normal.y < 0.f) result.grounded = true;
				else if (contact->normal.y > 0.f) result.hitCeiling = true;
				else result.hitWall = true;
			}
			if (!moved) break;
		}
		return result;
	}
}
