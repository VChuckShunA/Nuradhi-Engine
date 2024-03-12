#include "nrd_player.hpp"
#include "nrd_debugLine.hpp"
#include "lve_frame_info.hpp"
#include <glm/gtx/string_cast.hpp>

#include "nrd_solidObject.hpp"
const std::string nrd::NrdPlayer::HERO_ANIM_UP = "up";
const std::string nrd::NrdPlayer::HERO_ANIM_DOWN = "down";
const std::string nrd::NrdPlayer::HERO_ANIM_LEFT = "left";
const std::string nrd::NrdPlayer::HERO_ANIM_RIGHT = "right";
const std::string nrd::NrdPlayer::HERO_ANIM_IDLE_UP = "idleUp";
const std::string nrd::NrdPlayer::HERO_ANIM_IDLE_DOWN = "idleDown";
const std::string nrd::NrdPlayer::HERO_ANIM_IDLE_LEFT = "idleLeft";
const std::string nrd::NrdPlayer::HERO_ANIM_IDLE_RIGHT = "idleRight";
const std::string nrd::NrdPlayer::HERO_SLASH_ANIM_UP = "slashUp";
const std::string nrd::NrdPlayer::HERO_SLASH_ANIM_DOWN = "slashDown";
const std::string nrd::NrdPlayer::HERO_SLASH_ANIM_LEFT = "slashLeft";
const std::string nrd::NrdPlayer::HERO_SLASH_ANIM_RIGHT = "slashRight";
const std::string nrd::NrdPlayer::HERO_DASH_ANIM_UP = "dashUp";
const std::string nrd::NrdPlayer::HERO_DASH_ANIM_DOWN = "dashDown";
const std::string nrd::NrdPlayer::HERO_DASH_ANIM_LEFT = "dashLeft";
const std::string nrd::NrdPlayer::HERO_DASH_ANIM_RIGHT = "dashRight";
const std::string nrd::NrdPlayer::HERO_ANIM_DIE = "die";

const int nrd::NrdPlayer::HERO_STATE_IDLE = 0;
const int nrd::NrdPlayer::HERO_STATE_MOVE = 1;
const int nrd::NrdPlayer::HERO_STATE_SLASH = 2;
const int nrd::NrdPlayer::HERO_STATE_DASH = 3;
const int nrd::NrdPlayer::HERO_STATE_DEAD = 4;

namespace nrd {
	NrdPlayer::NrdPlayer(lve::LveDevice& lveDevice)
		: NrdLivingEntity(lveDevice,-1.0f,1.0f, -1.0f, 1.0f, -1.0f, 1.0f)
		{
		moveSpeed = 0;
		moveSpeedMax = 4.0f;
		hp = hpMax = 20;
		damage = 0;
		direction = DIR_DOWN;

		//nrd::NrdSweptAABB collider(lveDevice);
		//nrd::NrdDebugLine::Builder colliderLineBuilder{};
		//colliderLineBuilder= collider.getCollider() ;
	
		//debugline = std::make_unique<NrdDebugLine>(lveDevice, colliderLineBuilder);

		}
	void NrdPlayer::draw(lve::FrameInfo& frameInfo, VkPipelineLayout& pipelineLayout)
	{
		lve::LveGameObject::draw(frameInfo, pipelineLayout);
		//draw debug lines here
		debugline->bind(frameInfo.commandBuffer);
		debugline->draw(frameInfo.commandBuffer);
		/*std::cout << "x min : " << this->transform.translation.x + collider.xMin << "\n";
		std::cout << "x max : " << this->transform.translation.x + collider.xMax << "\n";
		std::cout << "y min : " << this->transform.translation.y + collider.yMin << "\n";
		std::cout << "y max : " << this->transform.translation.y + collider.yMax << "\n";
		std::cout << "z min : " << this->transform.translation.z + collider.zMin << "\n";
		std::cout << "z max : " << this->transform.translation.z + collider.zMax << "\n";
*/


//list of potential collisions

		/**/
		for (auto i = nrd::NrdEntity::entities.begin(); i != nrd::NrdEntity::entities.end(); i++)
		{
			if ((*i)!=this && distanceBetweenTwoEntities(this,*i)<3)
			{
				std::cout << "Collision ahead: " << distanceBetweenTwoEntities(this, *i) << "\n";
				//if (this->collider.xMax <1 &&
				//	this->collider.yMax < &&)
			}
			else {
				std::cout << "Nothing to collide with" << "\n";
			}
		}
/*
		// if we have a list of potential entities that we may hit, then lets check them properly to do collision resolution
		if (collisions.size() > 0) {
			updateCollisionBox();

			float collisionTime = 1;//1 means no collisions, anything less, e.g 0.234, means we collided part of the way of our movement
			float normalX, normalY; //to help work out which side we crash into stuff

			//our collisionBox before we tried to move
			SDL_Rect startingCollisionBox = lastCollisionBox;

			//loop through the entities that are in our short list from broadphase
			for (auto entity = collisions.begin(); entity != collisions.end(); entity++) {
				//temporary variables for normal x and y and also temp collisionTime
				float tmpNormalX, tmpNormalY;
				float tmpCollisionTime = SweptAABB(startingCollisionBox, totalXMove, totalYMove, (*entity)->collisionBox, tmpNormalX, tmpNormalY);

				//if this tmpCollision Time is less than last collision time, use it instead
				if (tmpCollisionTime < collisionTime) {
					collisionTime = tmpCollisionTime;
					normalX = tmpNormalX;
					normalY = tmpNormalY;
				}
			}

			//if there was a collision, lets slide off of it
			if (collisionTime < 1.0f) {
				//if we die on solids, run the crash function
				if (dieOnSolids) {
					crashOntoSolid();
				}

				//Move our collisionBox position up to where we collided
				startingCollisionBox.x += totalXMove * collisionTime;
				startingCollisionBox.y += totalYMove * collisionTime;

				//how much move time was remaining
				float remainingTime = 1.0f - collisionTime;
				//update entities x and y to where we bumped into other entity
				x = startingCollisionBox.x + startingCollisionBox.w / 2;
				y = startingCollisionBox.y - collisionBoxYOffset;
				//COLLISION RESPONSE: SLIDE
				//work out dotProduct using remainingTime
				float dotProd = (totalXMove * totalYMove * normalX) * remainingTime;
				totalXMove = dotProd * normalX;
				totalYMove = dotProd * normalY;
				x += totalXMove;
				y += totalYMove;
				//store collisionBox at this point
				lastCollisionBox = startingCollisionBox;
				//update our entities main collisionBox
				updateCollisionBox();

				//BECAUSE OUR SLIDING MAY HAVE BUMPED INTO OTHER OBJECT, BETTER RUN THE FUNCTION AGAIN
				updateCollisions();
			}*/
	}
	void NrdPlayer::update()
	{
		//check if dead
		if (hp < 1 && state != HERO_STATE_DEAD) {
			changeAnimation(HERO_STATE_DEAD, true);
			moving = false;
			die();
		}

		//updateCollisionBox();
		updateMovement();
		//updateCollisions();

		//updateHitBox();
		//updateDamage();

		//updateAnimation();
		//updateInvincibleTimer();
	}

	void NrdPlayer::slash()
	{
	}

	void NrdPlayer::dash()
	{
	}

	void NrdPlayer::die()
	{
	}

	void NrdPlayer::revive()
	{
	}

	void NrdPlayer::changeAnimation(int newState, bool resetFrameToBegining)
	{
	}

	void NrdPlayer::updateAnimation()
	{
	}

	void NrdPlayer::updateDamage()
	{
	}

}