#include "engine/render/fog_render_system.hpp"
#include "engine/core/components.hpp"
#include "engine/fog/fog_simulation.hpp"
#include "engine/fog/fog_volume.hpp"

//libs
#include <glm/gtc/packing.hpp>

//std
#include <algorithm>
#include <stdexcept>
#include <vector>

namespace nrd {

	namespace {
		//matches the push constant block in fog.vert and fog.frag
		struct FogPushConstants {
			glm::vec4 centre; //xyz ground, w time
			glm::vec4 extents; //half width, half depth, height, slices
			glm::vec4 colour; //rgb, a density
			glm::vec4 shape; //coverage, height falloff, noise scale, turbulence
			glm::vec4 motion; //wind x, wind z, shadow, light scatter
			glm::vec4 style; //tone bands, halftone, halftone size
			glm::vec4 gridOrigin; //xyz the simulation grid's corner at the ground, w 1 when interactive
			glm::vec4 gridSize; //xyz the grid's size (x, height, z)
		};
		static_assert(sizeof(FogPushConstants) <= 128, "push constants are only guaranteed 128 bytes");

		constexpr VkFormat VOLUME_FORMAT = VK_FORMAT_R16G16B16A16_SFLOAT; //density, displacement x, y, z
		constexpr VkDeviceSize TEXEL_BYTES = 4 * sizeof(std::uint16_t);
	}

	FogRenderSystem::FogRenderSystem(lve::LveDevice& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout)
		: lveDevice{ device }
	{
		simulationSetLayout = lve::LveDescriptorSetLayout::Builder(lveDevice)
			.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
			.build();
		descriptorPool = lve::LveDescriptorPool::Builder(lveDevice)
			.setMaxSets(MAX_INTERACTIVE_VOLUMES * FRAMES + 1)
			.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_INTERACTIVE_VOLUMES * FRAMES + 1)
			.build();

		VkSamplerCreateInfo samplerInfo{};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = VK_FILTER_LINEAR; //the grid is coarse; filtering smooths it into the fog
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.maxLod = 0.f;
		if (vkCreateSampler(lveDevice.device(), &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create fog simulation sampler");
		}

		//the undisturbed texture is uploaded once and never changes
		undisturbed = createVolumeTexture(1, 1, 1);
		const std::uint16_t texel[4] = { glm::packHalf1x16(1.f), 0, 0, 0 };
		undisturbed.staging->writeToBuffer(const_cast<std::uint16_t*>(texel), sizeof(texel));
		VkCommandBuffer commandBuffer = lveDevice.beginSingleTimeCommands();
		recordUpload(commandBuffer, undisturbed, 1, 1, 1);
		lveDevice.endSingleTimeCommands(commandBuffer);

		createPipelineLayout(globalSetLayout);
		createPipeline(renderPass);
	}

	FogRenderSystem::~FogRenderSystem()
	{
		for (auto& [entity, frames] : volumes) {
			for (VolumeTexture& texture : frames) destroyVolumeTexture(texture);
		}
		destroyVolumeTexture(undisturbed);
		vkDestroySampler(lveDevice.device(), sampler, nullptr);
		vkDestroyPipelineLayout(lveDevice.device(), pipelineLayout, nullptr);
	}

