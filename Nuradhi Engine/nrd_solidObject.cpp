#include "nrd_solidObject.hpp"

namespace nrd {

	NrdSolidObject::NrdSolidObject(lve::LveDevice& lveDevice)
		: NrdLivingEntity(lveDevice, -.25, .25, -.25, .25, 0, -.5)
	{
		//nrd::NrdSweptAABB collider(lveDevice, -.25,.25, -.25, .25, 0, -.5);
		//nrd::NrdDebugLine::Builder debugLineBuilder{};
		//debugLineBuilder = collider.getCollider();
		

		//debugline = std::make_unique<NrdDebugLine>(lveDevice, debugLineBuilder);
	}

	void NrdSolidObject::draw(lve::FrameInfo& frameInfo, VkPipelineLayout& pipelineLayout)
	{
		lve::LveGameObject::draw(frameInfo, pipelineLayout);
		//draw debug lines here
		debugline->bind(frameInfo.commandBuffer);
		debugline->draw(frameInfo.commandBuffer);
	}

}