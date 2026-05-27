#include "vulkan_context.h"
#include <iostream>

#define VMA_IMPLEMENTATION
#include <vulkan_memory_allocator/vk_mem_alloc.h>
#include <SDL3/SDL_vulkan.h>

#include <core/log.h>
#include <window/window.h>

#include "vulkan_helpers.h"

namespace
{
	VKAPI_ATTR VkBool32 VKAPI_CALL RuyaVulkanDebugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageTypes,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData)
	{
		const char* type = "General";
		if (messageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT)
			type = "Validation";
		else if (messageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
			type = "Performance";

		switch (messageSeverity)
		{
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
			RUYA_LOG_DEBUG("[Vulkan][{}] {}", type, pCallbackData->pMessage);
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
			RUYA_LOG_INFO("[Vulkan][{}] {}", type, pCallbackData->pMessage);
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
			RUYA_LOG_WARN("[Vulkan][{}] {}", type, pCallbackData->pMessage);
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
			RUYA_LOG_ERROR("[Vulkan][{}] {}", type, pCallbackData->pMessage);
			break;
		default:
			RUYA_LOG_INFO("[Vulkan][{}] {}", type, pCallbackData->pMessage);
			break;
		}

		return VK_FALSE;
	}
}

ruya::VulkanContext::~VulkanContext()
{
	WaitDeviceIdle();
	DestroyVulkanMemoryAllocator();
	DestroyImmediateCommandBufferAndFence();
	DestroyFrameResources();
	DestroySwapchain();
	DestroyDevice();
	DestroySurface();
	DestroyInstance();
}

void ruya::VulkanContext::Init(Window* window, bool useValidation)
{
	applicationName = "Ruya Engine (Vulkan-1.3)";
	vulkanApiMajorVersion = 1;
	vulkanApiMinorVersion = 3;
	this->useValidation = useValidation;

	this->window = window;

	swapChainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;
	swapchainColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
	swapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;

	CreateInstance();
	CreateSurface();
	CreateDevice();
	SetDeviceQueue();
	CreateSwapchain();
	CreateFrameResources();
	CreateImmediateCommandBufferAndFence();
	CreateVulkanMemoryAllocator();

	RUYA_LOG_INFO("Vulkan Context initialized.");
}

VkInstance ruya::VulkanContext::GetInstance() const
{
	return instance;
}

VkPhysicalDevice ruya::VulkanContext::GetPhysicalDevice() const
{
	return physicalDevice;
}

VkDevice ruya::VulkanContext::GetDevice() const
{
	return device;
}

VkQueue ruya::VulkanContext::GetDeviceQueue() const
{
	return graphicsQueue;
}

uint32_t ruya::VulkanContext::GetDeviceQueueFamilyIndex() const
{
	return graphicsQueueFamilyIndex;
}

VkFormat ruya::VulkanContext::GetSwapchainImageFormat() const
{
	return swapChainImageFormat;
}

VkExtent2D ruya::VulkanContext::GetSwapchainImageExtent() const
{
	return swapChainImageExtent;
}

VkImageView ruya::VulkanContext::GetCurrentSwapchainImageView() const
{
	return swapchainImagesViews[swapchainImageIndex];
}

ruya::VulkanFrameResource* ruya::VulkanContext::GetCurrentFrameResource()
{
	return &frameResources[frameIndex];
}

ruya::VulkanFrameResource* ruya::VulkanContext::GetFrameResourceByIndex(uint32_t index)
{
	return &frameResources[index];
}

bool ruya::VulkanContext::IsSwapchainValid() const
{
	return swapchainValid;
}

VmaAllocator ruya::VulkanContext::GetVulkanMemoryAllocator() const
{
	return vulkanMemoryAllocator;
}

