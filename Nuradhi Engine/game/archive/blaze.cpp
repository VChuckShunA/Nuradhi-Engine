#include "game/archive/blaze.hpp"
#include "engine/core/components.hpp"
#include "engine/physics/collision.hpp"
#include "engine/physics/physics.hpp"

//std
#include <iostream>

namespace game::archive {

	using namespace nrd;

	//Blaze's attacks. Each animation's frames last exactly the move's duration (3 frames at 12 fps = 0.25 s),
	//and the hitbox is live on the middle frame, where the fist or foot is out.
	Combat blazeCombat()
	{
		Combat combat{};
		AttackMove punch{};
		punch.animation = "punch";
		punch.duration = 3.f / 12.f;
		punch.activeStart = 1.f / 12.f;
		punch.activeEnd = 2.f / 12.f;
		punch.hitboxOffset = { 0.38f, -0.72f, 0.f }; //a fist's length in front, at chest height
		punch.hitboxHalfExtents = { 0.14f, 0.1f, 0.25f };
		punch.damage = 1;
		punch.knockback = 0.5f;
		combat.moves["punch"] = punch;

		AttackMove kick{};
		kick.animation = "kick";
		kick.duration = 3.f / 10.f;
		kick.activeStart = 1.f / 10.f;
		kick.activeEnd = 2.f / 10.f;
		kick.hitboxOffset = { 0.55f, -0.55f, 0.f }; //longer reach, waist height
		kick.hitboxHalfExtents = { 0.25f, 0.12f, 0.25f };
		kick.damage = 2;
		kick.knockback = 1.f;
		combat.moves["kick"] = kick;

		//jump attacks hold their pose and stay live until she lands (a jump lasts about 0.7 s)
		AttackMove jumpPunch{}; //flying knee
		jumpPunch.animation = "jumpPunch";
		jumpPunch.duration = 0.6f;
		jumpPunch.activeStart = 0.05f;
		jumpPunch.activeEnd = 0.6f;
		jumpPunch.hitboxOffset = { 0.3f, -0.55f, 0.f }; //the knee, in front at mid height
		jumpPunch.hitboxHalfExtents = { 0.18f, 0.2f, 0.25f }; //tall enough to catch a standing head even at the top of the jump
		jumpPunch.damage = 1;
		jumpPunch.knockback = 0.8f;
		jumpPunch.airborne = true;
		combat.moves["jumpPunch"] = jumpPunch;

		AttackMove jumpKick{}; //diagonal flying kick
		jumpKick.animation = "jumpKick";
		jumpKick.duration = 0.6f;
		jumpKick.activeStart = 0.1f; //after the turn into the kick
		jumpKick.activeEnd = 0.6f;
		jumpKick.hitboxOffset = { 0.5f, -0.2f, 0.f }; //the foot, low and in front
		jumpKick.hitboxHalfExtents = { 0.25f, 0.15f, 0.25f };
		jumpKick.damage = 2;
		jumpKick.knockback = 1.2f;
		jumpKick.airborne = true;
		combat.moves["jumpKick"] = jumpKick;

		//special: an energy palm blast. Slow to come out, but the beam reaches well past a kick and hits
		//everything in it, so it can only be used every 1.5 s.
		AttackMove special{};
		special.animation = "special";
		special.duration = 6.f / 12.f;
		special.activeStart = 2.f / 12.f; //after the two charge frames
		special.activeEnd = 6.f / 12.f;
		special.hitboxOffset = { 0.8f, -0.72f, 0.f }; //the beam, from her palm forward at chest height
		special.hitboxHalfExtents = { 0.55f, 0.15f, 0.3f };
		special.damage = 3;
		special.knockback = 2.f;
		special.cooldown = 1.5f;
		special.cooldownGroup = "special";
		combat.moves["special"] = special;

		//jump special: a flaming dive. The fire wraps her body and trails out in front, so the hitbox is big;
		//it's her one air attack for the jump and shares the ground special's cooldown.
		AttackMove jumpSpecial{};
		jumpSpecial.animation = "jumpSpecial";
		jumpSpecial.duration = 0.7f;
		jumpSpecial.activeStart = 0.08f; //after the tuck
		jumpSpecial.activeEnd = 0.7f;
		jumpSpecial.hitboxOffset = { 0.45f, -0.45f, 0.f };
		jumpSpecial.hitboxHalfExtents = { 0.55f, 0.35f, 0.3f };
		jumpSpecial.damage = 3;
		jumpSpecial.knockback = 2.f;
		jumpSpecial.airborne = true;
		jumpSpecial.cooldown = 1.5f;
		jumpSpecial.cooldownGroup = "special";
		combat.moves["jumpSpecial"] = jumpSpecial;
		return combat;
	}

