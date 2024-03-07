#pragma once
#include "lve_device.hpp"
#include "nrd_living_entity.hpp"
#include "nrd_debugLine.hpp"
#include "nrd_sweptAABB.hpp"
namespace nrd {

	class NrdSolidObject :public nrd::NrdLivingEntity {
	public:
		NrdSolidObject(lve::LveDevice& lveDevice); 
		virtual void draw(lve::FrameInfo& frameInfo, VkPipelineLayout& pipelineLayout) override;
		void destroyDebugLine() { debugline = nullptr; }
	private:
		std::unique_ptr<nrd::NrdDebugLine> debugline = nullptr;
		
	};
}