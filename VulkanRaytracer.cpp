#include "VulkanRaytracer.h"


VkResult VulkanRaytracer::createInstance()
{
	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = applicationName.c_str();
	appInfo.pEngineName = applicationName.c_str();
	// We require Vulkan 1.2 for ray tracing
	appInfo.apiVersion = VK_API_VERSION_1_2;

	std::vector<const char*> instanceExtensions;
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
	instanceExtensions.insert(instanceExtensions.end(), glfwExtensions, glfwExtensions + glfwExtensionCount);

	// Get extensions supported by the instance and store for later use
	uint32_t extCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extCount, nullptr);
	if (extCount > 0)
	{
		std::vector<VkExtensionProperties> extensions(extCount);
		if (vkEnumerateInstanceExtensionProperties(nullptr, &extCount, &extensions.front()) == VK_SUCCESS)
		{
			for (VkExtensionProperties extension : extensions)
			{
				supportedInstanceExtensions.push_back(extension.extensionName);
			}
		}
	}

	instanceExtensions.insert(instanceExtensions.end(), enabledInstanceExtensions.begin(), enabledInstanceExtensions.end());
	if (settings.validation)
	{
		instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}

	if (instanceExtensions.size() > 0)
	{
		for (auto enabledExtension : instanceExtensions)
		{
			// Output message if requested extension is not available
			if (std::find(supportedInstanceExtensions.begin(), supportedInstanceExtensions.end(), enabledExtension) == supportedInstanceExtensions.end())
			{
				std::cerr << "Enabled instance extension \"" << enabledExtension << "\" is not present at instance level\n";
			}
		}
	}

	VkInstanceCreateInfo instanceCreateInfo = {};
	instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceCreateInfo.pNext = nullptr;
	instanceCreateInfo.pApplicationInfo = &appInfo;
	if (instanceExtensions.size() > 0)
	{
		instanceCreateInfo.enabledExtensionCount = (uint32_t)instanceExtensions.size();
		instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();
	}
	if (settings.validation)
	{
		// The VK_LAYER_KHRONOS_validation contains all current validation functionality.
		// Note that on Android this layer requires at least NDK r20
		const char* validationLayerName = "VK_LAYER_KHRONOS_validation";
		// Check if this layer is available at instance level
		uint32_t instanceLayerCount;
		vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr);
		std::vector<VkLayerProperties> instanceLayerProperties(instanceLayerCount);
		vkEnumerateInstanceLayerProperties(&instanceLayerCount, instanceLayerProperties.data());
		bool validationLayerPresent = false;
		for (VkLayerProperties layer : instanceLayerProperties) {
			if (strcmp(layer.layerName, validationLayerName) == 0) {
				validationLayerPresent = true;
				break;
			}
		}
		if (validationLayerPresent) {
			instanceCreateInfo.ppEnabledLayerNames = &validationLayerName;
			instanceCreateInfo.enabledLayerCount = 1;

			VkDebugUtilsMessengerCreateInfoEXT createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
			createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
			createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
			createInfo.pfnUserCallback = vks::debug::debugCallback;
			instanceCreateInfo.pNext = &createInfo;
		} else {
			std::cerr << "Validation layer VK_LAYER_KHRONOS_validation not present, validation is disabled";
		}
	}
	return vkCreateInstance(&instanceCreateInfo, nullptr, &instance);
}

void VulkanRaytracer::renderFrame()
{
	prepareFrame();
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &drawCmdBuffers[currentBuffer];
	VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
	submitFrame();
}

void VulkanRaytracer::createCommandBuffers()
{
	// Create one command buffer for each swap chain image and reuse for rendering
	drawCmdBuffers.resize(swapChain.imageCount);

	VkCommandBufferAllocateInfo cmdBufAllocateInfo =
		vks::initializers::commandBufferAllocateInfo(
			cmdPool,
			VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			static_cast<uint32_t>(drawCmdBuffers.size()));

	VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, drawCmdBuffers.data()));
}

void VulkanRaytracer::destroyCommandBuffers()
{
	vkFreeCommandBuffers(device, cmdPool, static_cast<uint32_t>(drawCmdBuffers.size()), drawCmdBuffers.data());
}

void VulkanRaytracer::createPipelineCache()
{
	VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
	pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
	VK_CHECK_RESULT(vkCreatePipelineCache(device, &pipelineCacheCreateInfo, nullptr, &pipelineCache));
}

void VulkanRaytracer::prepare()
{
	if (vulkanDevice->enableDebugMarkers) {
		vks::debugmarker::setup(device);
	}
	initSwapchain();
	createCommandPool();
	setupSwapChain();
	createCommandBuffers();
	createSynchronizationPrimitives();
	setupDepthStencil();
	setupRenderPass();
	createPipelineCache();
	setupFrameBuffer();

	// Query the ray tracing properties of the current implementation, we will need them later on
	rayTracingProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PROPERTIES_KHR;
	VkPhysicalDeviceProperties2 deviceProps2{};
	deviceProps2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	deviceProps2.pNext = &rayTracingProperties;
	vkGetPhysicalDeviceProperties2(physicalDevice, &deviceProps2);

	// Query the ray tracing properties of the current implementation, we will need them later on
	rayTracingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_FEATURES_KHR;
	VkPhysicalDeviceFeatures2 deviceFeatures2{};
	deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	deviceFeatures2.pNext = &rayTracingFeatures;
	vkGetPhysicalDeviceFeatures2(physicalDevice, &deviceFeatures2);

	// Get the function pointers required for ray tracing
	vkGetBufferDeviceAddressKHR = reinterpret_cast<PFN_vkGetBufferDeviceAddressKHR>(vkGetDeviceProcAddr(device, "vkGetBufferDeviceAddressKHR"));
	vkBindAccelerationStructureMemoryKHR = reinterpret_cast<PFN_vkBindAccelerationStructureMemoryKHR>(vkGetDeviceProcAddr(device, "vkBindAccelerationStructureMemoryKHR"));
	vkCreateAccelerationStructureKHR = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(vkGetDeviceProcAddr(device, "vkCreateAccelerationStructureKHR"));
	vkDestroyAccelerationStructureKHR = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(vkGetDeviceProcAddr(device, "vkDestroyAccelerationStructureKHR"));
	vkGetAccelerationStructureMemoryRequirementsKHR = reinterpret_cast<PFN_vkGetAccelerationStructureMemoryRequirementsKHR>(vkGetDeviceProcAddr(device, "vkGetAccelerationStructureMemoryRequirementsKHR"));
	vkCmdBuildAccelerationStructureKHR = reinterpret_cast<PFN_vkCmdBuildAccelerationStructureKHR>(vkGetDeviceProcAddr(device, "vkCmdBuildAccelerationStructureKHR"));
	vkBuildAccelerationStructureKHR = reinterpret_cast<PFN_vkBuildAccelerationStructureKHR>(vkGetDeviceProcAddr(device, "vkBuildAccelerationStructureKHR"));
	vkGetAccelerationStructureDeviceAddressKHR = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(vkGetDeviceProcAddr(device, "vkGetAccelerationStructureDeviceAddressKHR"));
	vkCmdTraceRaysKHR = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(vkGetDeviceProcAddr(device, "vkCmdTraceRaysKHR"));
	vkGetRayTracingShaderGroupHandlesKHR = reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(vkGetDeviceProcAddr(device, "vkGetRayTracingShaderGroupHandlesKHR"));
	vkCreateRayTracingPipelinesKHR = reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(vkGetDeviceProcAddr(device, "vkCreateRayTracingPipelinesKHR"));

	// Create the acceleration structures used to render the ray traced scene
	createBottomLevelAccelerationStructure();
	createTopLevelAccelerationStructure();

	createStorageImage();
	createUniformBuffer();
	createRayTracingPipeline();
	createShaderBindingTable();
	createDescriptorSets();
	buildCommandBuffers();
	prepared = true;
}

