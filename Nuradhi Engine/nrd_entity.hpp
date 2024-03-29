#pragma once
#include "lve_game_object.hpp"
#include "nrd_globals.hpp"
#include "nrd_time_controller.hpp"
#include "nrd_sweptAABB.hpp"
namespace nrd {
	class NrdEntity : public lve::LveGameObject {
	public:
		nrd::NrdSweptAABB collider;
		float xMin, xMax, yMin, yMax, zMin, zMax;
		//reference constants
		static const int DIR_UP, DIR_DOWN, DIR_LEFT, DIR_RIGHT, DIR_NONE;
		//quick label to see what the entity is up to
		int state;

		float x, y;
		int direction;
		bool solid = true; //is this thing solid, can things pass through me
		bool collideWithSolids = true; //sometimes we are solid but I pass through other solids
		bool dieOnSolids = false;//useful if you need entities to die on touching solids(e.g bullet hit wall)
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

		//SDL_Rect collisionBox; //box describing the size of our entity and this is sused to bump into things
		//SDL_Rect lastCollisionBox; //where our collisionbox was last
		int collisionBoxW, collisionBoxH; //our default collisionBox siez
		float collisionBoxYOffset; //from my entities y value, where should I sit this collisionBox 

		//AnimationSet* animSet; //set of all animations this entity can have
		//Animation* currentAnim; //current animation the entity is using
		//Frame* currentFrame; //the current frame in the above animation the entity is using
		//float frameTimer; //helps animate frame to frame

		//VIRTUAL FUNCTIONS
		virtual void update();
		virtual void move(float angle);
		virtual void updateMovement();
		virtual void updateCollisionBox();

		//virtual void changeAnimation(int newState, bool resetFrameToBegining) = 0; //abstract function
		virtual void updateCollisions(); //how we bump into stuff in the world
		virtual void crashOntoSolid() { ; }//if dieonSolids is true, then this function will deal with dying

		//HELP FUNCTIONS
		//static float SweptAABB(SDL_Rect movingBox, float vx, float vy, SDL_Rect otherBox, float& normalX, float& normalY);
		//static float distanceBetweenTwoRects(SDL_Rect& r1, SDL_Rect& r2);
		static float distanceBetweenTwoEntities(NrdEntity* e1, NrdEntity* e2);
		static float angleBetweenTwoEntities(NrdEntity* e1, NrdEntity* e2);
		static float distanceBetweenTwoPoints(float cx1, float cy1, float cx2, float cy2);
		//static bool checkCollision(SDL_Rect cbox1, SDL_Rect cbox2);
		static int angleToDirection(float angle);
		static float angleBetweenTwoPoints(float cx1, float cy1, float cx2, float cy2);
		//static float angleBetweenTwoRects(SDL_Rect& r1, SDL_Rect& r2);

		//global entities list I can refer to at any time
		static std::list<NrdEntity*> entities;
		static bool EntityCompare(const NrdEntity* const& a, const NrdEntity* const& b);// compare 2 entities in a list to help sorting (sorts based on y value)
		static void removeInactiveEntitiesFromList(std::list<NrdEntity*>* entityList, bool deleteEntities);
		static void removeAllFromList(std::list<NrdEntity*>* entityList, bool deleteEntities);
		//virtual void draw(lve::FrameInfo& frameInfo, VkPipelineLayout& pipelineLayout) override;
		NrdEntity(lve::LveDevice& lveDevice,float xMin,float xMax, float yMin,float yMax,float zMin,float zMax);
		void destroyDebugLine() { debugline = nullptr; }
		std::unique_ptr<nrd::NrdDebugLine> debugline = nullptr;
	private:
	};
}