void ruya::VulkanContext::ImmediateSubmitCommand(std::function<void(VulkanCommandBuffer* commandBuffer)>&& function, const ImmediateSubmitTransferList& transferList)
{
	CHECK_VKRESULT(vkResetFences(device, 1, &immediateFence));

	asyncCommandBuffer->Reset();
	asyncCommandBuffer->Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	function(asyncCommandBuffer.get());

	for (auto& entry : transferList.buffers)
	{
		asyncCommandBuffer->ReleaseBufferOwnership(
			entry.buffer,
			asyncQueueFamilyIndex,
			graphicsQueueFamilyIndex,
			entry.releaseStage,
			entry.releaseAccess
		);
	}

	for (auto& imgEntry : transferList.images)
	{
		asyncCommandBuffer->ReleaseImageOwnership(
			imgEntry.image,
			asyncQueueFamilyIndex,
			graphicsQueueFamilyIndex,
			imgEntry.finalLayout,
			imgEntry.releaseStage,
			imgEntry.releaseAccess,
			imgEntry.subresourceRange
		);
	}

	asyncCommandBuffer->End();

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	VkCommandBuffer cb = asyncCommandBuffer->GetDeviceHandle();
	submitInfo.pCommandBuffers = &cb;

	CHECK_VKRESULT(vkQueueSubmit(asyncQueue, 1, &submitInfo, immediateFence));
	CHECK_VKRESULT(vkWaitForFences(device, 1, &immediateFence, VK_TRUE, UINT64_MAX));

	{
		std::unique_lock<std::mutex> lock(acquiresMutex);

		for (auto& entry : transferList.buffers)
		{
			pendingBufferAcquires.push_back({
				entry.buffer,
				entry.finalStage,
				entry.finalAccess
				});
		}

		for (auto& imgEntry : transferList.images)
		{
			pendingImageAcquires.push_back({
				imgEntry.image,
				imgEntry.finalLayout,
				imgEntry.finalLayout,
				imgEntry.finalStage,
				imgEntry.finalAccess,
				imgEntry.subresourceRange
				});
		}
	}
}

void ruya::VulkanContext::WaitDeviceIdle()
{
	vkDeviceWaitIdle(device);
}

uint32_t ruya::VulkanContext::GetSwapchainImageCount() const
{
	return swapchainImageCount;
}

bool ruya::VulkanContext::BeginFrame()
{
	if (window->IsWindowResized() || window->IsWindowMinimized() || window->IsFrameRateChanged())
	{
		if(window->IsWindowResized())
			window->SetWindowResized(false);

		if (window->IsFrameRateChanged()) 
		{
			window->SetFrameRateChanged(false);

			if (window->GetFrameRateMode() == 0) swapchainPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
			else if (window->GetFrameRateMode() == 1) swapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;
		}

		if (!window->IsWindowMinimized())
			RecreateSwapchain();

		swapchainValid = false;
		return false;
	}

	VulkanFrameResource& frameResource = frameResources[frameIndex];

	CHECK_VKRESULT_DEBUG(vkWaitForFences(device, 1, frameResource.GetRenderFence(), true, UINT64_MAX));

	CHECK_VKRESULT_DEBUG(vkResetFences(device, 1, frameResource.GetRenderFence()));

	VkResult acquireResult = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, swapchainImageAvailableSemaphores[frameIndex], VK_NULL_HANDLE, &swapchainImageIndex);

	if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR || acquireResult == VK_SUBOPTIMAL_KHR) 
	{
		if (!window->IsWindowMinimized())
			RecreateSwapchain();

		swapchainValid = false;

		CHECK_VKRESULT_DEBUG(vkResetFences(device, 1, frameResource.GetRenderFence()));

		return false;
	}

	CHECK_VKRESULT_DEBUG(acquireResult);

	swapchainValid = true;

	frameResource.GetDescriptorPool()->Reset();

	frameResource.GetCommandBuffer()->Reset();
	frameResource.GetCommandBuffer()->Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	frameResource.GetCommandBuffer()->ImageMemoryBarrier(
		swapchainImages[swapchainImageIndex],
		VK_ACCESS_NONE,
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
	);

	{
		std::unique_lock<std::mutex> lock(acquiresMutex);

		VulkanCommandBuffer* gfxCB = frameResource.GetCommandBuffer();

		for (auto& p : pendingBufferAcquires)
		{
			gfxCB->AcquireBufferOwnership(
				p.buffer,
				asyncQueueFamilyIndex,
				graphicsQueueFamilyIndex,
				p.dstStageMask,
				p.dstAccessMask
			);
		}
		pendingBufferAcquires.clear();

		for (auto& p : pendingImageAcquires)
		{
			gfxCB->AcquireImageOwnership(
				p.image,
				asyncQueueFamilyIndex,
				graphicsQueueFamilyIndex,
				p.oldLayout,
				p.newLayout,
				p.dstStageMask,
				p.dstAccessMask,
				p.subresourceRange
			);
		}
		pendingImageAcquires.clear();
	}

	return true;
}