VkPipelineShaderStageCreateInfo VulkanRaytracer::loadShader(std::string fileName, VkShaderStageFlagBits stage)
{
	VkPipelineShaderStageCreateInfo shaderStage = {};
	shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStage.stage = stage;
	shaderStage.module = vks::tools::loadShader(fileName.c_str(), device);
	shaderStage.pName = "main";
	assert(shaderStage.module != VK_NULL_HANDLE);
	shaderModules.push_back(shaderStage.module);
	return shaderStage;
}

void VulkanRaytracer::nextFrame()
{
	auto tStart = std::chrono::high_resolution_clock::now();
	if (viewUpdated)
	{
		viewUpdated = false;
	}

	render();
	frameCounter++;
	auto tEnd = std::chrono::high_resolution_clock::now();
	auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
	frameTimer = (float)tDiff / 1000.0f;
	camera.update(frameTimer);
	if (camera.moving())
	{
		viewUpdated = true;
	}
	// Convert to clamped timer value
	if (!paused)
	{
		timer += timerSpeed * frameTimer;
		if (timer > 1.0)
		{
			timer -= 1.0f;
		}
	}
	float fpsTimer = (float)(std::chrono::duration<double, std::milli>(tEnd - lastTimestamp).count());
	if (fpsTimer > 1000.0f)
	{
		lastFPS = static_cast<uint32_t>((float)frameCounter * (1000.0f / fpsTimer));
		frameCounter = 0;
		lastTimestamp = tEnd;
	}

	buildCommandBuffers();
}

void VulkanRaytracer::renderLoop()
{
	if (benchmark.active) {
		benchmark.run([=] { render(); }, vulkanDevice->properties);
		vkDeviceWaitIdle(device);
		if (benchmark.filename != "") {
			benchmark.saveResults();
		}
		return;
	}

	destWidth = width;
	destHeight = height;
	lastTimestamp = std::chrono::high_resolution_clock::now();


	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		if (prepared) {
			nextFrame();
		}
	}

	// Flush device to make sure all resources can be freed
	if (device != VK_NULL_HANDLE) {
		vkDeviceWaitIdle(device);
	}
}

void VulkanRaytracer::prepareFrame()
{
	// Acquire the next image from the swap chain
	VkResult result = swapChain.acquireNextImage(semaphores.presentComplete, &currentBuffer);
	// Recreate the swapchain if it's no longer compatible with the surface (OUT_OF_DATE) or no longer optimal for presentation (SUBOPTIMAL)
	if ((result == VK_ERROR_OUT_OF_DATE_KHR) || (result == VK_SUBOPTIMAL_KHR)) {
		windowResize();
	}
	else {
		VK_CHECK_RESULT(result);
	}
}

void VulkanRaytracer::submitFrame()
{
	VkResult result = swapChain.queuePresent(queue, currentBuffer, semaphores.renderComplete);
	if (!((result == VK_SUCCESS) || (result == VK_SUBOPTIMAL_KHR))) {
		if (result == VK_ERROR_OUT_OF_DATE_KHR) {
			// Swap chain is no longer compatible with the surface and needs to be recreated
			windowResize();
			return;
		} else {
			VK_CHECK_RESULT(result);
		}
	}
	VK_CHECK_RESULT(vkQueueWaitIdle(queue));
}

VulkanRaytracer::VulkanRaytracer(const std::vector<std::string>& args)
{
#ifdef DEBUG
	settings.validation = true;
#else
	settings.validation = false;
#endif // DEBUG

	// Parse command line arguments
	for (size_t i = 0; i < args.size(); ++i)
	{
		if (args[i] == "-vsync")
		{
			settings.vsync = true;
		}
		// Benchmark
		else if (args[i] == "-b" || args[i] == "--benchmark") {
			benchmark.active = true;
		}
		// Warmup time (in seconds)
		else if (args[i] == "-bw" || args[i] == "--benchwarmup")
		{
			if (args.size() > i + 1)
			{
				benchmark.warmup = std::atoi(args[i + 1].c_str());
			}
		}
		// Benchmark runtime (in seconds)
		else if (args[i] == "-br" || args[i] == "--benchruntime")
		{
			if (args.size() > i + 1)
			{
				benchmark.duration = std::atoi(args[i + 1].c_str());
			}
		}
		// Bench result save filename (overrides default)
		else if (args[i] == "-bf" || args[i] == "--benchfilename")
		{
			if (args.size() > i + 1)
			{
				benchmark.filename = args[i + 1];
			}
		}
		// Output frame times to benchmark result file
		else if (args[i] == "-bt" || args[i] == "--benchframetimes")
		{
			benchmark.outputFrameTimes = true;
		}
		else if (args[i] == "-g" || args[i] == "-gpu")
		{
			selectedDevice = std::atoi(args[i + 1].c_str());
		}
	}

	scene = loadScene("E:\\Programming\\pt_gAPIs\\vulcan_empty2\\data\\scene1.test");

	height = scene.height;
	width = scene.width;
	scene.eye_init.z = -scene.eye_init.z;

	camera.setPerspective(scene.fovy, (float)width / (float)height, 0.1f, 512.0f);
	camera.setLookAt(scene.eye_init, scene.center, scene.up_init);

	// Enable instance and device extensions required to use VK_KHR_ray_tracing
	enabledInstanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
	enabledDeviceExtensions.push_back(VK_KHR_MAINTENANCE3_EXTENSION_NAME);
	enabledDeviceExtensions.push_back(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);
	enabledDeviceExtensions.push_back(VK_KHR_RAY_TRACING_EXTENSION_NAME);
	enabledDeviceExtensions.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
	enabledDeviceExtensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
	enabledDeviceExtensions.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
	enabledDeviceExtensions.push_back(VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME);
}

VulkanRaytracer::~VulkanRaytracer()
{
	vkDestroyPipeline(device, pipeline, nullptr);
	vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
	vkDestroyImageView(device, storageImage.view, nullptr);
	vkDestroyImage(device, storageImage.image, nullptr);
	vkFreeMemory(device, storageImage.memory, nullptr);
	vkDestroyAccelerationStructureKHR(device, bottomLevelAS.accelerationStructure, nullptr);
	vkDestroyAccelerationStructureKHR(device, topLevelAS.accelerationStructure, nullptr);
	vertexBuffer.destroy();
	indexBuffer.destroy();
	shaderBindingTable.destroy();
	ubo.destroy();

	if (bottomLevelAS.objectMemory.memory != VK_NULL_HANDLE) {
		vkFreeMemory(device, bottomLevelAS.objectMemory.memory, nullptr);
	}

	if (topLevelAS.objectMemory.memory != VK_NULL_HANDLE) {
		vkFreeMemory(device, topLevelAS.objectMemory.memory, nullptr);
	}

	// Clean up Vulkan resources
	swapChain.cleanup();
	if (descriptorPool != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorPool(device, descriptorPool, nullptr);
	}
	destroyCommandBuffers();
	vkDestroyRenderPass(device, renderPass, nullptr);
	for (uint32_t i = 0; i < frameBuffers.size(); i++)
	{
		vkDestroyFramebuffer(device, frameBuffers[i], nullptr);
	}

	for (auto& shaderModule : shaderModules)
	{
		vkDestroyShaderModule(device, shaderModule, nullptr);
	}
	vkDestroyImageView(device, depthStencil.view, nullptr);
	vkDestroyImage(device, depthStencil.image, nullptr);
	vkFreeMemory(device, depthStencil.mem, nullptr);

	vkDestroyPipelineCache(device, pipelineCache, nullptr);

	vkDestroyCommandPool(device, cmdPool, nullptr);

	vkDestroySemaphore(device, semaphores.presentComplete, nullptr);
	vkDestroySemaphore(device, semaphores.renderComplete, nullptr);
	for (auto& fence : waitFences) {
		vkDestroyFence(device, fence, nullptr);
	}

	delete vulkanDevice;

	if (settings.validation)
	{
		vks::debug::freeDebugCallback(instance);
	}

	vkDestroyInstance(instance, nullptr);

	glfwDestroyWindow(window);
	glfwTerminate();
}

