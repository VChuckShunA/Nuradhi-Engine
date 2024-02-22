#include "nrd_player.hpp"
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
	NrdPlayer::NrdPlayer()
	{
		moveSpeed = 0;
		moveSpeedMax = 4.0f;
		hp = hpMax = 20;
		damage = 0;


		direction = DIR_DOWN;
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