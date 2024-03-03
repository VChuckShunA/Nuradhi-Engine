#pragma once
#include "nrd_entity.hpp"

namespace nrd {
	class NrdLivingEntity : public nrd::NrdEntity {

	public:

		int hp, hpMax;
		int damage = 0;
		//SDL_Rect hitBox;//similar to collisionBox, but describes damaging area

		float invincibleTimer = 0; // if >0, then can't hurt. Used to stope the player from spamming attacks, since the fps depends on the pc

		virtual void updateHitBox();
		virtual void updateDamage() = 0;//how we get damaged by other things (up to each class to define)
		virtual void die() = 0;//abstract. up to other classes to decided how they die
		virtual void updateInvincibleTimer();
		virtual void hitLanded(NrdLivingEntity* entity) { ; }

		
		NrdLivingEntity();
	};

}