bool VulkanRaytracer::initAPIs()
{
	glfwInit();

	// Vulkan instance
	 VkResult err = createInstance();
	if (err) {
		vks::tools::exitFatal("Could not create Vulkan instance : \n" + vks::tools::errorString(err), err);
		return false;
	}

	// If requested, we enable the default validation layers for debugging
	if (settings.validation)
	{
		// The report flags determine what type of messages for the layers will be displayed
		// For validating (debugging) an application the error and warning bits should suffice
		VkDebugReportFlagsEXT debugReportFlags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
		// Additional flags include performance info, loader and layer debug messages, etc.
		vks::debug::setupDebugging(instance, debugReportFlags, VK_NULL_HANDLE);
	}

	// Physical device
	uint32_t gpuCount = 0;
	// Get number of available physical devices
	VK_CHECK_RESULT(vkEnumeratePhysicalDevices(instance, &gpuCount, nullptr));
	assert(gpuCount > 0);
	// Enumerate devices
	std::vector<VkPhysicalDevice> physicalDevices(gpuCount);
	err = vkEnumeratePhysicalDevices(instance, &gpuCount, physicalDevices.data());
	if (err) {
		vks::tools::exitFatal("Could not enumerate physical devices : \n" + vks::tools::errorString(err), err);
		return false;
	}

	// List available GPUs
	std::cout << "Available Vulkan devices" << "\n";
	std::vector<VkPhysicalDevice> devices(gpuCount);
	VK_CHECK_RESULT(vkEnumeratePhysicalDevices(instance, &gpuCount, devices.data()));
	for (uint32_t j = 0; j < gpuCount; j++)
	{
		VkPhysicalDeviceProperties deviceProperties;
		vkGetPhysicalDeviceProperties(devices[j], &deviceProperties);
		std::cout << "Device [" << j << "] : " << deviceProperties.deviceName << std::endl;
		std::cout << " Type: " << vks::tools::physicalDeviceTypeString(deviceProperties.deviceType) << "\n";
		std::cout << " API: " << (deviceProperties.apiVersion >> 22) << "." << ((deviceProperties.apiVersion >> 12) & 0x3ff) << "." << (deviceProperties.apiVersion & 0xfff) << "\n";
	}

	// Select physical device
	physicalDevice = physicalDevices[selectedDevice];

	// Store properties (including limits), features and memory properties of the physical device (so that examples can check against them)
	vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
	vkGetPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &deviceMemoryProperties);

	// Derived examples can override this to set actual features (based on above readings) to enable for logical device creation
	getEnabledFeatures();

	// Vulkan device creation
	// This is handled by a separate class that gets a logical device representation
	// and encapsulates functions related to a device
	vulkanDevice = new vks::VulkanDevice(physicalDevice);
	VkResult res = vulkanDevice->createLogicalDevice(enabledFeatures, enabledDeviceExtensions, deviceCreatepNextChain);
	if (res != VK_SUCCESS) {
		vks::tools::exitFatal("Could not create Vulkan device: \n" + vks::tools::errorString(res), res);
		return false;
	}
	device = vulkanDevice->logicalDevice;

	// Get a graphics queue from the device
	vkGetDeviceQueue(device, vulkanDevice->queueFamilyIndices.graphics, 0, &queue);

	// Find a suitable depth format
	VkBool32 validDepthFormat = vks::tools::getSupportedDepthFormat(physicalDevice, &depthFormat);
	assert(validDepthFormat);

	swapChain.connect(instance, physicalDevice, device);

	// Create synchronization objects
	VkSemaphoreCreateInfo semaphoreCreateInfo = vks::initializers::semaphoreCreateInfo();
	// Create a semaphore used to synchronize image presentation
	// Ensures that the image is displayed before we start submitting new commands to the queue
	VK_CHECK_RESULT(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &semaphores.presentComplete));
	// Create a semaphore used to synchronize command submission
	// Ensures that the image is not presented until all commands have been submitted and executed
	VK_CHECK_RESULT(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &semaphores.renderComplete));

	// Set up submit info structure
	// Semaphores will stay the same during application lifetime
	// Command buffer submission info is set by each example
	submitInfo = vks::initializers::submitInfo();
	submitInfo.pWaitDstStageMask = &submitPipelineStages;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &semaphores.presentComplete;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &semaphores.renderComplete;

	return true;
}

void VulkanRaytracer::setupWindow()
{
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	window = glfwCreateWindow(width, height, "Vulkan", nullptr, nullptr);
	glfwSetWindowUserPointer(window, this);
	glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
	glfwSetCursorPosCallback(window, cursorPositionCallback);
	glfwSetMouseButtonCallback(window, mouseButtonCallback);
	glfwSetMouseButtonCallback(window, mouseButtonCallback);
	glfwSetKeyCallback(window, keyCallback);
}

void VulkanRaytracer::createSynchronizationPrimitives()
{
	// Wait fences to sync command buffer access
	VkFenceCreateInfo fenceCreateInfo = vks::initializers::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
	waitFences.resize(drawCmdBuffers.size());
	for (auto& fence : waitFences) {
		VK_CHECK_RESULT(vkCreateFence(device, &fenceCreateInfo, nullptr, &fence));
	}
}

void VulkanRaytracer::createCommandPool()
{
	VkCommandPoolCreateInfo cmdPoolInfo = {};
	cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cmdPoolInfo.queueFamilyIndex = swapChain.queueNodeIndex;
	//cmdPoolInfo.queueFamilyIndex = vulkanDevice->queueFamilyIndices.graphics;
	cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	VK_CHECK_RESULT(vkCreateCommandPool(device, &cmdPoolInfo, nullptr, &cmdPool));
}

void VulkanRaytracer::setupDepthStencil()
{
	VkImageCreateInfo imageCI{};
	imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCI.imageType = VK_IMAGE_TYPE_2D;
	imageCI.format = depthFormat;
	imageCI.extent = { width, height, 1 };
	imageCI.mipLevels = 1;
	imageCI.arrayLayers = 1;
	imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCI.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

	VK_CHECK_RESULT(vkCreateImage(device, &imageCI, nullptr, &depthStencil.image));
	VkMemoryRequirements memReqs{};
	vkGetImageMemoryRequirements(device, depthStencil.image, &memReqs);

	VkMemoryAllocateInfo memAllloc{};
	memAllloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memAllloc.allocationSize = memReqs.size;
	memAllloc.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(device, &memAllloc, nullptr, &depthStencil.mem));
	VK_CHECK_RESULT(vkBindImageMemory(device, depthStencil.image, depthStencil.mem, 0));

	VkImageViewCreateInfo imageViewCI{};
	imageViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
	imageViewCI.image = depthStencil.image;
	imageViewCI.format = depthFormat;
	imageViewCI.subresourceRange.baseMipLevel = 0;
	imageViewCI.subresourceRange.levelCount = 1;
	imageViewCI.subresourceRange.baseArrayLayer = 0;
	imageViewCI.subresourceRange.layerCount = 1;
	imageViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	// Stencil aspect should only be set on depth + stencil formats (VK_FORMAT_D16_UNORM_S8_UINT..VK_FORMAT_D32_SFLOAT_S8_UINT
	if (depthFormat >= VK_FORMAT_D16_UNORM_S8_UINT) {
		imageViewCI.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}
	VK_CHECK_RESULT(vkCreateImageView(device, &imageViewCI, nullptr, &depthStencil.view));
}

