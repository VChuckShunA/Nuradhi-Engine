#include "engine/render/post_process_system.hpp"

//libs
#include <glm/glm.hpp>

//std
#include <stdexcept>

namespace nrd {

	namespace {
		//matches the push constant block in post.frag
		struct PostPushConstants {
			glm::vec4 noir; //x enabled, y contrast, z brightness, w grain
			glm::vec4 noirLook; //x vignette, y keep red, z time, w keep blue
			glm::vec4 crt; //x enabled, y curvature, z scanlines, w mask
			glm::vec4 crtLook; //x aberration (pixels), y flicker, z vignette, w scanline size (pixels)
			glm::vec4 screen; //xy size in pixels
		};
		static_assert(sizeof(PostPushConstants) <= 128, "push constants are only guaranteed 128 bytes");
	}

	PostProcessSystem::PostProcessSystem(lve::LveDevice& device, VkRenderPass swapchainRenderPass, VkFormat colorFormat, VkFormat depthFormat)
		: lveDevice{ device }, colorFormat{ colorFormat }, depthFormat{ depthFormat }
	{
		createRenderPass();

		VkSamplerCreateInfo samplerInfo{};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = VK_FILTER_LINEAR; //the CRT's curve samples between pixels
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.maxLod = 0.f;
		if (vkCreateSampler(lveDevice.device(), &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create post-process sampler");
		}

		setLayout = lve::LveDescriptorSetLayout::Builder(lveDevice)
			.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
			.build();
		descriptorPool = lve::LveDescriptorPool::Builder(lveDevice)
			.setMaxSets(FRAMES)
			.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, FRAMES)
			.build();
		createPipeline(swapchainRenderPass);
	}

	PostProcessSystem::~PostProcessSystem()
	{
		destroyTargets();
		vkDestroyPipelineLayout(lveDevice.device(), pipelineLayout, nullptr);
		vkDestroySampler(lveDevice.device(), sampler, nullptr);
		vkDestroyRenderPass(lveDevice.device(), sceneRenderPass, nullptr);
	}