	//Blaze's sprite sheet: the frames are found from the image itself, the animations pick which frames to play
	std::shared_ptr<SpriteSheet> loadBlazeSheet(bool describe)
	{
		auto sheet = std::make_shared<SpriteSheet>(SpriteSheet::loadFromFile("Images/Sprites/Blaze.png"));
		const int row0 = 0; //stance, walk, punch, jump and kicks
		sheet->addAnimation({ "idle", //fighting stance, breathing back and forth
			{ sheet->frameAt(row0, 0), sheet->frameAt(row0, 1), sheet->frameAt(row0, 2), sheet->frameAt(row0, 1) },
			6.f, true });
		sheet->addRowAnimation("walk", row0, 3, 6, 10.f);
		sheet->addRowAnimation("run", 8, 1, 6, 14.f); //row 8: six-frame sprint
		sheet->addRowAnimation("jump", row0, 10, 3, 8.f, false); //crouch, rise, then hold the falling pose
		const int row3 = 3, row4 = 4;
		sheet->addAnimation({ "punch", //wind up, straight jab, pull back
			{ sheet->frameAt(row3, 11), sheet->frameAt(row3, 10), sheet->frameAt(row3, 11) }, 12.f, false });
		sheet->addAnimation({ "kick", //chamber, front kick, recover
			{ sheet->frameAt(row4, 2), sheet->frameAt(row4, 0), sheet->frameAt(row4, 2) }, 10.f, false });
		const int row2 = 2; //energy blast
		sheet->addAnimation({ "special", //charge twice, thrust the palm, then hold the long beam
			{ sheet->frameAt(row2, 5), sheet->frameAt(row2, 6), sheet->frameAt(row2, 1), sheet->frameAt(row2, 2), sheet->frameAt(row2, 9), sheet->frameAt(row2, 9) },
			12.f, false });
		sheet->addAnimation({ "jumpSpecial", //tuck, then dive in a trail of fire until landing
			{ sheet->frameAt(7, 10), sheet->frameAt(7, 11) }, 12.f, false });
		sheet->addAnimation({ "hurt", { sheet->frameAt(3, 3) }, 8.f, false }); //doubled over from a hit
		const int row1 = 1; //airborne poses
		sheet->addAnimation({ "jumpPunch", { sheet->frameAt(row1, 1) }, 10.f, false }); //flying knee, held
		sheet->addAnimation({ "jumpKick", //turn into a diagonal kick, held until landing
			{ sheet->frameAt(row1, 0), sheet->frameAt(row1, 2) }, 10.f, false });
		if (describe) sheet->describe(std::cout); //every detected frame with its row and column
		return sheet;
	}

	ecs::Entity spawnBlaze(ecs::Registry& registry, TextureHandle texture, const std::shared_ptr<SpriteSheet>& sheet,
		const glm::vec3& position, bool asPlayer)
	{
		//Her position is her feet, like the sprite's pivot, so her body box sits on top of it (-y is up):
		//1 unit tall like the sprite, half a unit wide and deep.
		const ecs::Entity blaze = registry.create();
		registry.add<Transform>(blaze, Transform{ position });
		registry.add<Sprite>(blaze, sheet, texture, BLAZE_PIXELS_PER_UNIT).keepBackInsideCollider = true;
		registry.add<RigidBody>(blaze);
		registry.add<Collider>(blaze, Collider::box({ 0.25f, 0.5f, 0.25f }, { 0.f, -0.5f, 0.f }, false));
		registry.add<CharacterAnimations>(blaze, CharacterAnimations{ "idle", "walk", "run", "jump", "hurt" });
		registry.add<Combat>(blaze, blazeCombat());
		if (asPlayer) {
			registry.add<PlayerController>(blaze);
			registry.add<Health>(blaze, Health{ 20, 20, 0.f, false }); //she stays down at 0 hp, as the player does
			registry.add<Name>(blaze, Name{ "Blaze (player)" });
		}
		return blaze;
	}
}
