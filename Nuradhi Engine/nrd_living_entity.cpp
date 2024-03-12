#include "nrd_living_entity.hpp"

namespace nrd {

	/*void NrdLivingEntity::updateHitBox()
	{
	}

	void NrdLivingEntity::updateInvincibleTimer() {
		if (invincibleTimer > 0) {
			invincibleTimer -= nrd::NrdTimeController::nrdTimeController.dT;
		}
	}
	*/
	

	

	NrdLivingEntity::NrdLivingEntity(lve::LveDevice& lveDevice, float xMin, float xMax, float yMin, float yMax, float zMin, float zMax) :
		NrdEntity(lveDevice, xMin, xMax, yMin, yMax, zMin, zMax)
	{
	}

}