void ruya::VulkanContext::EndFrame()
{
	VulkanFrameResource& frameResource = frameResources[frameIndex];

	frameResource.GetCommandBuffer()->ImageMemoryBarrier(
		swapchainImages[swapchainImageIndex],
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,    
		VK_ACCESS_NONE,                           
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 
		VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,          
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
		{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
	);

	frameResource.GetCommandBuffer()->End();

	VkCommandBufferSubmitInfo commandBufferSubmitInfo = {};
	commandBufferSubmitInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
	commandBufferSubmitInfo.pNext = nullptr;
	commandBufferSubmitInfo.commandBuffer = frameResource.GetCommandBuffer()->GetDeviceHandle();
	commandBufferSubmitInfo.deviceMask = 0;

	VkSemaphoreSubmitInfo waitSemaphoreSubmitInfo = {};
	waitSemaphoreSubmitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	waitSemaphoreSubmitInfo.pNext = nullptr;
	waitSemaphoreSubmitInfo.semaphore = swapchainImageAvailableSemaphores[frameIndex];
	waitSemaphoreSubmitInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;
	waitSemaphoreSubmitInfo.deviceIndex = 0;
	waitSemaphoreSubmitInfo.value = 0;

	VkSemaphoreSubmitInfo signalSemaphoreSubmitInfo = {};
	signalSemaphoreSubmitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	signalSemaphoreSubmitInfo.pNext = nullptr;
	signalSemaphoreSubmitInfo.semaphore = renderCompletedSemaphores[swapchainImageIndex];
	signalSemaphoreSubmitInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
	signalSemaphoreSubmitInfo.deviceIndex = 0;
	signalSemaphoreSubmitInfo.value = 0;

	VkSubmitInfo2 submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	submitInfo.pNext = nullptr;
	submitInfo.waitSemaphoreInfoCount = 1;
	submitInfo.pWaitSemaphoreInfos = &waitSemaphoreSubmitInfo;
	submitInfo.signalSemaphoreInfoCount = 1;
	submitInfo.pSignalSemaphoreInfos = &signalSemaphoreSubmitInfo;
	submitInfo.commandBufferInfoCount = 1;
	submitInfo.pCommandBufferInfos = &commandBufferSubmitInfo;

	CHECK_VKRESULT_DEBUG(vkQueueSubmit2(graphicsQueue, 1, &submitInfo, *frameResource.GetRenderFence()));

	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pNext = nullptr;
	presentInfo.pSwapchains = &swapchain;
	presentInfo.swapchainCount = 1;
	presentInfo.pWaitSemaphores = &renderCompletedSemaphores[swapchainImageIndex];
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pImageIndices = &swapchainImageIndex;

	VkResult presentResult = vkQueuePresentKHR(graphicsQueue, &presentInfo);

	frameIndex++;

	frameIndex = frameIndex % swapchainImageCount;

	if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) 
	{
		if (!window->IsWindowMinimized())
			RecreateSwapchain();
	}
}

