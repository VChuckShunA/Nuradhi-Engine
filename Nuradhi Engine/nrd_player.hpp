#pragma once
#include "nrd_living_entity.hpp"
#include "nrd_movement_controller.hpp"
#include "nrd_debugLine.hpp"
#include "nrd_sweptAABB.hpp"
namespace nrd {
	class NrdPlayer :public nrd::NrdLivingEntity {

	public:
		nrd::NrdSweptAABB collider;
		//reference variabls
		static const std::string HERO_ANIM_UP;
		static const std::string HERO_ANIM_DOWN;
		static const std::string HERO_ANIM_LEFT;
		static const std::string HERO_ANIM_RIGHT;
		static const std::string HERO_ANIM_IDLE_UP;
		static const std::string HERO_ANIM_IDLE_DOWN;
		static const std::string HERO_ANIM_IDLE_LEFT;
		static const std::string HERO_ANIM_IDLE_RIGHT;
		static const std::string HERO_SLASH_ANIM_UP;
		static const std::string HERO_SLASH_ANIM_DOWN;
		static const std::string HERO_SLASH_ANIM_LEFT;
		static const std::string HERO_SLASH_ANIM_RIGHT;
		static const std::string HERO_DASH_ANIM_UP;
		static const std::string HERO_DASH_ANIM_DOWN;
		static const std::string HERO_DASH_ANIM_LEFT;
		static const std::string HERO_DASH_ANIM_RIGHT;
		static const std::string HERO_ANIM_DIE;

		static const int HERO_STATE_IDLE;
		static const int HERO_STATE_MOVE;
		static const int HERO_STATE_SLASH;
		static const int HERO_STATE_DASH;
		static const int HERO_STATE_DEAD;
		//Collision
		//NrdPlayer(AnimationSet* animSet);
		NrdPlayer(lve::LveDevice& lveDevice);//Temporary Constructor
		virtual void draw(lve::FrameInfo& frameInfo, VkPipelineLayout& pipelineLayout) override;
		void update();
		void slash();
		void dash();
		void die();
		void revive();
		void changeAnimation(int newState, bool resetFrameToBegining);
		void updateAnimation();
		void updateDamage();
		void destroyDebugLine() { debugline = nullptr; }
		private:
			std::unique_ptr<nrd::NrdDebugLine> debugline=nullptr;
	};
}