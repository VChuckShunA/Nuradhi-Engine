#pragma once
#include "nrd_living_entity.hpp"
namespace nrd {

	//Static level geometry the player can stand on and bump into
	class NrdSolidObject :public nrd::NrdLivingEntity {
	public:
		explicit NrdSolidObject(const Collider& collider);
	};
}
