#include "engine/render/renderer.hpp"
#include "engine/core/components.hpp"
#include "engine/render/debug_render_system.hpp"
#include "engine/render/mesh_render_system.hpp"
#include "engine/render/particle_render_system.hpp"
#include "engine/render/point_light_system.hpp"
#include "engine/render/vulkan/lve_buffer.hpp"
#include "engine/render/vulkan/lve_camera.hpp"
#include "engine/render/vulkan/lve_descriptors.hpp"
#include "engine/render/vulkan/lve_device.hpp"
#include "engine/render/vulkan/lve_frame_info.hpp"
#include "engine/render/vulkan/lve_model.hpp"
#include "engine/render/vulkan/lve_renderer.hpp"
#include "engine/render/vulkan/lve_swap_chain.hpp"
#include "engine/render/vulkan/lve_texture.hpp"

//libs
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

//std
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace nrd {

	namespace {
		constexpr int FRAMES = lve::LveSwapChain::MAX_FRAMES_IN_FLIGHT;

		//Unit quad in the XY plane facing the camera (-z), with its bottom-centre at the origin, so it stands up from
		//wherever it's placed. Its UVs put the top of the image at the top (-y is up).
		std::unique_ptr<lve::LveModel> createSpriteQuadModel(lve::LveDevice& device)
		{
			const glm::vec3 white{ 1.f };
			const glm::vec3 towardsCamera{ 0.f, 0.f, -1.f };
			lve::LveModel::Builder builder{};
			builder.vertices = {
				{ { -0.5f, -1.f, 0.f }, white, towardsCamera, { 0.f, 0.f } }, //top left
				{ {  0.5f, -1.f, 0.f }, white, towardsCamera, { 1.f, 0.f } }, //top right
				{ {  0.5f,  0.f, 0.f }, white, towardsCamera, { 1.f, 1.f } }, //bottom right
				{ { -0.5f,  0.f, 0.f }, white, towardsCamera, { 0.f, 1.f } }, //bottom left
			};
			builder.indices = { 0, 1, 2, 2, 3, 0 };
			return std::make_unique<lve::LveModel>(device, builder);
		}

		void checkImGuiVulkanResult(VkResult result)
		{
			if (result < 0) throw std::runtime_error("Dear ImGui Vulkan call failed: " + std::to_string(result));
		}
	}

	struct Renderer::Impl {
		explicit Impl(lve::LveWindow& window) : device{ window }, swapchain{ window, device } {}

		//declaration order is destruction order in reverse: everything below the device is destroyed before it
		lve::LveDevice device;
		lve::LveRenderer swapchain;
		std::unique_ptr<lve::LveDescriptorPool> descriptorPool;
		std::unique_ptr<lve::LveDescriptorSetLayout> globalSetLayout;
		std::vector<std::unique_ptr<lve::LveBuffer>> uboBuffers; //one per frame in flight
		std::vector<std::unique_ptr<lve::Texture>> textures; //indexed by TextureHandle
		std::vector<std::vector<VkDescriptorSet>> descriptorSets; //[frame][texture]: that frame's UBO plus the texture
		std::vector<std::shared_ptr<lve::LveModel>> meshes; //indexed by MeshHandle
		std::unordered_map<std::string, MeshHandle> meshesByPath;
		MeshHandle spriteQuad{};
		std::unique_ptr<MeshRenderSystem> meshSystem;
		std::unique_ptr<PointLightSystem> lightSystem;
		std::unique_ptr<ParticleRenderSystem> particleSystem;
		std::unique_ptr<DebugRenderSystem> debugSystem;
		lve::LveCamera camera;
		bool debugOverlay = true;
		std::vector<Aabb> debugBoxes; //queued by drawDebugBox, cleared every frame

		TextureHandle addTexture(std::unique_ptr<lve::Texture> texture)
		{
			if (textures.size() >= MAX_TEXTURES) {
				throw std::runtime_error("Too many textures: raise Renderer::MAX_TEXTURES");
			}
			const TextureHandle handle{ static_cast<std::uint32_t>(textures.size()) };
			VkDescriptorImageInfo imageInfo{ texture->getSampler(), texture->getImageView(), texture->getImageLayout() };
			for (int frame = 0; frame < FRAMES; frame++) {
				auto bufferInfo = uboBuffers[frame]->descriptorInfo();
				VkDescriptorSet set{};
				const bool built = lve::LveDescriptorWriter(*globalSetLayout, *descriptorPool)
					.writeBuffer(0, &bufferInfo)
					.writeImage(1, &imageInfo)
					.build(set);
				if (!built) {
					throw std::runtime_error("Failed to allocate global descriptor set");
				}
				descriptorSets[frame].push_back(set);
			}
			textures.push_back(std::move(texture));
			return handle;
		}

		MeshHandle addMesh(std::shared_ptr<lve::LveModel> mesh)
		{
			meshes.push_back(std::move(mesh));
			return MeshHandle{ static_cast<std::uint32_t>(meshes.size() - 1) };
		}

		void updateCamera(ecs::Registry& registry)
		{
			const ecs::Entity cameraEntity = registry.findFirst<Transform, Camera>();
			if (cameraEntity == ecs::NullEntity) return;

			const Transform& transform = registry.get<Transform>(cameraEntity);
			const Camera& settings = registry.get<Camera>(cameraEntity);
			const float aspect = swapchain.getAspectRatio();
			if (settings.projection == Camera::Projection::Orthographic) {
				const float halfHeight = settings.orthographicHeight * 0.5f;
				camera.setOrthographicProjection(-aspect * halfHeight, aspect * halfHeight, -halfHeight, halfHeight, settings.nearPlane, settings.farPlane);
			}
			else {
				camera.setPerspectiveProjection(settings.fovY, aspect, settings.nearPlane, settings.farPlane);
			}
			camera.setViewYXZ(transform.position, transform.rotation);
		}
	};

	Renderer::Renderer(lve::LveWindow& window) : impl{ std::make_unique<Impl>(window) }
	{
		Impl& r = *impl;
		r.descriptorPool = lve::LveDescriptorPool::Builder(r.device)
			.setMaxSets(FRAMES * MAX_TEXTURES)
			.addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, FRAMES * MAX_TEXTURES)
			.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, FRAMES * MAX_TEXTURES)
			.build();
		r.globalSetLayout = lve::LveDescriptorSetLayout::Builder(r.device)
			.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
			.addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
			.build();

		r.uboBuffers.resize(FRAMES);
		for (auto& buffer : r.uboBuffers) {
			buffer = std::make_unique<lve::LveBuffer>(
				r.device,
				sizeof(lve::GlobalUbo),
				1,
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
			buffer->map();
		}
		r.descriptorSets.resize(FRAMES);

		const VkRenderPass renderPass = r.swapchain.getSwapChainRenderPass();
		const VkDescriptorSetLayout setLayout = r.globalSetLayout->getDescriptorSetLayout();
		r.meshSystem = std::make_unique<MeshRenderSystem>(r.device, renderPass, setLayout);
		r.lightSystem = std::make_unique<PointLightSystem>(r.device, renderPass, setLayout);
		r.particleSystem = std::make_unique<ParticleRenderSystem>(r.device, renderPass, setLayout);
		r.debugSystem = std::make_unique<DebugRenderSystem>(r.device);

		//texture 0: plain white, for meshes that don't name a texture
		const std::array<unsigned char, 4> white{ 255, 255, 255, 255 };
		r.addTexture(std::make_unique<lve::Texture>(r.device, white.data(), 1, 1));
		r.spriteQuad = r.addMesh(createSpriteQuadModel(r.device));

		//Dear ImGui, drawn in the swapchain's render pass after everything else
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui::GetIO().IniFilename = nullptr; //don't write imgui.ini next to the game
		ImGui::StyleColorsDark();
		//ImGui's colours are sRGB values, but the swapchain is an sRGB format that encodes on write, so convert
		//them to linear first or every window comes out washed out
		for (ImVec4& colour : ImGui::GetStyle().Colors) {
			colour.x = std::pow(colour.x, 2.2f);
			colour.y = std::pow(colour.y, 2.2f);
			colour.z = std::pow(colour.z, 2.2f);
		}
		ImGui_ImplGlfw_InitForVulkan(window.getGLFWwindow(), true); //chains to the window's own callbacks
		ImGui_ImplVulkan_InitInfo imguiInfo{};
		imguiInfo.ApiVersion = VK_API_VERSION_1_0;
		imguiInfo.Instance = r.device.getInstance();
		imguiInfo.PhysicalDevice = r.device.getPhysicalDevice();
		imguiInfo.Device = r.device.device();
		imguiInfo.QueueFamily = r.device.findPhysicalQueueFamilies().graphicsFamily;
		imguiInfo.Queue = r.device.graphicsQueue();
		imguiInfo.DescriptorPoolSize = 16; //the backend makes its own pool for its font and image descriptors
		imguiInfo.MinImageCount = 2;
		imguiInfo.ImageCount = std::max(2u, r.swapchain.getSwapChainImageCount());
		imguiInfo.PipelineInfoMain.RenderPass = r.swapchain.getSwapChainRenderPass();
		imguiInfo.PipelineInfoMain.Subpass = 0;
		imguiInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
		imguiInfo.CheckVkResultFn = checkImGuiVulkanResult;
		if (!ImGui_ImplVulkan_Init(&imguiInfo)) throw std::runtime_error("Failed to initialise Dear ImGui's Vulkan backend");
	}

	Renderer::~Renderer()
	{
		//ImGui's Vulkan objects go before the device does (impl is destroyed after this body)
		waitIdle();
		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
	}

	void Renderer::beginUiFrame()
	{
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
	}

	bool Renderer::uiWantsKeyboard() const { return ImGui::GetIO().WantCaptureKeyboard; }
	bool Renderer::uiWantsMouse() const { return ImGui::GetIO().WantCaptureMouse; }

	TextureHandle Renderer::loadTexture(const std::string& path, TextureFilter filter)
	{
		return impl->addTexture(std::make_unique<lve::Texture>(impl->device, path, filter));
	}

	MeshHandle Renderer::loadMesh(const std::string& path)
	{
		const auto found = impl->meshesByPath.find(path);
		if (found != impl->meshesByPath.end()) return found->second;
		const MeshHandle handle = impl->addMesh(lve::LveModel::createModelFromFile(impl->device, path));
		impl->meshesByPath[path] = handle;
		return handle;
	}

	MeshHandle Renderer::spriteQuad() const { return impl->spriteQuad; }

	void Renderer::setDebugOverlay(bool enabled) { impl->debugOverlay = enabled; }
	bool Renderer::debugOverlay() const { return impl->debugOverlay; }
	void Renderer::drawDebugBox(const Aabb& box) { impl->debugBoxes.push_back(box); }

	float Renderer::aspectRatio() const { return impl->swapchain.getAspectRatio(); }

	void Renderer::cameraMatrices(ecs::Registry& registry, glm::mat4& view, glm::mat4& projection)
	{
		impl->updateCamera(registry);
		view = impl->camera.getView();
		projection = impl->camera.getProjection();
	}

	void Renderer::render(ecs::Registry& registry, float frameTime)
	{
		Impl& r = *impl;
		const std::vector<Aabb> debugBoxes = std::move(r.debugBoxes);
		r.debugBoxes.clear();
		ImGui::Render(); //every UI frame has to end, even if this frame is skipped below
		VkCommandBuffer commandBuffer = r.swapchain.beginFrame();
		if (!commandBuffer) return; //the swapchain was just recreated; draw next frame

		r.updateCamera(registry);
		const int frameIndex = r.swapchain.getFrameIndex();
		lve::FrameInfo frameInfo{
			frameIndex,
			frameTime,
			commandBuffer,
			r.camera,
			r.descriptorSets[frameIndex][0],
			registry,
			r.descriptorSets[frameIndex],
			r.meshes,
			r.spriteQuad.id };

		lve::GlobalUbo ubo{};
		ubo.projection = r.camera.getProjection();
		ubo.view = r.camera.getView();
		ubo.inverseView = r.camera.getInverseView();
		r.lightSystem->update(frameInfo, ubo);
		r.uboBuffers[frameIndex]->writeToBuffer(&ubo);
		r.uboBuffers[frameIndex]->flush();

		r.swapchain.beginSwapChainRenderPass(commandBuffer);
		r.swapchain.bindPipelineEssentials(commandBuffer); //viewport and scissor, shared by every pipeline below
		r.meshSystem->render(frameInfo, false);
		r.particleSystem->render(frameInfo); //transparent: after everything solid
		r.lightSystem->render(frameInfo);
		if (r.debugOverlay) {
			r.meshSystem->render(frameInfo, true);
			r.debugSystem->renderColliders(frameInfo, r.meshSystem->pipelineLayout());
			r.debugSystem->renderBoxes(frameInfo, r.meshSystem->pipelineLayout(), debugBoxes);
		}
		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
		r.swapchain.endSwapChainRenderPass(commandBuffer);
		r.swapchain.endFrame();
	}

	void Renderer::waitIdle()
	{
		vkDeviceWaitIdle(impl->device.device());
	}
}
