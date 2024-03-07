#include "nrd_player.hpp"
#include "nrd_debugLine.hpp"
#include "lve_frame_info.hpp"
#include <glm/gtx/string_cast.hpp>
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
		: collider(lveDevice,-1.0f,-1.0f,1.0f,1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f, 1.0f, 1.0f, 1.0f, -1.0f, -1.0f, -1.0f, -1.0f)
		{
		moveSpeed = 0;
		moveSpeedMax = 4.0f;
		hp = hpMax = 20;
		damage = 0;
		direction = DIR_DOWN;

		//nrd::NrdSweptAABB collider(lveDevice);
		nrd::NrdDebugLine::Builder colliderLineBuilder{};
		colliderLineBuilder= collider.getCollider() ;
	
		debugline = std::make_unique<NrdDebugLine>(lveDevice, colliderLineBuilder);

		}
	void NrdPlayer::draw(lve::FrameInfo& frameInfo, VkPipelineLayout& pipelineLayout)
	{
		lve::LveGameObject::draw(frameInfo, pipelineLayout);
		//draw debug lines here
		debugline->bind(frameInfo.commandBuffer);
		debugline->draw(frameInfo.commandBuffer);
		std::cout << "x min : " << this->transform.translation.x + collider.xMin << "\n";
		std::cout << "x max : " << this->transform.translation.x + collider.xMax << "\n";
		std::cout << "y min : " << this->transform.translation.y + collider.yMin << "\n";
		std::cout << "y max : " << this->transform.translation.y + collider.yMax << "\n";
		std::cout << "z min : " << this->transform.translation.z + collider.zMin << "\n";
		std::cout << "z max : " << this->transform.translation.z + collider.zMax << "\n";

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