#pragma once

#include <stdlib.h>
#include <string>
#include <vector>
#include <array>
#include <chrono>
#include <numeric>

#define VK_ENABLE_BETA_EXTENSIONS
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "VulkanTools.h"
#include "VulkanDebug.h"
#include "VulkanSwapChain.h"
#include "VulkanBuffer.h"
#include "VulkanDevice.h"
#include "VulkanInitializers.hpp"
#include "camera.hpp"
#include "benchmark.hpp"


// Holds data for a ray tracing scratch buffer that is used as a temporary storage
struct RayTracingScratchBuffer
{
	uint64_t deviceAddress = 0;
	VkBuffer buffer = VK_NULL_HANDLE;
	VkDeviceMemory memory = VK_NULL_HANDLE;
};

// Holds data for a memory object bound to an acceleration structure
struct RayTracingObjectMemory
{
	uint64_t deviceAddress = 0;
	VkDeviceMemory memory = VK_NULL_HANDLE;
};

// Ray tracing acceleration structure
struct AccelerationStructure {
	VkAccelerationStructureKHR accelerationStructure;
	uint64_t handle;
	RayTracingObjectMemory objectMemory;
};

// Indices for the different ray tracing shader types
#define INDEX_RAYGEN_GROUP 0
#define INDEX_MISS_GROUP 1
#define INDEX_CLOSEST_HIT_GROUP 2


class VulkanRaytracer
{
public:
	VulkanRaytracer(const std::vector<std::string>& args = {}, bool enableValidation = false);
	virtual ~VulkanRaytracer();

	// Init GLFW, setup the vulkan instance, enable required extensions and connect to the physical device (GPU)
	bool initAPIs();
	void setupWindow();

	// Prepares all Vulkan resources and functions required to run the sample
	void prepare();

	// Entry point for the main render loop
	void renderLoop();

private:
	/** @brief (Virtual) Creates the application wide Vulkan instance */
	virtual VkResult createInstance(bool enableValidation);
	/** @brief (Virtual) Called when the camera view has changed */
	virtual void viewChanged();
	/** @brief (Virtual) Called after a key was pressed, can be used to do custom key handling */
	virtual void keyPressed(uint32_t);
	/** @brief (Virtual) Called after the mouse cursor moved and before internal events (like camera rotation) is handled */
	virtual void mouseMoved(double x, double y, bool& handled);
	/** @brief (Virtual) Called when the window has been resized, can be used by the sample application to recreate resources */
	virtual void windowResized();
	/** @brief (Virtual) Setup default depth and stencil views */
	virtual void setupDepthStencil();
	/** @brief (Virtual) Setup default framebuffers for all requested swapchain images */
	virtual void setupFrameBuffer();
	/** @brief (Virtual) Setup a default renderpass */
	virtual void setupRenderPass();

	/** @brief Loads a SPIR-V shader file for the given shader stage */
	VkPipelineShaderStageCreateInfo loadShader(std::string fileName, VkShaderStageFlagBits stage);

	/** Prepare the next frame for workload submission by acquiring the next swap chain image */
	void prepareFrame();
	/** @brief Presents the current image to the swap chain */
	void submitFrame();
	/** @brief (Virtual) Default image acquire + submission and command buffer submission function */
	virtual void renderFrame();

	RayTracingScratchBuffer createScratchBuffer(VkAccelerationStructureKHR accelerationStructure);
	void deleteScratchBuffer(RayTracingScratchBuffer& scratchBuffer);

	RayTracingObjectMemory createObjectMemory(VkAccelerationStructureKHR acceleration_structure);

	uint64_t getBufferDeviceAddress(VkBuffer buffer);

	void createStorageImage();

	void createBottomLevelAccelerationStructure();

	void createTopLevelAccelerationStructure();

	void createShaderBindingTable();

	void createDescriptorSets();

	void createRayTracingPipeline();

	void createUniformBuffer();

	void handleResize();

	//Called when resources have been recreated that require a rebuild of the command buffers(e.g.frame buffer), to be implemented by the sample application
	void buildCommandBuffers();

	void updateUniformBuffers();

	//Called after the physical device features have been read, can be used to set features to enable on the device
	void getEnabledFeatures();

	void draw();

	//Render function to be implemented by the sample application
	void render();

	// Returns the path to the root of the glsl or hlsl shader directory.
	std::string getShadersPath() const;

	static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
	static void cursorPositionCallback(GLFWwindow* window, double x, double y);
	static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);