void VulkanRaytracer::setupFrameBuffer()
{
	VkImageView attachments[2];

	// Depth/Stencil attachment is the same for all frame buffers
	attachments[1] = depthStencil.view;

	VkFramebufferCreateInfo frameBufferCreateInfo = {};
	frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	frameBufferCreateInfo.pNext = NULL;
	frameBufferCreateInfo.renderPass = renderPass;
	frameBufferCreateInfo.attachmentCount = 2;
	frameBufferCreateInfo.pAttachments = attachments;
	frameBufferCreateInfo.width = width;
	frameBufferCreateInfo.height = height;
	frameBufferCreateInfo.layers = 1;

	// Create frame buffers for every swap chain image
	frameBuffers.resize(swapChain.imageCount);
	for (uint32_t i = 0; i < frameBuffers.size(); i++)
	{
		attachments[0] = swapChain.buffers[i].view;
		VK_CHECK_RESULT(vkCreateFramebuffer(device, &frameBufferCreateInfo, nullptr, &frameBuffers[i]));
	}
}

void VulkanRaytracer::setupRenderPass()
{
	std::array<VkAttachmentDescription, 2> attachments = {};
	// Color attachment
	attachments[0].format = swapChain.colorFormat;
	attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	// Depth attachment
	attachments[1].format = depthFormat;
	attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference colorReference = {};
	colorReference.attachment = 0;
	colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthReference = {};
	depthReference.attachment = 1;
	depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpassDescription = {};
	subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpassDescription.colorAttachmentCount = 1;
	subpassDescription.pColorAttachments = &colorReference;
	subpassDescription.pDepthStencilAttachment = &depthReference;
	subpassDescription.inputAttachmentCount = 0;
	subpassDescription.pInputAttachments = nullptr;
	subpassDescription.preserveAttachmentCount = 0;
	subpassDescription.pPreserveAttachments = nullptr;
	subpassDescription.pResolveAttachments = nullptr;

	// Subpass dependencies for layout transitions
	std::array<VkSubpassDependency, 2> dependencies;

	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	dependencies[1].srcSubpass = 0;
	dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpassDescription;
	renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
	renderPassInfo.pDependencies = dependencies.data();

	VK_CHECK_RESULT(vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass));
}

void VulkanRaytracer::windowResize()
{
	if (!prepared)
	{
		return;
	}
	prepared = false;
	resized = true;

	// Ensure all operations on the device have been finished before destroying resources
	vkDeviceWaitIdle(device);

	// Recreate swap chain
	width = destWidth;
	height = destHeight;
	setupSwapChain();

	// Recreate the frame buffers
	vkDestroyImageView(device, depthStencil.view, nullptr);
	vkDestroyImage(device, depthStencil.image, nullptr);
	vkFreeMemory(device, depthStencil.mem, nullptr);
	setupDepthStencil();
	for (uint32_t i = 0; i < frameBuffers.size(); i++) {
		vkDestroyFramebuffer(device, frameBuffers[i], nullptr);
	}
	setupFrameBuffer();

	// Command buffers need to be recreated as they may store
	// references to the recreated frame buffer
	destroyCommandBuffers();
	createCommandBuffers();
	buildCommandBuffers();

	vkDeviceWaitIdle(device);

	if ((width > 0.0f) && (height > 0.0f)) {
		camera.updateAspectRatio((float)width / (float)height);
	}

	prepared = true;
}

void VulkanRaytracer::initSwapchain()
{
	swapChain.initSurface(window);
}

void VulkanRaytracer::setupSwapChain()
{
	swapChain.create(&width, &height, settings.vsync);
}

/*
	Create a scratch buffer to hold temporary data for a ray tracing acceleration structure
*/
RayTracingScratchBuffer VulkanRaytracer::createScratchBuffer(VkAccelerationStructureKHR accelerationStructure)
{
	RayTracingScratchBuffer scratchBuffer{};

	VkMemoryRequirements2 memoryRequirements2{};
	memoryRequirements2.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;

	VkAccelerationStructureMemoryRequirementsInfoKHR accelerationStructureMemoryRequirements{};
	accelerationStructureMemoryRequirements.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_KHR;
	accelerationStructureMemoryRequirements.type = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_BUILD_SCRATCH_KHR;
	accelerationStructureMemoryRequirements.buildType = VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR;
	accelerationStructureMemoryRequirements.accelerationStructure = accelerationStructure;
	vkGetAccelerationStructureMemoryRequirementsKHR(device, &accelerationStructureMemoryRequirements, &memoryRequirements2);

	VkBufferCreateInfo bufferCI{};
	bufferCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCI.size = memoryRequirements2.memoryRequirements.size;
	bufferCI.usage = VK_BUFFER_USAGE_RAY_TRACING_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	bufferCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	VK_CHECK_RESULT(vkCreateBuffer(device, &bufferCI, nullptr, &scratchBuffer.buffer));

	VkMemoryRequirements memoryRequirements{};
	vkGetBufferMemoryRequirements(device, scratchBuffer.buffer, &memoryRequirements);

	VkMemoryAllocateFlagsInfo memoryAllocateFI{};
	memoryAllocateFI.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
	memoryAllocateFI.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;

	VkMemoryAllocateInfo memoryAI{};
	memoryAI.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memoryAI.pNext = &memoryAllocateFI;
	memoryAI.allocationSize = memoryRequirements.size;
	memoryAI.memoryTypeIndex = vulkanDevice->getMemoryType(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(device, &memoryAI, nullptr, &scratchBuffer.memory));
	VK_CHECK_RESULT(vkBindBufferMemory(device, scratchBuffer.buffer, scratchBuffer.memory, 0));

	VkBufferDeviceAddressInfoKHR buffer_device_address_info{};
	buffer_device_address_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	buffer_device_address_info.buffer = scratchBuffer.buffer;
	scratchBuffer.deviceAddress = vkGetBufferDeviceAddressKHR(device, &buffer_device_address_info);

	return scratchBuffer;
}

void VulkanRaytracer::deleteScratchBuffer(RayTracingScratchBuffer& scratchBuffer)
{
	if (scratchBuffer.memory != VK_NULL_HANDLE) {
		vkFreeMemory(device, scratchBuffer.memory, nullptr);
	}
	if (scratchBuffer.buffer != VK_NULL_HANDLE) {
		vkDestroyBuffer(device, scratchBuffer.buffer, nullptr);
	}
}

/*
	Allocate memory that will be attached to a ray tracing acceleration structure
*/
RayTracingObjectMemory VulkanRaytracer::createObjectMemory(VkAccelerationStructureKHR acceleration_structure)
{
	RayTracingObjectMemory objectMemory{};

	VkMemoryRequirements2 memoryRequirements2{};
	memoryRequirements2.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;

	VkAccelerationStructureMemoryRequirementsInfoKHR accelerationStructureMemoryRequirements{};
	accelerationStructureMemoryRequirements.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_KHR;
	accelerationStructureMemoryRequirements.type = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_OBJECT_KHR;
	accelerationStructureMemoryRequirements.buildType = VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR;
	accelerationStructureMemoryRequirements.accelerationStructure = acceleration_structure;
	vkGetAccelerationStructureMemoryRequirementsKHR(device, &accelerationStructureMemoryRequirements, &memoryRequirements2);

	VkMemoryRequirements memoryRequirements = memoryRequirements2.memoryRequirements;

	VkMemoryAllocateInfo memoryAI{};
	memoryAI.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memoryAI.allocationSize = memoryRequirements.size;
	memoryAI.memoryTypeIndex = vulkanDevice->getMemoryType(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(device, &memoryAI, nullptr, &objectMemory.memory));

	return objectMemory;
}

