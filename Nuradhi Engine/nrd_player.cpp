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
		moveSpeedMax = 0.5f;
		hp = hpMax = 20;
		damage = 0;
		direction = DIR_DOWN;

		}
	void NrdPlayer::draw(lve::FrameInfo& frameInfo, VkPipelineLayout& pipelineLayout)
	{
		lve::LveGameObject::draw(frameInfo, pipelineLayout);
		//draw debug lines here
		debugline->bind(frameInfo.commandBuffer);
		debugline->draw(frameInfo.commandBuffer);

//list of potential collisions

		//this->transform.translation.y += .0001;
		
		for (auto i = nrd::NrdEntity::entities.begin(); i != nrd::NrdEntity::entities.end(); i++)
		{
			if ((*i)!=this)
			{//Actual testing
				if (!
					((this->collider.xMax * this->transform.scale.x + this->transform.translation.x <= (*i)->collider.xMin * (*i)->transform.scale.x + (*i)->transform.translation.x) &&
						(this->collider.xMin * this->transform.scale.x + this->transform.translation.x <= (*i)->collider.xMin * (*i)->transform.scale.x + (*i)->transform.translation.x)
						||
						(this->collider.xMax * this->transform.scale.x + this->transform.translation.x >= (*i)->collider.xMax * (*i)->transform.scale.x + (*i)->transform.translation.x) &&
						(this->collider.xMin * this->transform.scale.x + this->transform.translation.x >= (*i)->collider.xMax * (*i)->transform.scale.x + (*i)->transform.translation.x))
					)
				{
					if (!
						((this->collider.yMax * this->transform.scale.y + this->transform.translation.y >= (*i)->collider.zMin * (*i)->transform.scale.z + (*i)->transform.translation.y) &&
							(this->collider.yMin * this->transform.scale.y + this->transform.translation.y >= (*i)->collider.zMin * (*i)->transform.scale.z + (*i)->transform.translation.y)
							||
							(this->collider.yMax * this->transform.scale.y + this->transform.translation.y <= (*i)->collider.zMax * (*i)->transform.scale.z + (*i)->transform.translation.y) &&
							(this->collider.yMin * this->transform.scale.y + this->transform.translation.y <= (*i)->collider.zMax * (*i)->transform.scale.z + (*i)->transform.translation.y))
						)
					{
						if (!
							((this->collider.zMax * this->transform.scale.z + this->transform.translation.z <= (*i)->collider.yMin * (*i)->transform.scale.y + (*i)->transform.translation.z) &&
								(this->collider.zMin * this->transform.scale.z + this->transform.translation.z <= (*i)->collider.yMin * (*i)->transform.scale.y + (*i)->transform.translation.z)
								||
								(this->collider.zMax * this->transform.scale.z + this->transform.translation.z >= (*i)->collider.yMax * (*i)->transform.scale.y + (*i)->transform.translation.z) &&
								(this->collider.zMin * this->transform.scale.z + this->transform.translation.z >= (*i)->collider.yMax * (*i)->transform.scale.y + (*i)->transform.translation.z))
							)
						{
							std::cout << " Collission" << "\n";
							float xmaxdif = this->collider.xMax * this->transform.scale.x + this->transform.translation.x - (*i)->collider.xMin * (*i)->transform.scale.x - (*i)->transform.translation.x;
							float xmindif = this->collider.xMin * this->transform.scale.x + this->transform.translation.x - (*i)->collider.xMax * (*i)->transform.scale.x - (*i)->transform.translation.x;

							float xDist=0, yDist=0, zDist=0;
							
							if (abs(xmaxdif) <= abs(xmindif))
							{
								xDist = abs(xmaxdif) * -1;
							}
							else if (abs(xmindif) <= abs(xmaxdif))
							{
								xDist = abs(xmindif);
							}

							float ymaxdif = this->collider.yMax * this->transform.scale.y + this->transform.translation.y - (*i)->collider.zMax * (*i)->transform.scale.z - (*i)->transform.translation.y;
							float ymindif = this->collider.yMin * this->transform.scale.y + this->transform.translation.y - (*i)->collider.zMin * (*i)->transform.scale.z - (*i)->transform.translation.y;

							if ((abs(ymaxdif)!=0) && (abs(ymaxdif) <= abs(ymindif)))
							{
								yDist = abs(ymaxdif) * -1;
							}
							else if ((abs(ymindif)!=0) && (abs(ymindif) <= abs(ymaxdif)))
							{
							yDist = abs(ymindif);
							}

							float zMaxDif = this->collider.zMax * this->transform.scale.z + this->transform.translation.z - (*i)->collider.yMin * (*i)->transform.scale.y - (*i)->transform.translation.z;
							float zMinDif = this->collider.zMin * this->transform.scale.z + this->transform.translation.z - (*i)->collider.yMax * (*i)->transform.scale.y - (*i)->transform.translation.z;
							if (abs(zMaxDif) <= abs(zMinDif))
							{
								zDist = abs(zMaxDif) * -1;
							}
							else if (abs(zMinDif) <= abs(zMaxDif))
							{

								zDist = abs(zMinDif);
							}
							if (abs(xDist) < abs(zDist) && abs(xDist) < abs(yDist))
							{
								this->transform.translation.x += xDist;
								std::cout << "X Collission" << "\n";
								std::cout << "Y Dist" << yDist << "\n";
								std::cout << "X Dist" << xDist << "\n";
								std::cout << "Z Dist" << zDist << "\n";
							}
							else if(abs(yDist) < abs(xDist) && abs(yDist) < abs(zDist)) {
								this->transform.translation.y += yDist;
								std::cout << "Y Collission" << "\n";
								std::cout << "Y Dist" << yDist<< "\n";
								std::cout << "X Dist" << xDist << "\n";
								std::cout << "Z Dist" << zDist << "\n";
							}
							else if (abs(zDist) < abs(xDist) && abs(zDist) < abs(yDist)) {
								this->transform.translation.z += zDist;
								std::cout << "Z Collission" << "\n";
								std::cout << "Y Dist" << yDist << "\n";
								std::cout << "X Dist" << xDist << "\n";
								std::cout << "Z Dist" << zDist << "\n";
							}
							
						}
					}
				}
				
				
				
			}
			else {
				//std::cout << "Nothing to collide with" << "\n";
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