private:
	bool viewUpdated = false;
	uint32_t destWidth;
	uint32_t destHeight;
	bool resizing = false;
	void windowResize();
	//void handleMouseMove(int32_t x, int32_t y);
	void nextFrame();
	void createPipelineCache();
	void createCommandPool();
	void createSynchronizationPrimitives();
	void initSwapchain();
	void setupSwapChain();
	void createCommandBuffers();
	void destroyCommandBuffers();
	std::string shaderDir = "glsl";

	// Frame counter to display fps
	uint32_t frameCounter = 0;
	uint32_t lastFPS = 0;
	std::chrono::time_point<std::chrono::high_resolution_clock> lastTimestamp;
	// Vulkan instance, stores all per-application states
	VkInstance instance;
	std::vector<std::string> supportedInstanceExtensions;
	// Physical device (GPU) that Vulkan will use
	VkPhysicalDevice physicalDevice;
	// Index of selected device (defaults to the first device unless specified by command line)
	uint32_t selectedDevice = 0;
	// Stores physical device properties (for e.g. checking device limits)
	VkPhysicalDeviceProperties deviceProperties;
	// Stores the features available on the selected physical device (for e.g. checking if a feature is available)
	VkPhysicalDeviceFeatures deviceFeatures;
	// Stores all available memory (type) properties for the physical device
	VkPhysicalDeviceMemoryProperties deviceMemoryProperties;
	/** @brief Set of physical device features to be enabled for this example (must be set in the derived constructor) */
	VkPhysicalDeviceFeatures enabledFeatures{};
	/** @brief Set of device extensions to be enabled for this example (must be set in the derived constructor) */
	std::vector<const char*> enabledDeviceExtensions;
	std::vector<const char*> enabledInstanceExtensions;
	/** @brief Optional pNext structure for passing extension structures to device creation */
	void* deviceCreatepNextChain = nullptr;
	/** @brief Logical device, application's view of the physical device (GPU) */
	VkDevice device;
	// Handle to the device graphics queue that command buffers are submitted to
	VkQueue queue;
	// Depth buffer format (selected during Vulkan initialization)
	VkFormat depthFormat;
	// Command buffer pool
	VkCommandPool cmdPool;
	/** @brief Pipeline stages used to wait at for graphics queue submissions */
	VkPipelineStageFlags submitPipelineStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	// Contains command buffers and semaphores to be presented to the queue
	VkSubmitInfo submitInfo;
	// Command buffers used for rendering
	std::vector<VkCommandBuffer> drawCmdBuffers;
	// Global render pass for frame buffer writes
	VkRenderPass renderPass;
	// List of available frame buffers (same as number of swap chain images)
	std::vector<VkFramebuffer>frameBuffers;
	// Active frame buffer index
	uint32_t currentBuffer = 0;
	// Descriptor set pool
	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
	// List of shader modules created (stored for cleanup)
	std::vector<VkShaderModule> shaderModules;
	// Pipeline cache object
	VkPipelineCache pipelineCache;
	// Wraps the swap chain to present images (framebuffers) to the windowing system
	VulkanSwapChain swapChain;
	// Synchronization semaphores
	struct {
		// Swap chain image presentation
		VkSemaphore presentComplete;
		// Command buffer submission and execution
		VkSemaphore renderComplete;
	} semaphores;
	std::vector<VkFence> waitFences;

	bool prepared = false;
	bool resized = false;
	uint32_t width = 1280;
	uint32_t height = 720;

	PFN_vkGetBufferDeviceAddressKHR vkGetBufferDeviceAddressKHR = nullptr;
	PFN_vkBindAccelerationStructureMemoryKHR vkBindAccelerationStructureMemoryKHR = nullptr;
	PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR = nullptr;
	PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR = nullptr;
	PFN_vkGetAccelerationStructureMemoryRequirementsKHR vkGetAccelerationStructureMemoryRequirementsKHR = nullptr;
	PFN_vkCmdBuildAccelerationStructureKHR vkCmdBuildAccelerationStructureKHR = nullptr;
	PFN_vkBuildAccelerationStructureKHR vkBuildAccelerationStructureKHR = nullptr;
	PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR = nullptr;
	PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR = nullptr;
	PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR = nullptr;
	PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR = nullptr;

	VkPhysicalDeviceRayTracingPropertiesKHR rayTracingProperties{};
	VkPhysicalDeviceRayTracingFeaturesKHR rayTracingFeatures{};

	VkPhysicalDeviceBufferDeviceAddressFeatures enabledBufferDeviceAddresFeatures{};
	VkPhysicalDeviceRayTracingFeaturesKHR enabledRayTracingFeatures{};

	AccelerationStructure bottomLevelAS;
	AccelerationStructure topLevelAS;

	vks::Buffer vertexBuffer;
	vks::Buffer indexBuffer;
	uint32_t indexCount;
	std::vector<VkRayTracingShaderGroupCreateInfoKHR> shaderGroups{};
	vks::Buffer shaderBindingTable;

	struct StorageImage {
		VkDeviceMemory memory;
		VkImage image;
		VkImageView view;
		VkFormat format;
	} storageImage;

	struct UniformData {
		glm::mat4 viewInverse;
		glm::mat4 projInverse;
	} uniformData;
	vks::Buffer ubo;

	VkPipeline pipeline;
	VkPipelineLayout pipelineLayout;
	VkDescriptorSet descriptorSet;
	VkDescriptorSetLayout descriptorSetLayout;

	/** @brief Last frame time measured using a high performance timer (if available) */
	float frameTimer = 1.0f;

	vks::Benchmark benchmark;

	/** @brief Encapsulated physical and logical vulkan device */
	vks::VulkanDevice* vulkanDevice;

	/** @brief Example settings that can be changed e.g. by command line arguments */
	struct Settings {
		/** @brief Activates validation layers (and message output) when set to true */
		bool validation = false;
		/** @brief Set to true if fullscreen mode has been requested via command line */
		bool fullscreen = false;
		/** @brief Set to true if v-sync will be forced for the swapchain */
		bool vsync = false;
	} settings;

	VkClearColorValue defaultClearColor = { { 0.025f, 0.025f, 0.025f, 1.0f } };

	// Defines a frame rate independent timer value clamped from -1.0...1.0
	// For use in animations, rotations, etc.
	float timer = 0.0f;
	// Multiplier for speeding up (or slowing down) the global timer
	float timerSpeed = 0.25f;
	bool paused = false;

	Camera camera;
	glm::vec2 mousePos;

	std::string applicationName = "Raytracer";

	struct {
		VkImage image;
		VkDeviceMemory mem;
		VkImageView view;
	} depthStencil;

	struct {
		glm::vec2 axisLeft = glm::vec2(0.0f);
		glm::vec2 axisRight = glm::vec2(0.0f);
	} gamePadState;

	struct {
		bool left = false;
		bool right = false;
		bool middle = false;
	} mouseButtons;

	GLFWwindow* window;
};