/*
	Gets the device address from a buffer that's required for some of the buffers used for ray tracing
*/
uint64_t VulkanRaytracer::getBufferDeviceAddress(VkBuffer buffer)
{
	VkBufferDeviceAddressInfoKHR bufferDeviceAI{};
	bufferDeviceAI.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	bufferDeviceAI.buffer = buffer;
	return vkGetBufferDeviceAddressKHR(device, &bufferDeviceAI);
}

/*
	Set up a storage image that the ray generation shader will be writing to
*/
void VulkanRaytracer::createStorageImage()
{
	VkImageCreateInfo image = vks::initializers::imageCreateInfo();
	image.imageType = VK_IMAGE_TYPE_2D;
	image.format = swapChain.colorFormat;
	image.extent.width = width;
	image.extent.height = height;
	image.extent.depth = 1;
	image.mipLevels = 1;
	image.arrayLayers = 1;
	image.samples = VK_SAMPLE_COUNT_1_BIT;
	image.tiling = VK_IMAGE_TILING_OPTIMAL;
	image.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
	image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VK_CHECK_RESULT(vkCreateImage(device, &image, nullptr, &storageImage.image));

	VkMemoryRequirements memReqs;
	vkGetImageMemoryRequirements(device, storageImage.image, &memReqs);
	VkMemoryAllocateInfo memoryAllocateInfo = vks::initializers::memoryAllocateInfo();
	memoryAllocateInfo.allocationSize = memReqs.size;
	memoryAllocateInfo.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(device, &memoryAllocateInfo, nullptr, &storageImage.memory));
	VK_CHECK_RESULT(vkBindImageMemory(device, storageImage.image, storageImage.memory, 0));

	VkImageViewCreateInfo colorImageView = vks::initializers::imageViewCreateInfo();
	colorImageView.viewType = VK_IMAGE_VIEW_TYPE_2D;
	colorImageView.format = swapChain.colorFormat;
	colorImageView.subresourceRange = {};
	colorImageView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	colorImageView.subresourceRange.baseMipLevel = 0;
	colorImageView.subresourceRange.levelCount = 1;
	colorImageView.subresourceRange.baseArrayLayer = 0;
	colorImageView.subresourceRange.layerCount = 1;
	colorImageView.image = storageImage.image;
	VK_CHECK_RESULT(vkCreateImageView(device, &colorImageView, nullptr, &storageImage.view));

	VkCommandBuffer cmdBuffer = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
	vks::tools::setImageLayout(cmdBuffer, storageImage.image,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_GENERAL,
		{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
	vulkanDevice->flushCommandBuffer(cmdBuffer, queue);
}

/*
	Create the bottom level acceleration structure contains the scene's actual geometry (vertices, triangles)
*/
void VulkanRaytracer::createBottomLevelAccelerationStructure()
{
	auto vertices = scene.vertices;
	auto indices = scene.indices;
	uint32_t numTriangles = indices.size() / 3;

	// Create buffers
	// For the sake of simplicity we won't stage the vertex data to the GPU memory
	// Vertex buffer
	VK_CHECK_RESULT(vulkanDevice->createBuffer(
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&vertexBuffer,
		vertices.size() * sizeof(vec3),
		vertices.data()));
	// Index buffer
	VK_CHECK_RESULT(vulkanDevice->createBuffer(
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&indexBuffer,
		indices.size() * sizeof(uint32_t),
		indices.data()));

	VkDeviceOrHostAddressConstKHR vertexBufferDeviceAddress{};
	VkDeviceOrHostAddressConstKHR indexBufferDeviceAddress{};

	vertexBufferDeviceAddress.deviceAddress = getBufferDeviceAddress(vertexBuffer.buffer);
	indexBufferDeviceAddress.deviceAddress = getBufferDeviceAddress(indexBuffer.buffer);

	VkAccelerationStructureCreateGeometryTypeInfoKHR accelerationCreateGeometryInfo{};
	accelerationCreateGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_GEOMETRY_TYPE_INFO_KHR;
	accelerationCreateGeometryInfo.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
	accelerationCreateGeometryInfo.maxPrimitiveCount = numTriangles;
	accelerationCreateGeometryInfo.indexType = VK_INDEX_TYPE_UINT32;
	accelerationCreateGeometryInfo.maxVertexCount = static_cast<uint32_t>(vertices.size());
	accelerationCreateGeometryInfo.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
	accelerationCreateGeometryInfo.allowsTransforms = VK_FALSE;

	VkAccelerationStructureCreateInfoKHR accelerationCI{};
	accelerationCI.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
	accelerationCI.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	accelerationCI.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	accelerationCI.maxGeometryCount = 1;
	accelerationCI.pGeometryInfos = &accelerationCreateGeometryInfo;
	VK_CHECK_RESULT(vkCreateAccelerationStructureKHR(device, &accelerationCI, nullptr, &bottomLevelAS.accelerationStructure));

	// Bind object memory to the top level acceleration structure
	bottomLevelAS.objectMemory = createObjectMemory(bottomLevelAS.accelerationStructure);

	VkBindAccelerationStructureMemoryInfoKHR bindAccelerationMemoryInfo{};
	bindAccelerationMemoryInfo.sType = VK_STRUCTURE_TYPE_BIND_ACCELERATION_STRUCTURE_MEMORY_INFO_KHR;
	bindAccelerationMemoryInfo.accelerationStructure = bottomLevelAS.accelerationStructure;
	bindAccelerationMemoryInfo.memory = bottomLevelAS.objectMemory.memory;
	VK_CHECK_RESULT(vkBindAccelerationStructureMemoryKHR(device, 1, &bindAccelerationMemoryInfo));

	VkAccelerationStructureGeometryKHR accelerationStructureGeometry{};
	accelerationStructureGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	accelerationStructureGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
	accelerationStructureGeometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
	accelerationStructureGeometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
	accelerationStructureGeometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
	accelerationStructureGeometry.geometry.triangles.vertexData.deviceAddress = vertexBufferDeviceAddress.deviceAddress;
	accelerationStructureGeometry.geometry.triangles.vertexStride = sizeof(vec3);
	accelerationStructureGeometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
	accelerationStructureGeometry.geometry.triangles.indexData.deviceAddress = indexBufferDeviceAddress.deviceAddress;

	std::vector<VkAccelerationStructureGeometryKHR> acceleration_geometries = { accelerationStructureGeometry };
	VkAccelerationStructureGeometryKHR* acceleration_structure_geometries = acceleration_geometries.data();

	// Create a small scratch buffer used during build of the bottom level acceleration structure
	RayTracingScratchBuffer scratchBuffer = createScratchBuffer(bottomLevelAS.accelerationStructure);

	VkAccelerationStructureBuildGeometryInfoKHR accelerationBuildGeometryInfo{};
	accelerationBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	accelerationBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	accelerationBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	accelerationBuildGeometryInfo.update = VK_FALSE;
	accelerationBuildGeometryInfo.dstAccelerationStructure = bottomLevelAS.accelerationStructure;
	accelerationBuildGeometryInfo.geometryArrayOfPointers = VK_FALSE;
	accelerationBuildGeometryInfo.geometryCount = 1;
	accelerationBuildGeometryInfo.ppGeometries = &acceleration_structure_geometries;
	accelerationBuildGeometryInfo.scratchData.deviceAddress = scratchBuffer.deviceAddress;

	VkAccelerationStructureBuildOffsetInfoKHR accelerationBuildOffsetInfo{};
	accelerationBuildOffsetInfo.primitiveCount = numTriangles;
	accelerationBuildOffsetInfo.primitiveOffset = 0x0;
	accelerationBuildOffsetInfo.firstVertex = 0;
	accelerationBuildOffsetInfo.transformOffset = 0x0;

	std::vector<VkAccelerationStructureBuildOffsetInfoKHR*> accelerationBuildOffsets = { &accelerationBuildOffsetInfo };

	if (rayTracingFeatures.rayTracingHostAccelerationStructureCommands)
	{
		// Implementation supports building acceleration structure building on host
		VK_CHECK_RESULT(vkBuildAccelerationStructureKHR(device, 1, &accelerationBuildGeometryInfo, accelerationBuildOffsets.data()));
	}
	else
	{
		// Acceleration structure needs to be build on the device
		VkCommandBuffer commandBuffer = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		vkCmdBuildAccelerationStructureKHR(commandBuffer, 1, &accelerationBuildGeometryInfo, accelerationBuildOffsets.data());
		vulkanDevice->flushCommandBuffer(commandBuffer, queue);
	}

	VkAccelerationStructureDeviceAddressInfoKHR accelerationDeviceAddressInfo{};
	accelerationDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
	accelerationDeviceAddressInfo.accelerationStructure = bottomLevelAS.accelerationStructure;

	bottomLevelAS.handle = vkGetAccelerationStructureDeviceAddressKHR(device, &accelerationDeviceAddressInfo);

	deleteScratchBuffer(scratchBuffer);
}

/*
	The top level acceleration structure contains the scene's object instances
*/
void VulkanRaytracer::createTopLevelAccelerationStructure()
{
	VkAccelerationStructureCreateGeometryTypeInfoKHR accelerationCreateGeometryInfo{};
	accelerationCreateGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_GEOMETRY_TYPE_INFO_KHR;
	accelerationCreateGeometryInfo.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
	accelerationCreateGeometryInfo.maxPrimitiveCount = 1;
	accelerationCreateGeometryInfo.allowsTransforms = VK_FALSE;

	VkAccelerationStructureCreateInfoKHR accelerationCI{};
	accelerationCI.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
	accelerationCI.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	accelerationCI.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	accelerationCI.maxGeometryCount = 1;
	accelerationCI.pGeometryInfos = &accelerationCreateGeometryInfo;
	VK_CHECK_RESULT(vkCreateAccelerationStructureKHR(device, &accelerationCI, nullptr, &topLevelAS.accelerationStructure));

	// Bind object memory to the top level acceleration structure
	topLevelAS.objectMemory = createObjectMemory(topLevelAS.accelerationStructure);

	VkBindAccelerationStructureMemoryInfoKHR bindAccelerationMemoryInfo{};
	bindAccelerationMemoryInfo.sType = VK_STRUCTURE_TYPE_BIND_ACCELERATION_STRUCTURE_MEMORY_INFO_KHR;
	bindAccelerationMemoryInfo.accelerationStructure = topLevelAS.accelerationStructure;
	bindAccelerationMemoryInfo.memory = topLevelAS.objectMemory.memory;
	VK_CHECK_RESULT(vkBindAccelerationStructureMemoryKHR(device, 1, &bindAccelerationMemoryInfo));

	VkTransformMatrixKHR transform_matrix = {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f };

	VkAccelerationStructureInstanceKHR instance{};
	instance.transform = transform_matrix;
	instance.instanceCustomIndex = 0;
	instance.mask = 0xFF;
	instance.instanceShaderBindingTableRecordOffset = 0;
	instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
	instance.accelerationStructureReference = bottomLevelAS.handle;

	// Buffer for instance data
	vks::Buffer instancesBuffer;
	VK_CHECK_RESULT(vulkanDevice->createBuffer(
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&instancesBuffer,
		sizeof(instance),
		&instance));

	VkDeviceOrHostAddressConstKHR instance_data_device_address{};
	instance_data_device_address.deviceAddress = getBufferDeviceAddress(instancesBuffer.buffer);

	VkAccelerationStructureGeometryKHR accelerationStructureGeometry{};
	accelerationStructureGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	accelerationStructureGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
	accelerationStructureGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
	accelerationStructureGeometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
	accelerationStructureGeometry.geometry.instances.arrayOfPointers = VK_FALSE;
	accelerationStructureGeometry.geometry.instances.data.deviceAddress = instance_data_device_address.deviceAddress;

	std::vector<VkAccelerationStructureGeometryKHR> acceleration_geometries = { accelerationStructureGeometry };
	VkAccelerationStructureGeometryKHR* acceleration_structure_geometries = acceleration_geometries.data();

	// Create a small scratch buffer used during build of the top level acceleration structure
	RayTracingScratchBuffer scratchBuffer = createScratchBuffer(topLevelAS.accelerationStructure);

	VkAccelerationStructureBuildGeometryInfoKHR accelerationBuildGeometryInfo{};
	accelerationBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	accelerationBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	accelerationBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	accelerationBuildGeometryInfo.update = VK_FALSE;
	accelerationBuildGeometryInfo.srcAccelerationStructure = VK_NULL_HANDLE;
	accelerationBuildGeometryInfo.dstAccelerationStructure = topLevelAS.accelerationStructure;
	accelerationBuildGeometryInfo.geometryArrayOfPointers = VK_FALSE;
	accelerationBuildGeometryInfo.geometryCount = 1;
	accelerationBuildGeometryInfo.ppGeometries = &acceleration_structure_geometries;
	accelerationBuildGeometryInfo.scratchData.deviceAddress = scratchBuffer.deviceAddress;

	VkAccelerationStructureBuildOffsetInfoKHR accelerationBuildOffsetInfo{};
	accelerationBuildOffsetInfo.primitiveCount = 1;
	accelerationBuildOffsetInfo.primitiveOffset = 0x0;
	accelerationBuildOffsetInfo.firstVertex = 0;
	accelerationBuildOffsetInfo.transformOffset = 0x0;
	std::vector<VkAccelerationStructureBuildOffsetInfoKHR*> accelerationBuildOffsets = { &accelerationBuildOffsetInfo };

	if (rayTracingFeatures.rayTracingHostAccelerationStructureCommands)
	{
		// Implementation supports building acceleration structure building on host
		VK_CHECK_RESULT(vkBuildAccelerationStructureKHR(device, 1, &accelerationBuildGeometryInfo, accelerationBuildOffsets.data()));
	}
	else
	{
		// Acceleration structure needs to be build on the device
		VkCommandBuffer commandBuffer = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		vkCmdBuildAccelerationStructureKHR(commandBuffer, 1, &accelerationBuildGeometryInfo, accelerationBuildOffsets.data());
		vulkanDevice->flushCommandBuffer(commandBuffer, queue);
	}

	VkAccelerationStructureDeviceAddressInfoKHR accelerationDeviceAddressInfo{};
	accelerationDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
	accelerationDeviceAddressInfo.accelerationStructure = topLevelAS.accelerationStructure;

	topLevelAS.handle = vkGetAccelerationStructureDeviceAddressKHR(device, &accelerationDeviceAddressInfo);

	deleteScratchBuffer(scratchBuffer);
	instancesBuffer.destroy();
}

/*
	Create the Shader Binding Table that binds the programs and top-level acceleration structure
*/
void VulkanRaytracer::createShaderBindingTable() {
	const uint32_t groupCount = static_cast<uint32_t>(shaderGroups.size());

	const uint32_t sbtSize = rayTracingProperties.shaderGroupBaseAlignment * groupCount;
	VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_RAY_TRACING_BIT_KHR, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &shaderBindingTable, sbtSize));
	shaderBindingTable.map();

	// Write the shader handles to the shader binding table
	std::vector<uint8_t> shaderHandleStorage(sbtSize);
	VK_CHECK_RESULT(vkGetRayTracingShaderGroupHandlesKHR(device, pipeline, 0, groupCount, sbtSize, shaderHandleStorage.data()));

	auto* data = static_cast<uint8_t*>(shaderBindingTable.mapped);
	// This part is required, as the alignment and handle size may differ
	for (uint32_t i = 0; i < groupCount; i++)
	{
		memcpy(data, shaderHandleStorage.data() + i * rayTracingProperties.shaderGroupHandleSize, rayTracingProperties.shaderGroupHandleSize);
		data += rayTracingProperties.shaderGroupBaseAlignment;
	}
	shaderBindingTable.unmap();
}

