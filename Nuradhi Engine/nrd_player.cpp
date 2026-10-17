#include "nrd_player.hpp"
#include "lve_frame_info.hpp"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>
#include <algorithm>

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

const std::string nrd::NrdPlayer::ANIM_IDLE = "idle";
const std::string nrd::NrdPlayer::ANIM_WALK = "walk";
const std::string nrd::NrdPlayer::ANIM_JUMP = "jump";

const int nrd::NrdPlayer::HERO_STATE_IDLE = 0;
const int nrd::NrdPlayer::HERO_STATE_MOVE = 1;
const int nrd::NrdPlayer::HERO_STATE_SLASH = 2;
const int nrd::NrdPlayer::HERO_STATE_DASH = 3;
const int nrd::NrdPlayer::HERO_STATE_DEAD = 4;

namespace nrd {
	NrdPlayer::NrdPlayer()
	{
		moveSpeed = 0;
		moveSpeedMax = 0.5f;
		hp = hpMax = 20;
		damage = 0;
		direction = DIR_DOWN;

		//the player's position is its feet, like the sprite's pivot, so the body box sits on top of it (-y is up):
		//1 unit tall like the sprite, half a unit wide and deep. It gets pushed; it doesn't push.
		collider = Collider::box({ 0.25f, 0.5f, 0.25f }, { 0.f, -0.5f, 0.f }, false);
	}

	void NrdPlayer::applyGravity(float dt)
	{
		//a long frame (window drag, breakpoint) would otherwise drop the player through thin floors in one step
		dt = std::min(dt, MAX_GRAVITY_STEP);
		verticalVelocity = std::min(verticalVelocity + gravity * dt, terminalVelocity);
		transform.translation.y += verticalVelocity * dt;
	}

	void NrdPlayer::jump()
	{
		if (grounded)
		{
			verticalVelocity = -jumpSpeed; //-y is up in Vulkan
			grounded = false;
		}
	}

	void NrdPlayer::resolveCollisions(const CollisionWorld& world)
	{
		const CollisionResult result = world.resolve(*this);
		grounded = result.grounded;
		//landing stops the fall, hitting something overhead stops the rise
		if (result.grounded && verticalVelocity > 0.f) verticalVelocity = 0.f;
		if (result.hitCeiling && verticalVelocity < 0.f) verticalVelocity = 0.f;

		if (NrdGlobals::debugging && result.contactCount > 0) {
			std::cout << "contacts " << result.contactCount
				<< " grounded " << result.grounded
				<< " ceiling " << result.hitCeiling
				<< " wall " << result.hitWall
				<< " position " << glm::to_string(transform.translation) << "\n";
		}
	}
	void NrdPlayer::update()
	{
		//check if dead
		if (hp < 1 && state != HERO_STATE_DEAD) {
			changeAnimation(HERO_STATE_DEAD, true);
			moving = false;
			die();
		}

		updateMovement();

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

	void NrdPlayer::updateAnimation(float dt)
	{
		if (!sprite) return;

		//face the way we last walked; the sheet is drawn facing right
		if (moveInput.x < 0.f) sprite->flipX = true;
		else if (moveInput.x > 0.f) sprite->flipX = false;

		if (!grounded) sprite->animator.play(ANIM_JUMP);
		else if (glm::dot(moveInput, moveInput) > 0.f) sprite->animator.play(ANIM_WALK);
		else sprite->animator.play(ANIM_IDLE);
		sprite->animator.update(dt);
	}

	void NrdPlayer::updateDamage()
	{
	}

}