	void FogRenderSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout)
	{
		VkPushConstantRange pushConstantRange{};
		pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		pushConstantRange.offset = 0;
		pushConstantRange.size = sizeof(FogPushConstants);

		const VkDescriptorSetLayout setLayouts[2] = { globalSetLayout, simulationSetLayout->getDescriptorSetLayout() };
		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = 2;
		pipelineLayoutInfo.pSetLayouts = setLayouts;
		pipelineLayoutInfo.pushConstantRangeCount = 1;
		pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
		if (vkCreatePipelineLayout(lveDevice.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create fog pipeline layout");
		}
	}

	void FogRenderSystem::createPipeline(VkRenderPass renderPass)
	{
		lve::PipelineConfigInfo config{};
		lve::LvePipeline::defaultPipelineConfigInfo(config);
		lve::LvePipeline::enableAlphaBlending(config);
		config.depthStencilInfo.depthWriteEnable = VK_FALSE; //tested against the scene, so it wraps around whoever stands in it
		config.rasterizationInfo.cullMode = VK_CULL_MODE_NONE; //slices are seen from above or below
		config.bindingDescription.clear(); //no vertex buffer: the slices come from gl_VertexIndex and gl_InstanceIndex
		config.attributeDescription.clear();
		config.renderPass = renderPass;
		config.pipelineLayout = pipelineLayout;
		pipeline = std::make_unique<lve::LvePipeline>(lveDevice, "shaders/fog.vert.spv", "shaders/fog.frag.spv", config);
	}

	FogRenderSystem::VolumeTexture FogRenderSystem::createVolumeTexture(int width, int height, int depth)
	{
		VolumeTexture texture{};
		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_3D;
		imageInfo.format = VOLUME_FORMAT;
		imageInfo.extent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height), static_cast<uint32_t>(depth) };
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		lveDevice.createImageWithInfo(imageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, texture.image, texture.memory);

		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = texture.image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
		viewInfo.format = VOLUME_FORMAT;
		viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		if (vkCreateImageView(lveDevice.device(), &viewInfo, nullptr, &texture.view) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create fog simulation image view");
		}

		texture.staging = std::make_unique<lve::LveBuffer>(
			lveDevice,
			TEXEL_BYTES,
			static_cast<uint32_t>(width * height * depth),
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		texture.staging->map();

		VkDescriptorImageInfo descriptorInfo{ sampler, texture.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
		if (!lve::LveDescriptorWriter(*simulationSetLayout, *descriptorPool).writeImage(0, &descriptorInfo).build(texture.descriptorSet)) {
			throw std::runtime_error("Too many interactive fog volumes: raise FogRenderSystem::MAX_INTERACTIVE_VOLUMES");
		}
		return texture;
	}

	void FogRenderSystem::destroyVolumeTexture(VolumeTexture& texture)
	{
		//the descriptor sets go with the pool
		texture.staging.reset();
		vkDestroyImageView(lveDevice.device(), texture.view, nullptr);
		vkDestroyImage(lveDevice.device(), texture.image, nullptr);
		vkFreeMemory(lveDevice.device(), texture.memory, nullptr);
	}

	void FogRenderSystem::recordUpload(VkCommandBuffer commandBuffer, VolumeTexture& texture, int width, int height, int depth)
	{
		//the whole image is rewritten, so its old contents can be discarded
		VkImageMemoryBarrier toTransfer{};
		toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		toTransfer.srcAccessMask = 0;
		toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		toTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		toTransfer.image = texture.image;
		toTransfer.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toTransfer);

		VkBufferImageCopy region{};
		region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
		region.imageExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height), static_cast<uint32_t>(depth) };
		vkCmdCopyBufferToImage(commandBuffer, texture.staging->getBuffer(), texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

		VkImageMemoryBarrier toShader = toTransfer;
		toShader.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		toShader.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		toShader.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		toShader.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toShader);
	}

	void FogRenderSystem::prepare(lve::FrameInfo& frameInfo)
	{
		frameInfo.registry.each<FogVolume>([&](ecs::Entity entity, FogVolume& fog) {
			if (!fog.enabled || !fog.interactive || !fog.simulation) return;
			auto found = volumes.find(entity);
			if (found == volumes.end()) {
				std::array<VolumeTexture, FRAMES> frames;
				for (VolumeTexture& texture : frames) texture = createVolumeTexture(FogSimulation::NX, FogSimulation::NY, FogSimulation::NZ);
				found = volumes.emplace(entity, std::move(frames)).first;
			}
			//this frame's texture: the GPU finished with it when the frame's fence was waited on
			VolumeTexture& texture = found->second[frameInfo.frameIndex];
			const std::vector<std::uint16_t>& texels = fog.simulation->texels();
			texture.staging->writeToBuffer(const_cast<std::uint16_t*>(texels.data()), texels.size() * sizeof(std::uint16_t));
			recordUpload(frameInfo.commandBuffer, texture, FogSimulation::NX, FogSimulation::NY, FogSimulation::NZ);
		});
	}

	void FogRenderSystem::render(lve::FrameInfo& frameInfo)
	{
		const glm::vec3 cameraPosition = glm::vec3(frameInfo.camera.getInverseView()[3]);
		const glm::vec3 cameraForward = glm::normalize(glm::vec3(frameInfo.camera.getInverseView()[2]));

		//volumes are transparent too: the farthest first
		struct Draw {
			float depth;
			FogPushConstants push;
			VkDescriptorSet simulation;
		};
		std::vector<Draw> draws;
		frameInfo.registry.each<FogVolume, Transform>([&](ecs::Entity entity, FogVolume& fog, Transform& transform) {
			if (!fog.enabled || fog.slices <= 0 || fog.height <= 0.f) return;
			FogPushConstants push{};
			push.centre = glm::vec4(transform.position, fog.time);
			push.extents = { fog.halfExtents.x, fog.halfExtents.y, fog.height, static_cast<float>(fog.slices) };
			push.colour = glm::vec4(fog.colour, fog.density);
			push.shape = { fog.coverage, fog.heightFalloff, fog.noiseScale, fog.turbulence };
			push.motion = { fog.wind.x, fog.wind.y, fog.shadow, fog.lightScatter };
			push.style = { static_cast<float>(std::max(fog.toneBands, 0)), fog.halftone, std::max(fog.halftoneSize, 2.f), 0.f };

			VkDescriptorSet simulation = undisturbed.descriptorSet;
			const auto found = volumes.find(entity);
			if (fog.interactive && fog.simulation && found != volumes.end()) {
				simulation = found->second[frameInfo.frameIndex].descriptorSet;
				push.gridOrigin = glm::vec4(fog.simulation->origin(), 1.f);
				push.gridSize = glm::vec4(fog.simulation->size(), 0.f);
			}
			draws.push_back({ glm::dot(transform.position - cameraPosition, cameraForward), push, simulation });
		});
		if (draws.empty()) return;
		std::sort(draws.begin(), draws.end(), [](const Draw& a, const Draw& b) { return a.depth > b.depth; });

		pipeline->bind(frameInfo.commandBuffer);
		vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &frameInfo.globalDescriptorSet, 0, nullptr);
		for (const Draw& draw : draws) {
			vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &draw.simulation, 0, nullptr);
			vkCmdPushConstants(frameInfo.commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(FogPushConstants), &draw.push);
			vkCmdDraw(frameInfo.commandBuffer, 6, static_cast<uint32_t>(draw.push.extents.w), 0, 0);
		}
	}
}