/*
	Create the descriptor sets used for the ray tracing dispatch
*/
void VulkanRaytracer::createDescriptorSets()
{
	std::vector<VkDescriptorPoolSize> poolSizes = {
		{ VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 }
	};
	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, 1);
	VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, nullptr, &descriptorPool));

	VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);
	VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &descriptorSet));

	VkWriteDescriptorSetAccelerationStructureKHR descriptorAccelerationStructureInfo{};
	descriptorAccelerationStructureInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
	descriptorAccelerationStructureInfo.accelerationStructureCount = 1;
	descriptorAccelerationStructureInfo.pAccelerationStructures = &topLevelAS.accelerationStructure;

	VkWriteDescriptorSet accelerationStructureWrite{};
	accelerationStructureWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	// The specialized acceleration structure descriptor has to be chained
	accelerationStructureWrite.pNext = &descriptorAccelerationStructureInfo;
	accelerationStructureWrite.dstSet = descriptorSet;
	accelerationStructureWrite.dstBinding = 0;
	accelerationStructureWrite.descriptorCount = 1;
	accelerationStructureWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

	VkDescriptorImageInfo storageImageDescriptor{};
	storageImageDescriptor.imageView = storageImage.view;
	storageImageDescriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

	VkWriteDescriptorSet resultImageWrite = vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, &storageImageDescriptor);
	VkWriteDescriptorSet uniformBufferWrite = vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2, &ubo.descriptor);

	std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
		accelerationStructureWrite,
		resultImageWrite,
		uniformBufferWrite
	};
	vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, VK_NULL_HANDLE);
}

