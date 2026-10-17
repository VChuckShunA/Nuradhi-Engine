#pragma once
#include <vulkan/vulkan_core.h>
#include "lve_device.hpp"
#include "engine/render/render_components.hpp"
#include <string>

namespace lve {
	using TextureFilter = nrd::TextureFilter;

	class Texture {
	public:
		Texture(LveDevice& device, const std::string& filepath, TextureFilter filter = TextureFilter::Smooth);
		//from tightly packed 8-bit RGBA pixels, top row first
		Texture(LveDevice& device, const unsigned char* rgba, int pixelWidth, int pixelHeight, TextureFilter filter = TextureFilter::Smooth);
		~Texture();


		Texture(const Texture&) = delete;
		Texture& operator=(const Texture&) = delete;
		Texture(Texture&&) = delete;
		Texture& operator=(Texture&&) = delete;

		VkSampler getSampler() { return sampler; }
		VkImageView getImageView() { return imageView; }
		VkImageLayout getImageLayout() { return imageLayout; }

	private:
		void createFromPixels(const unsigned char* rgba, TextureFilter filter);
		void transitionImageLayout(VkImageLayout oldLayout, VkImageLayout newLayout);
		void generateMipmaps();

		int width, height, mipLevels;

		lve::LveDevice & lveDevice;
		VkImage image;
		VkDeviceMemory imageMemory;
		VkImageView imageView;
		VkSampler sampler;
		VkFormat imageFormat;
		VkImageLayout imageLayout;

	};
}