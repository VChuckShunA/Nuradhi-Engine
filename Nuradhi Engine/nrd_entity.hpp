#pragma once
#include "lve_game_object.hpp"
#include "nrd_globals.hpp"
#include "nrd_time_controller.hpp"
#include "nrd_collision.hpp"
#include "nrd_sprite.hpp"

//std
#include <list>
#include <memory>

namespace nrd {
	class NrdEntity : public lve::LveGameObject {
	public:
		//collision box around transform.translation, in world units (see nrd_collision.hpp). Register the entity
		//with the level's CollisionWorld for it to take part in collision.
		nrd::Collider collider;
		Aabb worldBounds() const { return collider.worldBounds(transform.translation); }

		//optional: draw as the current frame of an animated sprite, with its pivot on transform.translation,
		//instead of drawing the model with the entity's transform. Give it a sprite quad model (createSpriteQuad).
		std::unique_ptr<SpriteRenderer> sprite;
		void draw(lve::FrameInfo& frameInfo, VkPipelineLayout& pipelineLayout) override;

		//reference constants
		static const int DIR_UP, DIR_DOWN, DIR_LEFT, DIR_RIGHT, DIR_NONE;
		//quick label to see what the entity is up to
		int state;

		float x, y;
		int direction;
		bool active = true; //entity turned on or off
		//string type = "entity"; // what type of entity is it? e.g hero, enemy, wall .etc
		bool moving; //is the entity moving?
		float angle; //angle to move entity in (360 degree angle)
		float moveSpeed;
		float moveSpeedMax;
		float slideAngle; //direction pushed in
		float slideAmount; //amount of push in the slideAngle
		float moveLerp = 4;
		float totalXMove, totalYMove; //keeps track of total x,y movement per movement turn. just incase we need to retract movement

		//AnimationSet* animSet; //set of all animations this entity can have
		//Animation* currentAnim; //current animation the entity is using
		//Frame* currentFrame; //the current frame in the above animation the entity is using
		//float frameTimer; //helps animate frame to frame

		//VIRTUAL FUNCTIONS
		virtual void update();
		virtual void move(float angle);
		virtual void updateMovement();

		//virtual void changeAnimation(int newState, bool resetFrameToBegining) = 0; //abstract function

		//HELP FUNCTIONS
		//static float distanceBetweenTwoRects(SDL_Rect& r1, SDL_Rect& r2);
		static float distanceBetweenTwoEntities(NrdEntity* e1, NrdEntity* e2);
		static float angleBetweenTwoEntities(NrdEntity* e1, NrdEntity* e2);
		static float distanceBetweenTwoPoints(float cx1, float cy1, float cx2, float cy2);
		static int angleToDirection(float angle);
		static float angleBetweenTwoPoints(float cx1, float cy1, float cx2, float cy2);
		//static float angleBetweenTwoRects(SDL_Rect& r1, SDL_Rect& r2);

		static bool EntityCompare(const NrdEntity* const& a, const NrdEntity* const& b);// compare 2 entities in a list to help sorting (sorts based on y value)
		static void removeInactiveEntitiesFromList(std::list<NrdEntity*>* entityList, bool deleteEntities);
		static void removeAllFromList(std::list<NrdEntity*>* entityList, bool deleteEntities);
	private:
	};
}
