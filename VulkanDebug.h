#pragma once
#include <iostream>
#include <string>
#include <sstream>

#include "vulkan/vulkan.h"

#include "VulkanTools.h"


class VulkanDebug
{
public:
	void setupInstance(VkInstance instance);

	void setupDevice(VkDevice device);

	void destroy(VkInstance instance);

	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData);

	void setBufferName(VkBuffer buffer, const std::string& name) const;

	void setAccelerationStructureName(VkBuffer buffer, const std::string& name);

private:
	void setObjectName(uint64_t object, VkObjectType objectType, const std::string& name) const;

private:
	VkDevice m_device;
	PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT = VK_NULL_HANDLE;
	PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT = VK_NULL_HANDLE;
	VkDebugUtilsMessengerEXT debugUtilsMessenger = VK_NULL_HANDLE;
	PFN_vkSetDebugUtilsObjectNameEXT vkSetDebugUtilsObjectNameEXT = VK_NULL_HANDLE;
};
