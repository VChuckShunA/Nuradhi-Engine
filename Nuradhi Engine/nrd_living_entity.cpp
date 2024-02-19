#include "nrd_living_entity.hpp"

namespace nrd {

	void NrdLivingEntity::updateHitBox()
	{
	}

	void NrdLivingEntity::updateInvincibleTimer() {
		if (invincibleTimer > 0) {
			invincibleTimer -= nrd::NrdTimeController::nrdTimeController.dT;
		}
	}

	void NrdLivingEntity::draw()
	{
	}

	NrdLivingEntity::NrdLivingEntity()
	{
	}

}