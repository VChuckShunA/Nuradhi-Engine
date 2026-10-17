#pragma once
#include <vulkan/vulkan_core.h>
#include "lve_device.hpp"
#include <string>

namespace lve {
	enum class TextureFilter {
		Smooth, //linear filtering with mipmaps, for regular textures
		Pixelated, //nearest filtering, top mip only, clamped: crisp pixel art, and frames on a sprite sheet can't bleed into each other
	};

	class Texture {
	public:
		Texture(LveDevice& device, const std::string& filepath, TextureFilter filter = TextureFilter::Smooth);
		~Texture();


		Texture(const Texture&) = delete;
		Texture& operator=(const Texture&) = delete;
		Texture(Texture&&) = delete;
		Texture& operator=(Texture&&) = delete;

		VkSampler getSampler() { return sampler; }
		VkImageView getImageView() { return imageView; }
		VkImageLayout getImageLayout() { return imageLayout; }

	private:
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