	void PostProcessSystem::createRenderPass()
	{
		//the swapchain's render pass, except the colour is kept for sampling instead of being presented
		VkAttachmentDescription colorAttachment{};
		colorAttachment.format = colorFormat;
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; //endScenePass makes it readable

		VkAttachmentDescription depthAttachment{};
		depthAttachment.format = depthFormat;
		depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentReference colorReference{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
		VkAttachmentReference depthReference{ 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorReference;
		subpass.pDepthStencilAttachment = &depthReference;

		//exactly the swapchain pass's dependency: render passes are only compatible (so the scene's pipelines, built
		//for the swapchain pass, can draw in this one) when their dependencies match. Handing the colour to the
		//filter pass is an explicit barrier in endScenePass instead.
		VkSubpassDependency dependency{};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		const VkAttachmentDescription attachments[2] = { colorAttachment, depthAttachment };
		VkRenderPassCreateInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = 2;
		renderPassInfo.pAttachments = attachments;
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &dependency;
		if (vkCreateRenderPass(lveDevice.device(), &renderPassInfo, nullptr, &sceneRenderPass) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create the post-process scene render pass");
		}
	}

	void PostProcessSystem::createPipeline(VkRenderPass swapchainRenderPass)
	{
		VkPushConstantRange pushConstantRange{};
		pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		pushConstantRange.offset = 0;
		pushConstantRange.size = sizeof(PostPushConstants);

		const VkDescriptorSetLayout layout = setLayout->getDescriptorSetLayout();
		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = 1;
		pipelineLayoutInfo.pSetLayouts = &layout;
		pipelineLayoutInfo.pushConstantRangeCount = 1;
		pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
		if (vkCreatePipelineLayout(lveDevice.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create post-process pipeline layout");
		}

		lve::PipelineConfigInfo config{};
		lve::LvePipeline::defaultPipelineConfigInfo(config);
		config.depthStencilInfo.depthTestEnable = VK_FALSE; //it covers the whole screen, whatever the depth buffer says
		config.depthStencilInfo.depthWriteEnable = VK_FALSE;
		config.rasterizationInfo.cullMode = VK_CULL_MODE_NONE;
		config.bindingDescription.clear(); //a full-screen triangle from gl_VertexIndex
		config.attributeDescription.clear();
		config.renderPass = swapchainRenderPass;
		config.pipelineLayout = pipelineLayout;
		pipeline = std::make_unique<lve::LvePipeline>(lveDevice, "shaders/post.vert.spv", "shaders/post.frag.spv", config);
	}

	void PostProcessSystem::createTargets()
	{
		for (Target& target : targets) {
			const auto createImage = [&](VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspect, VkImage& image, VkDeviceMemory& memory, VkImageView& view) {
				VkImageCreateInfo imageInfo{};
				imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
				imageInfo.imageType = VK_IMAGE_TYPE_2D;
				imageInfo.format = format;
				imageInfo.extent = { extent.width, extent.height, 1 };
				imageInfo.mipLevels = 1;
				imageInfo.arrayLayers = 1;
				imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
				imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
				imageInfo.usage = usage;
				imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
				imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				lveDevice.createImageWithInfo(imageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, image, memory);

				VkImageViewCreateInfo viewInfo{};
				viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
				viewInfo.image = image;
				viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
				viewInfo.format = format;
				viewInfo.subresourceRange = { aspect, 0, 1, 0, 1 };
				if (vkCreateImageView(lveDevice.device(), &viewInfo, nullptr, &view) != VK_SUCCESS) {
					throw std::runtime_error("Failed to create a post-process image view");
				}
			};
			createImage(colorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT,
				target.colorImage, target.colorMemory, target.colorView);
			createImage(depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_ASPECT_DEPTH_BIT,
				target.depthImage, target.depthMemory, target.depthView);

			const VkImageView attachments[2] = { target.colorView, target.depthView };
			VkFramebufferCreateInfo framebufferInfo{};
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = sceneRenderPass;
			framebufferInfo.attachmentCount = 2;
			framebufferInfo.pAttachments = attachments;
			framebufferInfo.width = extent.width;
			framebufferInfo.height = extent.height;
			framebufferInfo.layers = 1;
			if (vkCreateFramebuffer(lveDevice.device(), &framebufferInfo, nullptr, &target.framebuffer) != VK_SUCCESS) {
				throw std::runtime_error("Failed to create a post-process framebuffer");
			}

			VkDescriptorImageInfo imageInfo{ sampler, target.colorView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
			lve::LveDescriptorWriter writer(*setLayout, *descriptorPool);
			writer.writeImage(0, &imageInfo);
			if (target.descriptorSet == VK_NULL_HANDLE) {
				if (!writer.build(target.descriptorSet)) throw std::runtime_error("Failed to allocate a post-process descriptor set");
			}
			else {
				writer.overwrite(target.descriptorSet); //the sets outlive a resize; only the image they point at changes
			}
		}
	}

	void PostProcessSystem::destroyTargets()
	{
		for (Target& target : targets) {
			if (target.framebuffer == VK_NULL_HANDLE) continue;
			vkDestroyFramebuffer(lveDevice.device(), target.framebuffer, nullptr);
			vkDestroyImageView(lveDevice.device(), target.colorView, nullptr);
			vkDestroyImage(lveDevice.device(), target.colorImage, nullptr);
			vkFreeMemory(lveDevice.device(), target.colorMemory, nullptr);
			vkDestroyImageView(lveDevice.device(), target.depthView, nullptr);
			vkDestroyImage(lveDevice.device(), target.depthImage, nullptr);
			vkFreeMemory(lveDevice.device(), target.depthMemory, nullptr);
			const VkDescriptorSet keep = target.descriptorSet;
			target = Target{};
			target.descriptorSet = keep;
		}
	}

	void PostProcessSystem::ensureTargets(VkExtent2D newExtent)
	{
		if (newExtent.width == extent.width && newExtent.height == extent.height && targets[0].framebuffer != VK_NULL_HANDLE) return;
		vkDeviceWaitIdle(lveDevice.device()); //the old images may still be in use by a frame in flight
		destroyTargets();
		extent = newExtent;
		createTargets();
	}

	void PostProcessSystem::beginScenePass(VkCommandBuffer commandBuffer, int frameIndex)
	{
		VkClearValue clearValues[2]{};
		clearValues[0].color = { 0.01f, 0.01f, 0.01f, 1.f }; //the swapchain pass's clear colour
		clearValues[1].depthStencil = { 1.f, 0 };
		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = sceneRenderPass;
		renderPassInfo.framebuffer = targets[frameIndex].framebuffer;
		renderPassInfo.renderArea = { { 0, 0 }, extent };
		renderPassInfo.clearValueCount = 2;
		renderPassInfo.pClearValues = clearValues;
		vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
	}

	void PostProcessSystem::endScenePass(VkCommandBuffer commandBuffer, int frameIndex)
	{
		vkCmdEndRenderPass(commandBuffer);

		//the scene is finished before the filter pass samples it
		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = targets[frameIndex].colorImage;
		barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
	}

	void PostProcessSystem::draw(VkCommandBuffer commandBuffer, int frameIndex, const PostProcessSettings& settings, float time)
	{
		const FilmNoirFilter& noir = settings.noir;
		const CrtFilter& crt = settings.crt;
		PostPushConstants push{};
		push.noir = { noir.enabled ? 1.f : 0.f, noir.contrast, noir.brightness, noir.grain };
		push.noirLook = { noir.vignette, noir.keepRed, time, noir.keepBlue };
		push.crt = { crt.enabled ? 1.f : 0.f, crt.curvature, crt.scanlines, crt.mask };
		push.crtLook = { crt.aberration, crt.flicker, crt.vignette, crt.scanlineSize > 1.f ? crt.scanlineSize : 1.f };
		push.screen = { static_cast<float>(extent.width), static_cast<float>(extent.height), 0.f, 0.f };

		pipeline->bind(commandBuffer);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &targets[frameIndex].descriptorSet, 0, nullptr);
		vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PostPushConstants), &push);
		vkCmdDraw(commandBuffer, 3, 1, 0, 0);
	}
}