void ruya::VulkanContext::CreateInstance()
{
	if (volkInitialize() != VK_SUCCESS)
	{
		RUYA_LOG_ERROR("[Volk] Volk failed to initialize");
	}

	vkb::InstanceBuilder builder;

	builder.set_app_name(applicationName.c_str())
		.require_api_version(vulkanApiMajorVersion, vulkanApiMinorVersion, 0);

#if defined(DEBUG) || defined(RELEASE)
	builder.request_validation_layers(true)
		.set_debug_callback(RuyaVulkanDebugCallback)
		.add_debug_messenger_severity
		(
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT
		)
		.add_debug_messenger_type
		(
			VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT
		);
#else
	builder.request_validation_layers(false);
#endif

	auto inst_ret = builder.build();
	if (!inst_ret)
	{
		RUYA_LOG_ERROR("Failed to create Vulkan instance: {0}", inst_ret.error().message());
		return;
	}

	vkbInstance = inst_ret.value();
	instance = vkbInstance.instance;

#if defined(DEBUG) || defined(RELEASE)
	debugUtilsMessenger = vkbInstance.debug_messenger;
#else
	debugUtilsMessenger = VK_NULL_HANDLE;
#endif

	volkLoadInstance(instance);
}

void ruya::VulkanContext::DestroyInstance()
{
#if defined(DEBUG) || defined(RELEASE)
	vkDestroyDebugUtilsMessengerEXT(instance, debugUtilsMessenger, nullptr);
#endif
	vkDestroyInstance(instance, nullptr);
}

void ruya::VulkanContext::CreateSurface()
{
	if(!SDL_Vulkan_CreateSurface(window->GetSDLWindow(), instance, nullptr, &surface))
	{
		RUYA_LOG_ERROR("[SDL] Create vulkan surface failed");
	}
}

void ruya::VulkanContext::DestroySurface()
{
	vkDestroySurfaceKHR(instance, surface, nullptr);
}

