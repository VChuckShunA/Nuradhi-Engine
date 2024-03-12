#include "nrd_entity.hpp"
#include "nrd_solidObject.hpp"

const int	nrd::NrdEntity::DIR_UP = 0, 
			nrd::NrdEntity::DIR_DOWN = 1, 
			nrd::NrdEntity::DIR_LEFT = 2, 
			nrd::NrdEntity::DIR_RIGHT = 3,
			nrd::NrdEntity::DIR_NONE = -1;


namespace nrd {
	void NrdEntity::update()
	{
	}

	

	void NrdEntity::move(float angle)
	{
		moving = true;
		moveSpeed = moveSpeedMax;
		this->angle = angle;

		int newDirection = angleToDirection(angle);
		//if direction changed, update current animation
		if (direction != newDirection)
		{
			direction = newDirection;
			//changeAnimation(state, false);
		}
	}

	void NrdEntity::updateMovement()
	{
		//updateCollisionBox();
		//store collision Box before we move
		//lastCollisionBox = collisionBox;

		//reset total moves
		totalXMove = 0;
		totalYMove = 0;

		if (moveSpeed > 0)
		{
			//works out move distance using speed, dt(time since last loop) and multiplies by lerp
			float moveDist = moveSpeed * (nrd::NrdTimeController::nrdTimeController.dT) * moveLerp;
			if (moveDist > 0)
			{
				//xmove= distance * cos(angle in radians)
				float xMove = moveDist * (cos(angle * nrd::NrdGlobals::PI / 180));
				//ymove= distance * cos(angle in radians)
				float yMove = moveDist * (sin(angle * nrd::NrdGlobals::PI / 180));

				x += xMove;
				y += yMove;

				totalXMove = xMove;
				totalYMove = yMove;

				if (!moving) {
					moveSpeed -= moveDist;
				}
			}
		}
		/*
		//sliding around
		if (slideAmount > 0) {
			float slideDist = slideAmount * TimeController::timeController.dT * moveLerp;
			if (slideDist > 0) {
				float xMove = slideDist * (cos(slideAngle * Globals::PI / 180));
				float yMove = slideDist * (sin(slideAngle * Globals::PI / 180));

				x += xMove;
				y += yMove;

				totalXMove += xMove;
				totalYMove += yMove;
				slideAmount -= slideDist;

			}
			else {
				slideAmount = 0;
			}
		}*/

		//now that we've moved, move the collision box upto where we are now
		//updateCollisionBox();
		//to help with collision checking, union collisionBox with lastCollisionBox
		//SDL_UnionRect(&collisionBox, &lastCollisionBox, &collisionBox);
	}

	void NrdEntity::updateCollisionBox()
	{
	}

	void NrdEntity::updateCollisions()
	{
	}

	float NrdEntity::distanceBetweenTwoEntities(NrdEntity* e1, NrdEntity* e2)
	{
		//float d = abs(sqrt(pow(e2->x - e1->x, 2) + pow(e2->y - e1->y, 2)));
		float d = abs(sqrt(pow(e2->transform.translation.x - e1->transform.translation.x, 2) + pow(e2->transform.translation.y - e1->transform.translation.y, 2) + pow(e2->transform.translation.z - e1->transform.translation.z, 2)));
		return d;
	}

	float NrdEntity::angleBetweenTwoEntities(NrdEntity* e1, NrdEntity* e2)
	{
		float dx, dy;
		float x1 = e1->x, y1 = e1->y, x2 = e2->x, y2 = e2->y;

		dx = x2 - x1;
		dy = y2 - y1;

		return atan2(dy, dx) * 180 / NrdGlobals::PI;
	}

	float NrdEntity::distanceBetweenTwoPoints(float cx1, float cy1, float cx2, float cy2)
	{
		float d = abs(sqrt(pow(cx2 - cx1, 2) + pow(cy2 - cy1, 2)));
		return d;
	}

	int NrdEntity::angleToDirection(float angle)
	{
		if ((angle >= 0 && angle <= 45) || angle >= 315 && angle <= 360)
		{
			return DIR_RIGHT;
		}
		else if (angle >= 45 && angle <= 135) {
			return DIR_DOWN;
		}
		else if (angle >= 135 && angle <= 225) {
			return DIR_LEFT;
		}
		else
			return DIR_UP;
	}

	float NrdEntity::angleBetweenTwoPoints(float cx1, float cy1, float cx2, float cy2)
	{
		float dx = cx2 - cx1;
		float dy = cy2 - cy1;

		return atan2(dy, dx) * 180 / NrdGlobals::PI;
	}

	bool NrdEntity::EntityCompare(const NrdEntity* const& a, const NrdEntity* const& b)
	{
		// compare 2 entities in a list to help sorting (sorts based on y value)
		if (a != 0 && b != 0)
		{
			return (a->y < b->y);
		}
		else {
			return false;
		}
	}

	void NrdEntity::removeInactiveEntitiesFromList(std::list<NrdEntity*>* entityList, bool deleteEntities)
	{
		for (auto entity = entityList->begin(); entity != entityList->end();)
		{
			//if entity is not active
			if (!(*entity)->active)
			{
				if (deleteEntities)
				{
					delete (*entity);
				}
				entity = entityList->erase(entity);
			}
			else
			{
				entity++;
			}
		}
	}

	void NrdEntity::removeAllFromList(std::list<NrdEntity*>* entityList, bool deleteEntities)
	{
		for (auto entity = entityList->begin(); entity != entityList->end();)
		{
			if (deleteEntities)
			{
				delete (*entity);
			}
			entity = entityList->erase(entity);

		}
	}

	

	std::list<NrdEntity*> NrdEntity::entities;
	NrdEntity::NrdEntity(lve::LveDevice& lveDevice, float xMin, float xMax, float yMin, float yMax, float zMin, float zMax):
		xMin(xMin), xMax(xMax), yMin(yMin), yMax(yMax), zMin(zMin), zMax(zMax),
		collider(lveDevice, xMin, xMax, yMin, yMax, zMin, zMax)
	{
		NrdEntity::entities.push_back(this);
		//nrd::NrdSweptAABB collider(lveDevice);
		nrd::NrdDebugLine::Builder colliderLineBuilder{};
		colliderLineBuilder = collider.getCollider();

		debugline = std::make_unique<NrdDebugLine>(lveDevice, colliderLineBuilder);
	}


}