/*
	Create our ray tracing pipeline
*/
void VulkanRaytracer::createRayTracingPipeline()
{
	VkDescriptorSetLayoutBinding accelerationStructureLayoutBinding{};
	accelerationStructureLayoutBinding.binding = 0;
	accelerationStructureLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	accelerationStructureLayoutBinding.descriptorCount = 1;
	accelerationStructureLayoutBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

	VkDescriptorSetLayoutBinding resultImageLayoutBinding{};
	resultImageLayoutBinding.binding = 1;
	resultImageLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	resultImageLayoutBinding.descriptorCount = 1;
	resultImageLayoutBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

	VkDescriptorSetLayoutBinding uniformBufferBinding{};
	uniformBufferBinding.binding = 2;
	uniformBufferBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uniformBufferBinding.descriptorCount = 1;
	uniformBufferBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

	std::vector<VkDescriptorSetLayoutBinding> bindings({
		accelerationStructureLayoutBinding,
		resultImageLayoutBinding,
		uniformBufferBinding
		});

	VkDescriptorSetLayoutCreateInfo descriptorSetlayoutCI{};
	descriptorSetlayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptorSetlayoutCI.bindingCount = static_cast<uint32_t>(bindings.size());
	descriptorSetlayoutCI.pBindings = bindings.data();
	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorSetlayoutCI, nullptr, &descriptorSetLayout));

	VkPipelineLayoutCreateInfo pipelineLayoutCI{};
	pipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCI.setLayoutCount = 1;
	pipelineLayoutCI.pSetLayouts = &descriptorSetLayout;
	VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipelineLayout));

	const uint32_t shaderIndexRaygen = 0;
	const uint32_t shaderIndexMiss = 1;
	const uint32_t shaderIndexClosestHit = 2;

	std::array<VkPipelineShaderStageCreateInfo, 3> shaderStages;
	shaderStages[shaderIndexRaygen] = loadShader("shaders/raygen.rgen.spv", VK_SHADER_STAGE_RAYGEN_BIT_KHR);
	shaderStages[shaderIndexMiss] = loadShader("shaders/miss.rmiss.spv", VK_SHADER_STAGE_MISS_BIT_KHR);
	shaderStages[shaderIndexClosestHit] = loadShader("shaders/closesthit.rchit.spv", VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);

	/*
		Setup ray tracing shader groups
	*/
	VkRayTracingShaderGroupCreateInfoKHR raygenGroupCI{};
	raygenGroupCI.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	raygenGroupCI.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	raygenGroupCI.generalShader = shaderIndexRaygen;
	raygenGroupCI.closestHitShader = VK_SHADER_UNUSED_KHR;
	raygenGroupCI.anyHitShader = VK_SHADER_UNUSED_KHR;
	raygenGroupCI.intersectionShader = VK_SHADER_UNUSED_KHR;
	shaderGroups.push_back(raygenGroupCI);

	VkRayTracingShaderGroupCreateInfoKHR missGroupCI{};
	missGroupCI.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	missGroupCI.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	missGroupCI.generalShader = shaderIndexMiss;
	missGroupCI.closestHitShader = VK_SHADER_UNUSED_KHR;
	missGroupCI.anyHitShader = VK_SHADER_UNUSED_KHR;
	missGroupCI.intersectionShader = VK_SHADER_UNUSED_KHR;
	shaderGroups.push_back(missGroupCI);

	VkRayTracingShaderGroupCreateInfoKHR closesHitGroupCI{};
	closesHitGroupCI.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	closesHitGroupCI.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
	closesHitGroupCI.generalShader = VK_SHADER_UNUSED_KHR;
	closesHitGroupCI.closestHitShader = shaderIndexClosestHit;
	closesHitGroupCI.anyHitShader = VK_SHADER_UNUSED_KHR;
	closesHitGroupCI.intersectionShader = VK_SHADER_UNUSED_KHR;
	shaderGroups.push_back(closesHitGroupCI);

	VkRayTracingPipelineCreateInfoKHR rayTracingPipelineCI{};
	rayTracingPipelineCI.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
	rayTracingPipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
	rayTracingPipelineCI.pStages = shaderStages.data();
	rayTracingPipelineCI.groupCount = static_cast<uint32_t>(shaderGroups.size());
	rayTracingPipelineCI.pGroups = shaderGroups.data();
	rayTracingPipelineCI.maxRecursionDepth = 1;
	rayTracingPipelineCI.layout = pipelineLayout;
	rayTracingPipelineCI.libraries.sType = VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR;
	VK_CHECK_RESULT(vkCreateRayTracingPipelinesKHR(device, VK_NULL_HANDLE, 1, &rayTracingPipelineCI, nullptr, &pipeline));
}

/*
	Create the uniform buffer used to pass matrices to the ray tracing ray generation shader
*/
void VulkanRaytracer::createUniformBuffer()
{
	VK_CHECK_RESULT(vulkanDevice->createBuffer(
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&ubo,
		sizeof(uniformData),
		&uniformData));
	VK_CHECK_RESULT(ubo.map());

	updateUniformBuffers();
}

