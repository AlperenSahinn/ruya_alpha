#pragma once
#include <string>
#include <functional>
#include <memory>

#include <volk/volk.h>
#include <vk_bootstrap/VkBootstrap.h>
#include <vulkan_memory_allocator/vk_mem_alloc.h>

#include <window/window.h>

#include "vulkan_frame_resource.h"
#include "vulkan_command_buffer.h"

namespace ruya
{
	class VulkanContext
	{
	public:
		VulkanContext() = default;
		~VulkanContext();

		VulkanContext(const VulkanContext&) = delete;
		VulkanContext& operator=(const VulkanContext&) = delete;

		void Init(Window* window, bool useValidation);
		void Destroy();

		VkInstance GetInstance() const;
		VkPhysicalDevice GetPhysicalDevice() const;
		VkDevice GetDevice() const;
		VkQueue GetDeviceQueue() const;
		uint32_t GetDeviceQueueFamilyIndex() const;

		VkFormat GetSwapchainImageFormat() const;
		VkExtent2D GetSwapchainImageExtent() const;
		VkImageView GetCurrentSwapchainImageView() const;
		VulkanFrameResource* GetCurrentFrameResource();
		VulkanFrameResource* GetFrameResourceByIndex(uint32_t index);
		bool IsSwapchainValid() const;

		VmaAllocator GetVulkanMemoryAllocator() const;

		void ImmediateSubmitCommand(std::function<void(VulkanCommandBuffer* commandBuffer)>&& function);

		bool BeginFrame();
		void EndFrame();

		void WaitDeviceIdle();

		uint32_t GetSwapchainImageCount() const;

	private:
		void CreateInstance();
		void DestroyInstance();

		void CreateSurface();
		void DestroySurface();

		void CreateDevice();
		void DestroyDevice();

		void SetDeviceQueue();

		void CreateSwapchain();
		void RecreateSwapchain();
		void DestroySwapchain();

		void CreateFrameResources();
		void DestroyFrameResources();

		void CreateImmediateCommandBufferAndFence();
		void DestroyImmediateCommandBufferAndFence();

		void CreateVulkanMemoryAllocator();
		void DestroyVulkanMemoryAllocator();

	private:
		std::string applicationName;
		uint32_t vulkanApiMajorVersion;
		uint32_t vulkanApiMinorVersion;
		bool useValidation;

		Window* window;

		vkb::Instance vkbInstance;
		vkb::Device vkbDevice;

		VkInstance instance;
		VkDebugUtilsMessengerEXT debugUtilsMessenger;
		VkSurfaceKHR surface;
		VkDevice device;
		VkPhysicalDevice physicalDevice;

		VkQueue deviceQueue;
		uint32_t deviceQueueFamilyIndex;

		VkFormat swapChainImageFormat;
		VkColorSpaceKHR swapchainColorSpace;
		VkPresentModeKHR swapchainPresentMode;
		uint32_t swapchainImageCount;
		VkExtent2D swapChainImageExtent;
		VkSwapchainKHR swapchain;
		uint32_t swapchainImageIndex;
		uint32_t frameIndex;
		std::vector<VkImage> swapchainImages;
		std::vector<VkImageView> swapchainImagesViews;
		std::vector<VkSemaphore> swapchainImageAvailableSemaphores;
		std::vector<VkSemaphore> renderCompletedSemaphores;
		std::vector<VulkanFrameResource> frameResources;
		bool swapchainValid;

		VmaAllocator vulkanMemoryAllocator;

		VkCommandPool commandPool;
		std::unique_ptr<VulkanCommandBuffer> commandBuffer;
		VkFence immediateFence;
	};
}