void ruya::VulkanContext::CreateDevice()
{
	VkPhysicalDeviceFeatures physicalDeviceFeatures = {};
	physicalDeviceFeatures.shaderInt64 = true;
	physicalDeviceFeatures.samplerAnisotropy = VK_TRUE;
	physicalDeviceFeatures.multiDrawIndirect = VK_TRUE;
	physicalDeviceFeatures.depthClamp = VK_FALSE;
	physicalDeviceFeatures.wideLines = VK_TRUE;
	physicalDeviceFeatures.shaderResourceMinLod = VK_TRUE;

	VkPhysicalDeviceVulkan11Features physicalDeviceFeatures11{};
	physicalDeviceFeatures11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
	physicalDeviceFeatures11.shaderDrawParameters = VK_TRUE;
	physicalDeviceFeatures11.storageBuffer16BitAccess = VK_TRUE;
	physicalDeviceFeatures11.uniformAndStorageBuffer16BitAccess = VK_TRUE;

	VkPhysicalDeviceVulkan13Features features13 = {};
	features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	features13.dynamicRendering = true;
	features13.synchronization2 = true;

	VkPhysicalDeviceAccelerationStructureFeaturesKHR accelFeature = {};
	accelFeature.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
	accelFeature.accelerationStructure = VK_TRUE;

	VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeature = {};
	rtPipelineFeature.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
	rtPipelineFeature.rayTracingPipeline = VK_TRUE;

#if defined(DEBUG) || defined(RELEASE)
	VkPhysicalDeviceRayTracingValidationFeaturesNV physicalDeviceRayTracingValidationFeaturesNV = {};
	physicalDeviceRayTracingValidationFeaturesNV.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_VALIDATION_FEATURES_NV;
	physicalDeviceRayTracingValidationFeaturesNV.rayTracingValidation = VK_TRUE;
#endif

	VkPhysicalDeviceVulkan12Features features12{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
	features12.bufferDeviceAddress = true;
	features12.descriptorIndexing = true;
	features12.runtimeDescriptorArray = VK_TRUE;
	features12.shaderStorageBufferArrayNonUniformIndexing = VK_TRUE;
	features12.descriptorBindingPartiallyBound = VK_TRUE;
	features12.descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE;
	features12.descriptorBindingUniformBufferUpdateAfterBind = VK_TRUE;
	features12.shaderUniformBufferArrayNonUniformIndexing = VK_TRUE;
	features12.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
	features12.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
	features12.scalarBlockLayout = VK_TRUE;
	features12.descriptorBindingVariableDescriptorCount = VK_TRUE;
	features12.uniformAndStorageBuffer8BitAccess = VK_TRUE;

	VkPhysicalDeviceRayQueryFeaturesKHR rayQueryFeatures = {};
	rayQueryFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;
	rayQueryFeatures.rayQuery = VK_TRUE;

	vkb::PhysicalDeviceSelector selector{ vkbInstance };
	selector = selector.set_minimum_version(1, 3);
	selector = selector.set_required_features(physicalDeviceFeatures);
	selector = selector.set_required_features_11(physicalDeviceFeatures11);
	selector = selector.set_required_features_13(features13);
	selector = selector.set_required_features_12(features12);
	selector = selector.add_required_extension_features<VkPhysicalDeviceAccelerationStructureFeaturesKHR>(accelFeature);
	selector = selector.add_required_extension_features<VkPhysicalDeviceRayTracingPipelineFeaturesKHR>(rtPipelineFeature);
	selector = selector.add_required_extension_features<VkPhysicalDeviceRayQueryFeaturesKHR>(rayQueryFeatures);
#if defined(DEBUG) || defined(RELEASE)
	selector = selector.add_required_extension_features<VkPhysicalDeviceRayTracingValidationFeaturesNV>(physicalDeviceRayTracingValidationFeaturesNV);
#endif
	selector = selector.add_required_extension(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
	selector = selector.add_required_extension(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
	selector = selector.add_required_extension(VK_KHR_RAY_QUERY_EXTENSION_NAME);

#if defined(DEBUG) || defined(RELEASE)
	selector = selector.add_required_extension(VK_NV_RAY_TRACING_VALIDATION_EXTENSION_NAME);
#endif

	selector = selector.add_required_extension(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
	selector = selector.set_surface(surface);
	std::vector<vkb::PhysicalDevice> physicalDevices = selector.select_devices().value();

	vkb::PhysicalDevice selectedDevice;

	RUYA_LOG_INFO("All aviable devices listed: ");

	for (uint32_t i = 0; i < physicalDevices.size(); i++)
	{
		std::cout << "\t[" << i << "] : " << physicalDevices[i].name << "\n";
	}

	if(physicalDevices.size() == 1)
	{
		RUYA_LOG_INFO("[Vulkan] Automatically device 0 selected: {}", physicalDevices[0].name.c_str());

		selectedDevice = physicalDevices[0];
	}

	else if (physicalDevices.size() > 0)
	{
		uint32_t selectedDeviceIndex;

		std::cout << "[Vulkan] Please select a device by entering its index: ";

		while (true)
		{
			std::cin >> selectedDeviceIndex;

			if (selectedDeviceIndex < physicalDevices.size())
				break;

			std::cout << "[Vulkan] Please select a valid device! Please select a device by entering its index: ";
		}

		std::cout << "[Vulkan] You selected device " << selectedDeviceIndex << " : "
			<< physicalDevices[selectedDeviceIndex].name << std::endl;

		selectedDevice = physicalDevices[selectedDeviceIndex];
	}

	uint32_t gfxFamilyIndex = UINT32_MAX;
	uint32_t transferFamilyIndex = UINT32_MAX;
	{
		uint32_t count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(selectedDevice.physical_device, &count, nullptr);
		std::vector<VkQueueFamilyProperties> props(count);
		vkGetPhysicalDeviceQueueFamilyProperties(selectedDevice.physical_device, &count, props.data());

		for (uint32_t i = 0; i < count; i++)
		{
			if (props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				gfxFamilyIndex = i;
				break;
			}
		}

		for (uint32_t i = 0; i < count; i++)
		{
			if (i == gfxFamilyIndex) continue;

			bool hasGfx = props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT;
			bool hasTransfer = props[i].queueFlags & VK_QUEUE_TRANSFER_BIT;
			bool hasCompute = props[i].queueFlags & VK_QUEUE_COMPUTE_BIT;

			if (!hasGfx && hasTransfer && hasCompute)
			{
				transferFamilyIndex = i;
				RUYA_LOG_INFO("[Vulkan] Dedicated transfer queue family found: {}", i);
				break;
			}
		}
	}

	std::vector<vkb::CustomQueueDescription> queueDescs;
	if (gfxFamilyIndex == transferFamilyIndex)
	{
		queueDescs.push_back(vkb::CustomQueueDescription(gfxFamilyIndex, { 1.0f, 0.5f }));
	}
	else
	{
		queueDescs.push_back(vkb::CustomQueueDescription(gfxFamilyIndex, { 1.0f }));
		queueDescs.push_back(vkb::CustomQueueDescription(transferFamilyIndex, { 1.0f }));
	}

	vkb::DeviceBuilder deviceBuilder{ selectedDevice };
	deviceBuilder.custom_queue_setup(queueDescs);
	vkbDevice = deviceBuilder.build().value();

	device = vkbDevice.device;
	physicalDevice = selectedDevice.physical_device;

	graphicsQueueFamilyIndex = gfxFamilyIndex;
	asyncQueueFamilyIndex = transferFamilyIndex;

	volkLoadDevice(device);
}

void ruya::VulkanContext::DestroyDevice()
{
	vkDestroyDevice(device, nullptr);
}

void ruya::VulkanContext::SetDeviceQueue()
{
	vkGetDeviceQueue(device, graphicsQueueFamilyIndex, 0, &graphicsQueue);
	vkGetDeviceQueue(device, asyncQueueFamilyIndex, 0, &asyncQueue);

	RUYA_LOG_INFO("[Queue] graphicsQueue: {:p} (family {})", (void*)graphicsQueue, graphicsQueueFamilyIndex);
	RUYA_LOG_INFO("[Queue] asyncQueue:    {:p} (family {})", (void*)asyncQueue, asyncQueueFamilyIndex);
}

void ruya::VulkanContext::CreateSwapchain()
{
	VkSurfaceCapabilitiesKHR capabilities;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities);

	VkExtent2D actualExtent;
	if (capabilities.currentExtent.width != 0xFFFFFFFF) {
		actualExtent = capabilities.currentExtent;
	}
	else {
		uint32_t width = static_cast<uint32_t>(window->GetWindowWidth());
		uint32_t height = static_cast<uint32_t>(window->GetWindowHeight());

		actualExtent.width = std::clamp(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
		actualExtent.height = std::clamp(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
	}

	VkSwapchainCreateInfoKHR swapchainCreateInfo = {};
	swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchainCreateInfo.pNext = nullptr;
	swapchainCreateInfo.flags = 0;
	swapchainCreateInfo.surface = surface;
	swapchainCreateInfo.minImageCount = 2;
	swapchainCreateInfo.imageFormat = swapChainImageFormat;
	swapchainCreateInfo.imageColorSpace = swapchainColorSpace;
	swapchainCreateInfo.imageExtent = actualExtent;
	swapchainCreateInfo.imageArrayLayers = 1;
	swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	swapchainCreateInfo.queueFamilyIndexCount = 0;
	swapchainCreateInfo.pQueueFamilyIndices = nullptr;
	swapchainCreateInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapchainCreateInfo.presentMode = swapchainPresentMode;
	swapchainCreateInfo.clipped = VK_TRUE;
	swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

	CHECK_VKRESULT(vkCreateSwapchainKHR(device, &swapchainCreateInfo, nullptr, &swapchain));

	swapChainImageExtent = actualExtent;

	vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, nullptr);
	swapchainImages.resize(swapchainImageCount);
	vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, swapchainImages.data());

	swapchainImageAvailableSemaphores.resize(2);
	renderCompletedSemaphores.resize(2);

	swapchainImagesViews.resize(swapchainImageCount);
	for (uint32_t i = 0; i < swapchainImageCount; i++) {
		VkImageViewCreateInfo imageViewCreateInfo = {};
		imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCreateInfo.pNext = nullptr;
		imageViewCreateInfo.flags = 0;
		imageViewCreateInfo.image = swapchainImages[i];
		imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewCreateInfo.format = swapChainImageFormat;
		imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
		imageViewCreateInfo.subresourceRange.levelCount = 1;
		imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
		imageViewCreateInfo.subresourceRange.layerCount = 1;

		CHECK_VKRESULT(vkCreateImageView(device, &imageViewCreateInfo, nullptr, &swapchainImagesViews[i]));

		VkSemaphoreCreateInfo semapCreateInfo = {};
		semapCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		semapCreateInfo.pNext = nullptr;

		CHECK_VKRESULT(vkCreateSemaphore(device, &semapCreateInfo, nullptr, &swapchainImageAvailableSemaphores[i]));
		CHECK_VKRESULT(vkCreateSemaphore(device, &semapCreateInfo, nullptr, &renderCompletedSemaphores[i]));
	}

	frameIndex = 0;
	swapchainImageIndex = 0;
}

void ruya::VulkanContext::RecreateSwapchain()
{
	vkDeviceWaitIdle(device);

	DestroySwapchain();

	for (int i = 0; i < frameResources.size(); i++)
	{
		vkDestroyFence(device, *frameResources[i].GetRenderFence(), nullptr);

		VkFenceCreateInfo fenceCreateInfo = {};
		fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceCreateInfo.pNext = nullptr;
		fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		CHECK_VKRESULT(vkCreateFence(device, &fenceCreateInfo, nullptr, frameResources[i].GetRenderFence()));
	}

	CreateSwapchain();
}

void ruya::VulkanContext::DestroySwapchain()
{
	for (int i = 0; i < swapchainImagesViews.size(); i++) 
	{
		vkDestroyImageView(device, swapchainImagesViews[i], nullptr);
		vkDestroySemaphore(device, swapchainImageAvailableSemaphores[i], nullptr);
		vkDestroySemaphore(device, renderCompletedSemaphores[i], nullptr);
	}

	if (swapchain != VK_NULL_HANDLE) {
		vkDestroySwapchainKHR(device, swapchain, nullptr);
		swapchain = VK_NULL_HANDLE;
	}

	swapchainImageAvailableSemaphores.clear();
	renderCompletedSemaphores.clear();
	swapchainImagesViews.clear();
	swapchainImages.clear();
}

void ruya::VulkanContext::CreateFrameResources()
{
	for (uint32_t i = 0; i < swapchainImageCount; i++)
	{
		frameResources.push_back(VulkanFrameResource());
		frameResources.back().Create(this);
	}
}

void ruya::VulkanContext::DestroyFrameResources()
{
	for (uint32_t i = 0; i < static_cast<uint32_t>(frameResources.size()); i++)
	{
		frameResources[i].Destroy(this);
	}

	frameResources.clear();
}

void ruya::VulkanContext::CreateImmediateCommandBufferAndFence()
{
	VkCommandPoolCreateInfo commandPoolInfo = {};
	commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolInfo.pNext = nullptr;
	commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	commandPoolInfo.queueFamilyIndex = asyncQueueFamilyIndex;

	CHECK_VKRESULT(vkCreateCommandPool(device, &commandPoolInfo, nullptr, &asyncCommandPool));

	asyncCommandBuffer = std::make_unique<VulkanCommandBuffer>(this, asyncCommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	VkFenceCreateInfo fenceCreateInfo = {};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.pNext = nullptr;
	fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	CHECK_VKRESULT(vkCreateFence(device, &fenceCreateInfo, nullptr, &immediateFence));
}

void ruya::VulkanContext::DestroyImmediateCommandBufferAndFence()
{
	vkDestroyFence(device, immediateFence, nullptr);
	vkDestroyCommandPool(device, asyncCommandPool, nullptr);
}

void ruya::VulkanContext::CreateVulkanMemoryAllocator()
{
	VmaVulkanFunctions vulkanFunctions = {};
	vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
	vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

	VmaAllocatorCreateInfo allocatorInfo = {};
	allocatorInfo.physicalDevice = physicalDevice;
	allocatorInfo.device = device;
	allocatorInfo.instance = instance;
	allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
	allocatorInfo.pVulkanFunctions = &vulkanFunctions;

	CHECK_VKRESULT(vmaCreateAllocator(&allocatorInfo, &vulkanMemoryAllocator));
}

void ruya::VulkanContext::DestroyVulkanMemoryAllocator()
{
	vmaDestroyAllocator(vulkanMemoryAllocator);
}