/*
	If the window has been resized, we need to recreate the storage image and it's descriptor
*/
void VulkanRaytracer::handleResize()
{
	// Delete allocated resources
	vkDestroyImageView(device, storageImage.view, nullptr);
	vkDestroyImage(device, storageImage.image, nullptr);
	vkFreeMemory(device, storageImage.memory, nullptr);
	// Recreate image
	createStorageImage();
	// Update descriptor
	VkDescriptorImageInfo storageImageDescriptor{ VK_NULL_HANDLE, storageImage.view, VK_IMAGE_LAYOUT_GENERAL };
	VkWriteDescriptorSet resultImageWrite = vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, &storageImageDescriptor);
	vkUpdateDescriptorSets(device, 1, &resultImageWrite, 0, VK_NULL_HANDLE);
}

/*
	Command buffer generation
*/
void VulkanRaytracer::buildCommandBuffers()
{
	if (resized)
	{
		handleResize();
	}

	VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();

	VkImageSubresourceRange subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

	for (int32_t i = 0; i < drawCmdBuffers.size(); ++i)
	{
		VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo));

		/*
			Dispatch the ray tracing commands
		*/
		vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);
		vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelineLayout, 0, 1, &descriptorSet, 0, 0);


		/*
			Setup the buffer regions pointing to the shaders in our shader binding table
		*/
		const VkDeviceSize sbtSize = rayTracingProperties.shaderGroupBaseAlignment * (VkDeviceSize)shaderGroups.size();

		VkStridedBufferRegionKHR raygenShaderSBTEntry{};
		raygenShaderSBTEntry.buffer = shaderBindingTable.buffer;
		raygenShaderSBTEntry.offset = static_cast<VkDeviceSize>(rayTracingProperties.shaderGroupBaseAlignment * INDEX_RAYGEN_GROUP);
		raygenShaderSBTEntry.stride = rayTracingProperties.shaderGroupBaseAlignment;
		raygenShaderSBTEntry.size = sbtSize;

		VkStridedBufferRegionKHR missShaderSBTEntry{};
		missShaderSBTEntry.buffer = shaderBindingTable.buffer;
		missShaderSBTEntry.offset = static_cast<VkDeviceSize>(rayTracingProperties.shaderGroupBaseAlignment * INDEX_MISS_GROUP);
		missShaderSBTEntry.stride = rayTracingProperties.shaderGroupBaseAlignment;
		missShaderSBTEntry.size = sbtSize;

		VkStridedBufferRegionKHR hitShaderSBTEntry{};
		hitShaderSBTEntry.buffer = shaderBindingTable.buffer;
		hitShaderSBTEntry.offset = static_cast<VkDeviceSize>(rayTracingProperties.shaderGroupBaseAlignment * INDEX_CLOSEST_HIT_GROUP);
		hitShaderSBTEntry.stride = rayTracingProperties.shaderGroupBaseAlignment;
		hitShaderSBTEntry.size = sbtSize;

		VkStridedBufferRegionKHR callableShaderSBTEntry{};

		/*
			Dispatch the ray tracing commands
		*/
		vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);
		vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelineLayout, 0, 1, &descriptorSet, 0, 0);

		vkCmdTraceRaysKHR(
			drawCmdBuffers[i],
			&raygenShaderSBTEntry,
			&missShaderSBTEntry,
			&hitShaderSBTEntry,
			&callableShaderSBTEntry,
			width,
			height,
			1);

		/*
			Copy ray tracing output to swap chain image
		*/

		// Prepare current swap chain image as transfer destination
		vks::tools::setImageLayout(
			drawCmdBuffers[i],
			swapChain.images[i],
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			subresourceRange);

		// Prepare ray tracing output image as transfer source
		vks::tools::setImageLayout(
			drawCmdBuffers[i],
			storageImage.image,
			VK_IMAGE_LAYOUT_GENERAL,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			subresourceRange);

		VkImageCopy copyRegion{};
		copyRegion.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
		copyRegion.srcOffset = { 0, 0, 0 };
		copyRegion.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
		copyRegion.dstOffset = { 0, 0, 0 };
		copyRegion.extent = { width, height, 1 };
		vkCmdCopyImage(drawCmdBuffers[i], storageImage.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, swapChain.images[i], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

		// Transition swap chain image back for presentation
		vks::tools::setImageLayout(
			drawCmdBuffers[i],
			swapChain.images[i],
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			subresourceRange);

		// Transition ray tracing output image back to general layout
		vks::tools::setImageLayout(
			drawCmdBuffers[i],
			storageImage.image,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			VK_IMAGE_LAYOUT_GENERAL,
			subresourceRange);

		VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
	}
}

void VulkanRaytracer::updateUniformBuffers()
{
	uniformData.projInverse = glm::inverse(camera.matrices.perspective);
	uniformData.viewInverse = glm::inverse(camera.matrices.view);
	memcpy(ubo.mapped, &uniformData, sizeof(uniformData));
}

void VulkanRaytracer::getEnabledFeatures()
{
	// Enable features required for ray tracing using feature chaining via pNext		
	enabledBufferDeviceAddresFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
	enabledBufferDeviceAddresFeatures.bufferDeviceAddress = VK_TRUE;

	enabledRayTracingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_FEATURES_KHR;
	enabledRayTracingFeatures.rayTracing = VK_TRUE;
	enabledRayTracingFeatures.pNext = &enabledBufferDeviceAddresFeatures;

	deviceCreatepNextChain = &enabledRayTracingFeatures;
}

void VulkanRaytracer::draw()
{
	prepareFrame();
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &drawCmdBuffers[currentBuffer];
	VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
	submitFrame();
}

void VulkanRaytracer::render()
{
	if (!prepared)
		return;
	draw();
	if (camera.updated)
		updateUniformBuffers();
}

void VulkanRaytracer::framebufferResizeCallback(GLFWwindow* window, int width, int height) {
	auto app = reinterpret_cast<VulkanRaytracer*>(glfwGetWindowUserPointer(window));
	app->resizing = true;
}

void VulkanRaytracer::cursorPositionCallback(GLFWwindow* window, double x, double y)
{
	auto app = reinterpret_cast<VulkanRaytracer*>(glfwGetWindowUserPointer(window));

	int32_t dx = (int32_t)app->mousePos.x - x;
	int32_t dy = (int32_t)app->mousePos.y - y;

	if (app->mouseButtons.left) {
		app->camera.rotate(glm::vec2(-dx * app->camera.rotationSpeed, -dy * app->camera.rotationSpeed));
		app->viewUpdated = true;
	}
	app->mousePos = glm::vec2((float)x, (float)y);
}

void VulkanRaytracer::mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
	auto app = reinterpret_cast<VulkanRaytracer*>(glfwGetWindowUserPointer(window));

	if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS)
		app->mouseButtons.left = true;
	else if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE)
		app->mouseButtons.left = false;
}

void VulkanRaytracer::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	auto app = reinterpret_cast<VulkanRaytracer*>(glfwGetWindowUserPointer(window));

	if (key == GLFW_KEY_LEFT && action == GLFW_PRESS)
		app->camera.keys.left = true;
	else if (key == GLFW_KEY_LEFT && action == GLFW_RELEASE)
		app->camera.keys.left = false;
	else if (key == GLFW_KEY_RIGHT && action == GLFW_PRESS)
		app->camera.keys.right = true;
	else if (key == GLFW_KEY_RIGHT && action == GLFW_RELEASE)
		app->camera.keys.right = false;
	else if (key == GLFW_KEY_UP && action == GLFW_PRESS)
		app->camera.keys.up = true;
	else if (key == GLFW_KEY_UP && action == GLFW_RELEASE)
		app->camera.keys.up = false;
	else if (key == GLFW_KEY_DOWN && action == GLFW_PRESS)
		app->camera.keys.down = true;
	else if (key == GLFW_KEY_DOWN && action == GLFW_RELEASE)
		app->camera.keys.down = false;

	else if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		glfwSetWindowShouldClose(window, GL_TRUE);
}
