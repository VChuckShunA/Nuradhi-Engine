#include "game/game_systems.hpp"
#include "game/game_components.hpp"
#include "engine/core/components.hpp"
#include "engine/physics/physics.hpp"
#include "engine/sprite/sprite.hpp"

//libs
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace game {

	using nrd::ecs::Entity;

	void playerControlSystem(nrd::Engine& engine, float dt)
	{
		const nrd::Input& input = engine.input();
		engine.registry().each<PlayerController, nrd::RigidBody>([&](Entity, PlayerController& controller, nrd::RigidBody& body) {
			//+z is forward, away from the camera
			glm::vec3 direction{ input.axis("moveX"), 0.f, input.axis("moveZ") };
			if (glm::dot(direction, direction) > 0.f) direction = glm::normalize(direction);
			body.velocity.x = direction.x * controller.moveSpeed;
			body.velocity.z = direction.z * controller.moveSpeed;

			//holding jump jumps again on landing
			if (input.actionDown("jump") && body.grounded) {
				body.velocity.y = -controller.jumpSpeed; //-y is up
				body.grounded = false;
			}
		});
	}

	void cameraYawSystem(nrd::Engine& engine, float dt)
	{
		const float turn = engine.input().axis("cameraYaw");
		engine.registry().each<CameraYawControl, nrd::Transform>([&](Entity, CameraYawControl& control, nrd::Transform& transform) {
			transform.rotation.y = glm::mod(transform.rotation.y + turn * control.speed * dt, glm::two_pi<float>());
		});
	}

	void orbitSystem(nrd::Engine& engine, float dt)
	{
		engine.registry().each<Orbit, nrd::Transform>([&](Entity, Orbit& orbit, nrd::Transform& transform) {
			const glm::mat4 rotation = glm::rotate(glm::mat4{ 1.f }, orbit.speed * dt, orbit.axis);
			transform.position = orbit.centre + glm::vec3(rotation * glm::vec4(transform.position - orbit.centre, 1.f));
		});
	}

	void debugOverlayToggleSystem(nrd::Engine& engine, float dt)
	{
		if (engine.input().actionPressed("toggleDebug")) {
			engine.renderer().setDebugOverlay(!engine.renderer().debugOverlay());
		}
	}

	void characterAnimationSystem(nrd::Engine& engine, float dt)
	{
		engine.registry().each<CharacterAnimations, nrd::Sprite, nrd::RigidBody>(
			[&](Entity, CharacterAnimations& animations, nrd::Sprite& sprite, nrd::RigidBody& body) {
				//face the way we're walking; the sheets are drawn facing right
				if (body.velocity.x < 0.f) sprite.flipX = true;
				else if (body.velocity.x > 0.f) sprite.flipX = false;

				const bool walking = body.velocity.x != 0.f || body.velocity.z != 0.f;
				if (!body.grounded) sprite.animator.play(animations.jump);
				else if (walking) sprite.animator.play(animations.walk);
				else sprite.animator.play(animations.idle);
			});
	}
}
