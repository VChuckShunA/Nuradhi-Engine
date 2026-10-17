#include "nrd_solidObject.hpp"

namespace nrd {

	NrdSolidObject::NrdSolidObject(const Collider& collider)
	{
		this->collider = collider;
		this->collider.isStatic = true;
	}

}
