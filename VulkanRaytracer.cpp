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
	vkEnumerateInstanceExtensionProperties(VK_NULL_HANDLE, &extCount, VK_NULL_HANDLE);
	if (extCount > 0)
	{
		std::vector<VkExtensionProperties> extensions(extCount);
		if (vkEnumerateInstanceExtensionProperties(VK_NULL_HANDLE, &extCount, &extensions.front()) == VK_SUCCESS)
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
	instanceCreateInfo.pApplicationInfo = &appInfo;
	if (instanceExtensions.size() > 0)
	{
		instanceCreateInfo.enabledExtensionCount = (uint32_t)instanceExtensions.size();
		instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();
	}
	if (settings.validation)
	{
		// The VK_LAYER_KHRONOS_validation contains all current validation functionality.
		const char* validationLayerName = "VK_LAYER_KHRONOS_validation";
		// Check if this layer is available at instance level
		uint32_t instanceLayerCount;
		vkEnumerateInstanceLayerProperties(&instanceLayerCount, VK_NULL_HANDLE);
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
		} else {
			std::cerr << "Validation layer VK_LAYER_KHRONOS_validation not present, validation is disabled";
		}
	}
	return vkCreateInstance(&instanceCreateInfo, VK_NULL_HANDLE, &instance);
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
	VK_CHECK_RESULT(vkCreatePipelineCache(device, &pipelineCacheCreateInfo, VK_NULL_HANDLE, &pipelineCache));
}

void VulkanRaytracer::prepare()
{
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
	rayTracingPipelineProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
	VkPhysicalDeviceProperties2 deviceProps2{};
	deviceProps2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	deviceProps2.pNext = &rayTracingPipelineProperties;
	vkGetPhysicalDeviceProperties2(physicalDevice, &deviceProps2);

	if (rayTracingPipelineProperties.maxRayRecursionDepth < scene.depth) {
		throw std::runtime_error("Device fails to support maxRecursionDepth = " + std::to_string(scene.depth));
	}

	// Query the ray tracing properties of the current implementation, we will need them later on
	accelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
	VkPhysicalDeviceFeatures2 deviceFeatures2{};
	deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	deviceFeatures2.pNext = &accelerationStructureFeatures;
	vkGetPhysicalDeviceFeatures2(physicalDevice, &deviceFeatures2);

	// Get the function pointers required for ray tracing
	vkGetBufferDeviceAddressKHR = reinterpret_cast<PFN_vkGetBufferDeviceAddressKHR>(vkGetDeviceProcAddr(device, "vkGetBufferDeviceAddressKHR"));
	vkCmdBuildAccelerationStructuresKHR = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(vkGetDeviceProcAddr(device, "vkCmdBuildAccelerationStructuresKHR"));
	vkBuildAccelerationStructuresKHR = reinterpret_cast<PFN_vkBuildAccelerationStructuresKHR>(vkGetDeviceProcAddr(device, "vkBuildAccelerationStructuresKHR"));
	vkCreateAccelerationStructureKHR = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(vkGetDeviceProcAddr(device, "vkCreateAccelerationStructureKHR"));
	vkDestroyAccelerationStructureKHR = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(vkGetDeviceProcAddr(device, "vkDestroyAccelerationStructureKHR"));
	vkGetAccelerationStructureBuildSizesKHR = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(vkGetDeviceProcAddr(device, "vkGetAccelerationStructureBuildSizesKHR"));
	vkGetAccelerationStructureDeviceAddressKHR = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(vkGetDeviceProcAddr(device, "vkGetAccelerationStructureDeviceAddressKHR"));
	vkCmdTraceRaysKHR = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(vkGetDeviceProcAddr(device, "vkCmdTraceRaysKHR"));
	vkGetRayTracingShaderGroupHandlesKHR = reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(vkGetDeviceProcAddr(device, "vkGetRayTracingShaderGroupHandlesKHR"));
	vkCreateRayTracingPipelinesKHR = reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(vkGetDeviceProcAddr(device, "vkCreateRayTracingPipelinesKHR"));

	// Create the acceleration structures used to render the ray traced scene
	scene.loadVulkanBuffersForScene(vkDebug, vulkanDevice, queue);
	createBottomLevelAccelerationStructureTriangles();
	createBottomLevelAccelerationStructureSpheres();
	createTopLevelAccelerationStructure();

	createStorageImage();
	createUniformBuffers();
	createRayTracingPipeline();
	createShaderBindingTables();
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
		updateTitle();
		frameCounter = 0;
		lastTimestamp = tEnd;
	}

	buildCommandBuffers();
}

void VulkanRaytracer::renderLoop()
{
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
#ifdef VALIDATION
	settings.validation = true;
#else
	settings.validation = false;
#endif // VALIDATION

	// Parse command line arguments
	for (size_t i = 0; i < args.size(); ++i)
	{
		if (args[i] == "-vsync")
		{
			settings.vsync = true;
		}
		else if (args[i] == "-g" || args[i] == "-gpu")
		{
			selectedDevice = std::atoi(args[i + 1].c_str());
		}
	}

	//scene.loadScene("E:\\Programming\\edx_cse168\\hw2\\data\\test_scene.test");
	//scene.loadScene("E:\\Programming\\edx_cse168\\hw2\\data\\scene1.test");
	//scene.loadScene("E:\\Programming\\edx_cse168\\hw2\\data\\scene2.test");
	//scene.loadScene("E:\\Programming\\edx_cse168\\hw2\\data\\scene3.test");
	//scene.loadScene("E:\\Programming\\edx_cse168\\hw2\\data\\scene4-ambient.test");
	//scene.loadScene("E:\\Programming\\edx_cse168\\hw2\\data\\scene4-diffuse.test");
	//scene.loadScene("E:\\Programming\\edx_cse168\\hw2\\data\\scene4-emission.test");
	//scene.loadScene("E:\\Programming\\edx_cse168\\hw2\\data\\scene4-specular.test");
	//scene.loadScene("E:\\Programming\\edx_cse168\\hw2\\data\\scene5.test");
	scene.loadScene("E:\\Programming\\edx_cse168\\hw2\\data\\scene6.test");
	//scene.loadScene("E:\\Programming\\edx_cse168\\hw2\\data\\scene7.test");

	height = scene.height;
	width = scene.width;

	camera.setPerspective(scene.fovy, (float)width / (float)height, 0.1f, 512.0f);
	camera.setLookAt(scene.eyeInit, scene.center, scene.upInit);

	// Enable instance and device extensions required to use VK_KHR_ray_tracing
	enabledInstanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
	enabledDeviceExtensions.push_back(VK_KHR_MAINTENANCE3_EXTENSION_NAME);
	enabledDeviceExtensions.push_back(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);
	enabledDeviceExtensions.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
	enabledDeviceExtensions.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
	enabledDeviceExtensions.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
	enabledDeviceExtensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
	enabledDeviceExtensions.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
	enabledDeviceExtensions.push_back(VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME);
}

VulkanRaytracer::~VulkanRaytracer()
{
	vkDestroyPipeline(device, pipeline, VK_NULL_HANDLE);
	vkDestroyPipelineLayout(device, pipelineLayout, VK_NULL_HANDLE);
	vkDestroyDescriptorSetLayout(device, descriptorSetLayout, VK_NULL_HANDLE);
	vkDestroyImageView(device, storageImage.view, VK_NULL_HANDLE);
	vkDestroyImage(device, storageImage.image, VK_NULL_HANDLE);
	vkFreeMemory(device, storageImage.memory, VK_NULL_HANDLE);
	deleteAccelerationStructure(trianglesBlas);
	deleteAccelerationStructure(spheresBlas);
	deleteAccelerationStructure(topLevelAS);

	vkDestroyBuffer(device, scene.verticesBuf.buffer, VK_NULL_HANDLE);
	vkFreeMemory(device, scene.verticesBuf.memory, VK_NULL_HANDLE);
	vkDestroyBuffer(device, scene.indicesBuf.buffer, VK_NULL_HANDLE);
	vkFreeMemory(device, scene.indicesBuf.memory, VK_NULL_HANDLE);
	vkDestroyBuffer(device, scene.spheresBuf.buffer, VK_NULL_HANDLE);
	vkFreeMemory(device, scene.spheresBuf.memory, VK_NULL_HANDLE);
	vkDestroyBuffer(device, scene.aabbsBuf.buffer, VK_NULL_HANDLE);
	vkFreeMemory(device, scene.aabbsBuf.memory, VK_NULL_HANDLE);
	vkDestroyBuffer(device, scene.pointLightsBuf.buffer, VK_NULL_HANDLE);
	vkFreeMemory(device, scene.pointLightsBuf.memory, VK_NULL_HANDLE);
	vkDestroyBuffer(device, scene.directLightsBuf.buffer, VK_NULL_HANDLE);
	vkFreeMemory(device, scene.directLightsBuf.memory, VK_NULL_HANDLE);
	vkDestroyBuffer(device, scene.triangleMaterialsBuf.buffer, VK_NULL_HANDLE);
	vkFreeMemory(device, scene.triangleMaterialsBuf.memory, VK_NULL_HANDLE);
	vkDestroyBuffer(device, scene.sphereMaterialsBuf.buffer, VK_NULL_HANDLE);
	vkFreeMemory(device, scene.sphereMaterialsBuf.memory, VK_NULL_HANDLE);
	vkDestroyBuffer(device, scene.quadLightsBuf.buffer, VK_NULL_HANDLE);
	vkFreeMemory(device, scene.quadLightsBuf.memory, VK_NULL_HANDLE);

	uboData.destroy();

	// Clean up Vulkan resources
	swapChain.cleanup();
	if (descriptorPool != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorPool(device, descriptorPool, VK_NULL_HANDLE);
	}
	destroyCommandBuffers();
	vkDestroyRenderPass(device, renderPass, nullptr);
	for (uint32_t i = 0; i < frameBuffers.size(); i++)
	{
		vkDestroyFramebuffer(device, frameBuffers[i], VK_NULL_HANDLE);
	}

	for (auto& shaderModule : shaderModules)
	{
		vkDestroyShaderModule(device, shaderModule, VK_NULL_HANDLE);
	}
	vkDestroyImageView(device, depthStencil.view, VK_NULL_HANDLE);
	vkDestroyImage(device, depthStencil.image, VK_NULL_HANDLE);
	vkFreeMemory(device, depthStencil.mem, VK_NULL_HANDLE);

	vkDestroyPipelineCache(device, pipelineCache, VK_NULL_HANDLE);

	vkDestroyCommandPool(device, cmdPool, VK_NULL_HANDLE);

	vkDestroySemaphore(device, semaphores.presentComplete, VK_NULL_HANDLE);
	vkDestroySemaphore(device, semaphores.renderComplete, VK_NULL_HANDLE);
	for (auto& fence : waitFences) {
		vkDestroyFence(device, fence, VK_NULL_HANDLE);
	}

	delete vulkanDevice;

	vkDebug.destroy(instance);

	vkDestroyInstance(instance, VK_NULL_HANDLE);

	glfwDestroyWindow(window);
	glfwTerminate();
}

bool VulkanRaytracer::initAPIs()
{
	glfwInit();

	 VkResult err = createInstance();
	if (err) {
		vks::tools::exitFatal("Could not create Vulkan instance : \n" + vks::tools::errorString(err), err);
		return false;
	}

	if (settings.validation)
	{
		vkDebug.setupInstance(instance);
	}

	// Physical device
	uint32_t gpuCount = 0;
	// Get number of available physical devices
	VK_CHECK_RESULT(vkEnumeratePhysicalDevices(instance, &gpuCount, VK_NULL_HANDLE));
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
	if (!vks::tools::getSupportedDepthFormat(physicalDevice, &depthFormat))
		throw std::runtime_error("Can't find supported depth format");

	swapChain.connect(instance, physicalDevice, device);

	// Create synchronization objects
	VkSemaphoreCreateInfo semaphoreCreateInfo = vks::initializers::semaphoreCreateInfo();
	// Create a semaphore used to synchronize image presentation
	// Ensures that the image is displayed before we start submitting new commands to the queue
	VK_CHECK_RESULT(vkCreateSemaphore(device, &semaphoreCreateInfo, VK_NULL_HANDLE, &semaphores.presentComplete));
	// Create a semaphore used to synchronize command submission
	// Ensures that the image is not presented until all commands have been submitted and executed
	VK_CHECK_RESULT(vkCreateSemaphore(device, &semaphoreCreateInfo, VK_NULL_HANDLE, &semaphores.renderComplete));

	// Set up submit info structure
	// Semaphores will stay the same during application lifetime
	// Command buffer submission info is set by each example
	submitInfo = vks::initializers::submitInfo();
	submitInfo.pWaitDstStageMask = &submitPipelineStages;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &semaphores.presentComplete;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &semaphores.renderComplete;

	if (settings.validation) {
		vkDebug.setupDevice(device);
	}

	return true;
}

void VulkanRaytracer::setupWindow()
{
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	window = glfwCreateWindow(width, height, applicationName.c_str(), nullptr, nullptr);
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
		VK_CHECK_RESULT(vkCreateFence(device, &fenceCreateInfo, VK_NULL_HANDLE, &fence));
	}
}

void VulkanRaytracer::createCommandPool()
{
	VkCommandPoolCreateInfo cmdPoolInfo = {};
	cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cmdPoolInfo.queueFamilyIndex = swapChain.queueNodeIndex;
	//cmdPoolInfo.queueFamilyIndex = vulkanDevice->queueFamilyIndices.graphics;
	cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	VK_CHECK_RESULT(vkCreateCommandPool(device, &cmdPoolInfo, VK_NULL_HANDLE, &cmdPool));
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

	VK_CHECK_RESULT(vkCreateImage(device, &imageCI, VK_NULL_HANDLE, &depthStencil.image));
	VkMemoryRequirements memReqs{};
	vkGetImageMemoryRequirements(device, depthStencil.image, &memReqs);

	VkMemoryAllocateInfo memAllloc{};
	memAllloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memAllloc.allocationSize = memReqs.size;
	memAllloc.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(device, &memAllloc, VK_NULL_HANDLE, &depthStencil.mem));
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
	VK_CHECK_RESULT(vkCreateImageView(device, &imageViewCI, VK_NULL_HANDLE, &depthStencil.view));
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
		VK_CHECK_RESULT(vkCreateFramebuffer(device, &frameBufferCreateInfo, VK_NULL_HANDLE, &frameBuffers[i]));
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
	subpassDescription.pInputAttachments = VK_NULL_HANDLE;
	subpassDescription.preserveAttachmentCount = 0;
	subpassDescription.pPreserveAttachments = VK_NULL_HANDLE;
	subpassDescription.pResolveAttachments = VK_NULL_HANDLE;

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

	VK_CHECK_RESULT(vkCreateRenderPass(device, &renderPassInfo, VK_NULL_HANDLE, &renderPass));
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
	vkDestroyImageView(device, depthStencil.view, VK_NULL_HANDLE);
	vkDestroyImage(device, depthStencil.image, VK_NULL_HANDLE);
	vkFreeMemory(device, depthStencil.mem, VK_NULL_HANDLE);
	setupDepthStencil();
	for (uint32_t i = 0; i < frameBuffers.size(); i++) {
		vkDestroyFramebuffer(device, frameBuffers[i], VK_NULL_HANDLE);
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
ScratchBuffer VulkanRaytracer::createScratchBuffer(VkDeviceSize size)
{
	ScratchBuffer scratchBuffer{};
	// Buffer and memory
	VkBufferCreateInfo bufferCreateInfo{};
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.size = size;
	bufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	VK_CHECK_RESULT(vkCreateBuffer(vulkanDevice->logicalDevice, &bufferCreateInfo, VK_NULL_HANDLE, &scratchBuffer.handle));
	VkMemoryRequirements memoryRequirements{};
	vkGetBufferMemoryRequirements(vulkanDevice->logicalDevice, scratchBuffer.handle, &memoryRequirements);
	VkMemoryAllocateFlagsInfo memoryAllocateFlagsInfo{};
	memoryAllocateFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
	memoryAllocateFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;
	VkMemoryAllocateInfo memoryAllocateInfo = {};
	memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memoryAllocateInfo.pNext = &memoryAllocateFlagsInfo;
	memoryAllocateInfo.allocationSize = memoryRequirements.size;
	memoryAllocateInfo.memoryTypeIndex = vulkanDevice->getMemoryType(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(vulkanDevice->logicalDevice, &memoryAllocateInfo, VK_NULL_HANDLE, &scratchBuffer.memory));
	VK_CHECK_RESULT(vkBindBufferMemory(vulkanDevice->logicalDevice, scratchBuffer.handle, scratchBuffer.memory, 0));
	scratchBuffer.deviceAddress = getBufferDeviceAddress(scratchBuffer.handle);
	return scratchBuffer;
}

void VulkanRaytracer::deleteScratchBuffer(ScratchBuffer& scratchBuffer)
{
	if (scratchBuffer.memory != VK_NULL_HANDLE) {
		vkFreeMemory(device, scratchBuffer.memory, VK_NULL_HANDLE);
	}
	if (scratchBuffer.handle != VK_NULL_HANDLE) {
		vkDestroyBuffer(device, scratchBuffer.handle, VK_NULL_HANDLE);
	}
}

/*
Gets the device address from a buffer that's required for some of the buffers used for ray tracing
*/
VkDeviceAddress VulkanRaytracer::getBufferDeviceAddress(VkBuffer buffer)
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
	VK_CHECK_RESULT(vkCreateImage(device, &image, VK_NULL_HANDLE, &storageImage.image));

	VkMemoryRequirements memReqs;
	vkGetImageMemoryRequirements(device, storageImage.image, &memReqs);
	VkMemoryAllocateInfo memoryAllocateInfo = vks::initializers::memoryAllocateInfo();
	memoryAllocateInfo.allocationSize = memReqs.size;
	memoryAllocateInfo.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(device, &memoryAllocateInfo, VK_NULL_HANDLE, &storageImage.memory));
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
	VK_CHECK_RESULT(vkCreateImageView(device, &colorImageView, VK_NULL_HANDLE, &storageImage.view));

	VkCommandBuffer cmdBuffer = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
	vks::tools::setImageLayout(cmdBuffer, storageImage.image,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_GENERAL,
		{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
	vulkanDevice->flushCommandBuffer(cmdBuffer, queue);
}

void VulkanRaytracer::createAccelerationStructure(AccelerationStructure& accelerationStructure, VkAccelerationStructureTypeKHR type, VkAccelerationStructureBuildSizesInfoKHR buildSizeInfo)
{
	// Buffer and memory
	VkBufferCreateInfo bufferCreateInfo{};
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.size = buildSizeInfo.accelerationStructureSize;
	bufferCreateInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	VK_CHECK_RESULT(vkCreateBuffer(vulkanDevice->logicalDevice, &bufferCreateInfo, VK_NULL_HANDLE, &accelerationStructure.buffer));
	VkMemoryRequirements memoryRequirements{};
	vkGetBufferMemoryRequirements(vulkanDevice->logicalDevice, accelerationStructure.buffer, &memoryRequirements);
	VkMemoryAllocateFlagsInfo memoryAllocateFlagsInfo{};
	memoryAllocateFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
	memoryAllocateFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;
	VkMemoryAllocateInfo memoryAllocateInfo{};
	memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memoryAllocateInfo.pNext = &memoryAllocateFlagsInfo;
	memoryAllocateInfo.allocationSize = memoryRequirements.size;
	memoryAllocateInfo.memoryTypeIndex = vulkanDevice->getMemoryType(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(vulkanDevice->logicalDevice, &memoryAllocateInfo, VK_NULL_HANDLE, &accelerationStructure.memory));
	VK_CHECK_RESULT(vkBindBufferMemory(vulkanDevice->logicalDevice, accelerationStructure.buffer, accelerationStructure.memory, 0));
	// Acceleration structure
	VkAccelerationStructureCreateInfoKHR accelerationStructureCreate_info{};
	accelerationStructureCreate_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
	accelerationStructureCreate_info.buffer = accelerationStructure.buffer;
	accelerationStructureCreate_info.size = buildSizeInfo.accelerationStructureSize;
	accelerationStructureCreate_info.type = type;
	vkCreateAccelerationStructureKHR(vulkanDevice->logicalDevice, &accelerationStructureCreate_info, VK_NULL_HANDLE, &accelerationStructure.handle);
	// AS device address
	VkAccelerationStructureDeviceAddressInfoKHR accelerationDeviceAddressInfo{};
	accelerationDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
	accelerationDeviceAddressInfo.accelerationStructure = accelerationStructure.handle;
	accelerationStructure.deviceAddress = vkGetAccelerationStructureDeviceAddressKHR(vulkanDevice->logicalDevice, &accelerationDeviceAddressInfo);
}

void VulkanRaytracer::deleteAccelerationStructure(AccelerationStructure& accelerationStructure)
{
	vkFreeMemory(device, accelerationStructure.memory, VK_NULL_HANDLE);
	vkDestroyBuffer(device, accelerationStructure.buffer, VK_NULL_HANDLE);
	vkDestroyAccelerationStructureKHR(device, accelerationStructure.handle, VK_NULL_HANDLE);
}

/*
	Create the bottom level acceleration structure contains the scene's actual geometry (vertices, triangles)
*/
void VulkanRaytracer::createBottomLevelAccelerationStructureTriangles()
{
	uint32_t numTriangles = scene.indices.size() / 3;

	VkDeviceOrHostAddressConstKHR vertexBufferDeviceAddress{};
  vertexBufferDeviceAddress.deviceAddress = getBufferDeviceAddress(scene.verticesBuf.buffer);
	VkDeviceOrHostAddressConstKHR indexBufferDeviceAddress{};
	indexBufferDeviceAddress.deviceAddress = getBufferDeviceAddress(scene.indicesBuf.buffer);

	// Build
	VkAccelerationStructureGeometryKHR accelerationStructureGeometry{};
	accelerationStructureGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	accelerationStructureGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
	accelerationStructureGeometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
	accelerationStructureGeometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
	accelerationStructureGeometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
	accelerationStructureGeometry.geometry.triangles.vertexData = vertexBufferDeviceAddress;
	accelerationStructureGeometry.geometry.triangles.maxVertex = scene.vertices.size();
	accelerationStructureGeometry.geometry.triangles.vertexStride = sizeof(Vertex);
	accelerationStructureGeometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
	accelerationStructureGeometry.geometry.triangles.indexData = indexBufferDeviceAddress;
	accelerationStructureGeometry.geometry.triangles.transformData.deviceAddress = 0;
	accelerationStructureGeometry.geometry.triangles.transformData.hostAddress = VK_NULL_HANDLE;

	// Get size info
	VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo{};
	accelerationStructureBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	accelerationStructureBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	accelerationStructureBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	accelerationStructureBuildGeometryInfo.geometryCount = 1;
	accelerationStructureBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;

	VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo{};
	accelerationStructureBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
	vkGetAccelerationStructureBuildSizesKHR(
		device,
		VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		&accelerationStructureBuildGeometryInfo,
		&numTriangles,
		&accelerationStructureBuildSizesInfo);

	createAccelerationStructure(trianglesBlas, VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR, accelerationStructureBuildSizesInfo);
	vkDebug.setBufferName(trianglesBlas.buffer, "TrianglesBLAS");

	// Create a small scratch buffer used during build of the bottom level acceleration structure
	ScratchBuffer scratchBuffer = createScratchBuffer(accelerationStructureBuildSizesInfo.buildScratchSize);

	VkAccelerationStructureBuildGeometryInfoKHR accelerationBuildGeometryInfo{};
	accelerationBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	accelerationBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	accelerationBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	accelerationBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	accelerationBuildGeometryInfo.dstAccelerationStructure = trianglesBlas.handle;
	accelerationBuildGeometryInfo.geometryCount = 1;
	accelerationBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;
	accelerationBuildGeometryInfo.scratchData.deviceAddress = scratchBuffer.deviceAddress;

	VkAccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo{};
	accelerationStructureBuildRangeInfo.primitiveCount = numTriangles;
	accelerationStructureBuildRangeInfo.primitiveOffset = 0;
	accelerationStructureBuildRangeInfo.firstVertex = 0;
	accelerationStructureBuildRangeInfo.transformOffset = 0;
	std::vector<VkAccelerationStructureBuildRangeInfoKHR*> accelerationBuildStructureRangeInfos = { &accelerationStructureBuildRangeInfo };

	if (accelerationStructureFeatures.accelerationStructureHostCommands)
	{
		// Implementation supports building acceleration structure building on host
		vkBuildAccelerationStructuresKHR(
			device,
			VK_NULL_HANDLE,
			1,
			&accelerationBuildGeometryInfo,
			accelerationBuildStructureRangeInfos.data());
	}
	else
	{
		// Acceleration structure needs to be build on the device
		VkCommandBuffer commandBuffer = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		vkCmdBuildAccelerationStructuresKHR(
			commandBuffer,
			1,
			&accelerationBuildGeometryInfo,
			accelerationBuildStructureRangeInfos.data());
		vulkanDevice->flushCommandBuffer(commandBuffer, queue);
	}

	deleteScratchBuffer(scratchBuffer);
}

void VulkanRaytracer::createBottomLevelAccelerationStructureSpheres()
{
	uint32_t numAabbs = scene.aabbs.size();
	VkDeviceOrHostAddressConstKHR aabbBufferDeviceAddress{};
	aabbBufferDeviceAddress.deviceAddress = getBufferDeviceAddress(scene.aabbsBuf.buffer);

	// Build
	VkAccelerationStructureGeometryKHR accelerationStructureGeometry{};
	accelerationStructureGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	accelerationStructureGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
	accelerationStructureGeometry.geometryType = VK_GEOMETRY_TYPE_AABBS_KHR;
	accelerationStructureGeometry.geometry.aabbs.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_AABBS_DATA_KHR;
	accelerationStructureGeometry.geometry.aabbs.data.deviceAddress = aabbBufferDeviceAddress.deviceAddress;
	accelerationStructureGeometry.geometry.aabbs.stride = sizeof(Aabb);

	// Get size info
	VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo{};
	accelerationStructureBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	accelerationStructureBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	accelerationStructureBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	accelerationStructureBuildGeometryInfo.geometryCount = 1;
	accelerationStructureBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;

	VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo{};
	accelerationStructureBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
	vkGetAccelerationStructureBuildSizesKHR(
		device,
		VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		&accelerationStructureBuildGeometryInfo,
		&numAabbs,
		&accelerationStructureBuildSizesInfo);

	createAccelerationStructure(spheresBlas, VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR, accelerationStructureBuildSizesInfo);
	vkDebug.setBufferName(spheresBlas.buffer, "SpheresBLAS");

	// Create a small scratch buffer used during build of the bottom level acceleration structure
	ScratchBuffer scratchBuffer = createScratchBuffer(accelerationStructureBuildSizesInfo.buildScratchSize);

	VkAccelerationStructureBuildGeometryInfoKHR accelerationBuildGeometryInfo{};
	accelerationBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	accelerationBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	accelerationBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	accelerationBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	accelerationBuildGeometryInfo.dstAccelerationStructure = spheresBlas.handle;
	accelerationBuildGeometryInfo.geometryCount = 1;
	accelerationBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;
	accelerationBuildGeometryInfo.scratchData.deviceAddress = scratchBuffer.deviceAddress;

	VkAccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo{};
	accelerationStructureBuildRangeInfo.primitiveCount = numAabbs;
	accelerationStructureBuildRangeInfo.primitiveOffset = 0;
	accelerationStructureBuildRangeInfo.firstVertex = 0;
	accelerationStructureBuildRangeInfo.transformOffset = 0;
	std::vector<VkAccelerationStructureBuildRangeInfoKHR*> accelerationBuildStructureRangeInfos = { &accelerationStructureBuildRangeInfo };

	if (accelerationStructureFeatures.accelerationStructureHostCommands)
	{
		// Implementation supports building acceleration structure building on host
		vkBuildAccelerationStructuresKHR(
			device,
			VK_NULL_HANDLE,
			1,
			&accelerationBuildGeometryInfo,
			accelerationBuildStructureRangeInfos.data());
	}
	else
	{
		// Acceleration structure needs to be build on the device
		VkCommandBuffer commandBuffer = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		vkCmdBuildAccelerationStructuresKHR(
			commandBuffer,
			1,
			&accelerationBuildGeometryInfo,
			accelerationBuildStructureRangeInfos.data());
		vulkanDevice->flushCommandBuffer(commandBuffer, queue);
	}

	deleteScratchBuffer(scratchBuffer);
}

/*
	The top level acceleration structure contains the scene's object instances
*/
void VulkanRaytracer::createTopLevelAccelerationStructure()
{
	VkTransformMatrixKHR transformMatrix = {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f
	};

	std::vector<VkAccelerationStructureInstanceKHR> instances;
	VkAccelerationStructureInstanceKHR instance{};
	instance.transform = transformMatrix;
	instance.instanceCustomIndex = 0;
	instance.mask = 0xFF;
	instance.instanceShaderBindingTableRecordOffset = 0;
	instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
	instance.accelerationStructureReference = trianglesBlas.deviceAddress;
	instances.push_back(instance);

	instance.instanceCustomIndex = 1;
	instance.instanceShaderBindingTableRecordOffset = 1;
	instance.accelerationStructureReference = spheresBlas.deviceAddress;
	instances.push_back(instance);

	// Buffer for instance data
	vks::Buffer instancesBuffer;
	VK_CHECK_RESULT(vulkanDevice->createBuffer(
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&instancesBuffer,
		sizeof(VkAccelerationStructureInstanceKHR) * instances.size(),
		instances.data()));

	VkDeviceOrHostAddressConstKHR instanceDataDeviceAddress{};
	instanceDataDeviceAddress.deviceAddress = getBufferDeviceAddress(instancesBuffer.buffer);

	VkAccelerationStructureGeometryKHR accelerationStructureGeometry{};
	accelerationStructureGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	accelerationStructureGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
	accelerationStructureGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
	accelerationStructureGeometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
	accelerationStructureGeometry.geometry.instances.arrayOfPointers = VK_FALSE;
	accelerationStructureGeometry.geometry.instances.data = instanceDataDeviceAddress;

	// Get size info
	VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo{};
	accelerationStructureBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	accelerationStructureBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	accelerationStructureBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	accelerationStructureBuildGeometryInfo.geometryCount = 1;
	accelerationStructureBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;

	uint32_t primitiveCount = 2;
	VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo{};
	accelerationStructureBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
	vkGetAccelerationStructureBuildSizesKHR(
		device,
		VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		&accelerationStructureBuildGeometryInfo,
		&primitiveCount,
		&accelerationStructureBuildSizesInfo);

	createAccelerationStructure(topLevelAS, VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR, accelerationStructureBuildSizesInfo);
	vkDebug.setBufferName(topLevelAS.buffer, "TLAS");

	// Create a small scratch buffer used during build of the top level acceleration structure
	ScratchBuffer scratchBuffer = createScratchBuffer(accelerationStructureBuildSizesInfo.buildScratchSize);

	VkAccelerationStructureBuildGeometryInfoKHR accelerationBuildGeometryInfo{};
	accelerationBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	accelerationBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	accelerationBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	accelerationBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	accelerationBuildGeometryInfo.dstAccelerationStructure = topLevelAS.handle;
	accelerationBuildGeometryInfo.geometryCount = 1;
	accelerationBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;
	accelerationBuildGeometryInfo.scratchData.deviceAddress = scratchBuffer.deviceAddress;

	VkAccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo{};
	accelerationStructureBuildRangeInfo.primitiveCount = primitiveCount;
	accelerationStructureBuildRangeInfo.primitiveOffset = 0;
	accelerationStructureBuildRangeInfo.firstVertex = 0;
	accelerationStructureBuildRangeInfo.transformOffset = 0;
	std::vector<VkAccelerationStructureBuildRangeInfoKHR*> accelerationBuildStructureRangeInfos = { &accelerationStructureBuildRangeInfo };

	if (accelerationStructureFeatures.accelerationStructureHostCommands)
	{
		// Implementation supports building acceleration structure building on host
		vkBuildAccelerationStructuresKHR(
			device,
			VK_NULL_HANDLE,
			1,
			&accelerationBuildGeometryInfo,
			accelerationBuildStructureRangeInfos.data());
	}
	else
	{
		// Acceleration structure needs to be build on the device
		VkCommandBuffer commandBuffer = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		vkCmdBuildAccelerationStructuresKHR(
			commandBuffer,
			1,
			&accelerationBuildGeometryInfo,
			accelerationBuildStructureRangeInfos.data());
		vulkanDevice->flushCommandBuffer(commandBuffer, queue);
	}

	deleteScratchBuffer(scratchBuffer);
	instancesBuffer.destroy();
}

template <class integral>
constexpr integral alignedSize(integral x, size_t a) noexcept
{
	return integral((x + (integral(a) - 1)) & ~integral(a - 1));
}

/*
	Create the Shader Binding Table that binds the programs and top-level acceleration structure
*/
void VulkanRaytracer::createShaderBindingTables()
{
	const uint32_t handleSize = rayTracingPipelineProperties.shaderGroupHandleSize;
	const uint32_t handleSizeAligned = alignedSize(rayTracingPipelineProperties.shaderGroupHandleSize, rayTracingPipelineProperties.shaderGroupBaseAlignment); //shaderGroupHandleAlignment????
	const uint32_t groupCount = static_cast<uint32_t>(shaderGroups.size());
	const uint32_t sbtSize = groupCount * handleSizeAligned;

	std::vector<uint8_t> shaderHandleStorage(sbtSize);
	VK_CHECK_RESULT(vkGetRayTracingShaderGroupHandlesKHR(device, pipeline, 0, groupCount, sbtSize, shaderHandleStorage.data()));

	VK_CHECK_RESULT(vulkanDevice->createBuffer(
		VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&shaderBindingTable,
		sbtSize));

	shaderBindingTable.map();
	auto* data = reinterpret_cast<uint8_t*>(shaderBindingTable.mapped);
	for (uint32_t g = 0; g < groupCount; ++g)
	{
		memcpy(data, shaderHandleStorage.data() + g * handleSize, handleSizeAligned);
		data += handleSizeAligned;
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
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 7 }
	};
	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, 1);
	VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, VK_NULL_HANDLE, &descriptorPool));

	VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);
	VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &descriptorSet));

	VkWriteDescriptorSetAccelerationStructureKHR descriptorAccelerationStructureInfo{};
	descriptorAccelerationStructureInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
	descriptorAccelerationStructureInfo.accelerationStructureCount = 1;
	descriptorAccelerationStructureInfo.pAccelerationStructures = &topLevelAS.handle;

	VkWriteDescriptorSet accelerationStructureWrite{};
	accelerationStructureWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	// The specialized acceleration structure descriptor has to be chained
	accelerationStructureWrite.pNext = &descriptorAccelerationStructureInfo;
	accelerationStructureWrite.dstSet = descriptorSet;
	accelerationStructureWrite.dstBinding = descriptorSetBindings["accelerationStructure"];
	accelerationStructureWrite.descriptorCount = 1;
	accelerationStructureWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

	VkDescriptorImageInfo storageImageDescriptor{ VK_NULL_HANDLE, storageImage.view, VK_IMAGE_LAYOUT_GENERAL };
	VkDescriptorBufferInfo vertexBufferDescriptor{ scene.verticesBuf.buffer , 0, VK_WHOLE_SIZE };
	VkDescriptorBufferInfo indexBufferDescriptor{ scene.indicesBuf.buffer , 0, VK_WHOLE_SIZE };
	VkDescriptorBufferInfo sphereBufferDescriptor{ scene.spheresBuf.buffer , 0, VK_WHOLE_SIZE };
	VkDescriptorBufferInfo pointLightsBufferDescriptor{ scene.pointLightsBuf.buffer , 0, VK_WHOLE_SIZE };
	VkDescriptorBufferInfo directLightsBufferDescriptor{ scene.directLightsBuf.buffer , 0, VK_WHOLE_SIZE };
	VkDescriptorBufferInfo triangleMaterialsBufferDescriptor{ scene.triangleMaterialsBuf.buffer , 0, VK_WHOLE_SIZE };
	VkDescriptorBufferInfo sphereMaterialsBufferDescriptor{ scene.sphereMaterialsBuf.buffer , 0, VK_WHOLE_SIZE };
	VkDescriptorBufferInfo quadLightsBufferDescriptor{ scene.quadLightsBuf.buffer , 0, VK_WHOLE_SIZE };

	std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
	accelerationStructureWrite,
		vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, descriptorSetBindings["resultImage"], &storageImageDescriptor),
		vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, descriptorSetBindings["uniformBuffer"], &uboData.descriptor),
		vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, descriptorSetBindings["vertexBuffer"], &vertexBufferDescriptor),
		vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, descriptorSetBindings["indexBuffer"], &indexBufferDescriptor)
	};

	if (!scene.spheres.empty())
	{
		writeDescriptorSets.push_back(vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			descriptorSetBindings["sphereBuffer"], &sphereBufferDescriptor));
	}
	if (!scene.pointLights.empty())
	{
		writeDescriptorSets.push_back(vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			descriptorSetBindings["pointLightsBuffer"], &pointLightsBufferDescriptor));
	}
	if (!scene.directLights.empty())
	{
		writeDescriptorSets.push_back(vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			descriptorSetBindings["directLightsBuffer"], &directLightsBufferDescriptor));
	}
	if (!scene.triangleMaterials.empty())
	{
		writeDescriptorSets.push_back(vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			descriptorSetBindings["triangleMaterialsBuffer"], &triangleMaterialsBufferDescriptor));
	}
	if (!scene.sphereMaterials.empty())
	{
		writeDescriptorSets.push_back(vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			descriptorSetBindings["sphereMaterialsBuffer"], &sphereMaterialsBufferDescriptor));
	}
	if (!scene.quadLights.empty())
	{
		writeDescriptorSets.push_back(vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			descriptorSetBindings["quadLightsBuffer"], &quadLightsBufferDescriptor));
	}

	vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, VK_NULL_HANDLE);
}

inline VkDescriptorSetLayoutBinding descriptorSetLayoutBinding(
	VkDescriptorType type,
	VkShaderStageFlags stageFlags,
	uint32_t binding,
	uint32_t descriptorCount = 1)
{
	VkDescriptorSetLayoutBinding setLayoutBinding{};
	setLayoutBinding.descriptorType = type;
	setLayoutBinding.stageFlags = stageFlags;
	setLayoutBinding.binding = binding;
	setLayoutBinding.descriptorCount = descriptorCount;
	return setLayoutBinding;
}

inline VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo(
	const std::vector<VkDescriptorSetLayoutBinding>& bindings)
{
	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
	descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptorSetLayoutCreateInfo.pBindings = bindings.data();
	descriptorSetLayoutCreateInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	return descriptorSetLayoutCreateInfo;
}

inline VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo(
	const VkDescriptorSetLayout* pSetLayouts,
	uint32_t setLayoutCount = 1)
{
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.setLayoutCount = setLayoutCount;
	pipelineLayoutCreateInfo.pSetLayouts = pSetLayouts;
	return pipelineLayoutCreateInfo;
}

/*
	Create our ray tracing pipeline
*/
void VulkanRaytracer::createRayTracingPipeline()
{
	std::vector<VkDescriptorSetLayoutBinding> bindings({
		descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, descriptorSetBindings["accelerationStructure"]),
		descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_RAYGEN_BIT_KHR, descriptorSetBindings["resultImage"]),
		descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR, descriptorSetBindings["uniformBuffer"]),
		descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, descriptorSetBindings["vertexBuffer"]),
		descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, descriptorSetBindings["indexBuffer"]),
		descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_INTERSECTION_BIT_KHR, descriptorSetBindings["sphereBuffer"]),
		descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, descriptorSetBindings["pointLightsBuffer"]),
		descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, descriptorSetBindings["directLightsBuffer"]),
		descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, descriptorSetBindings["triangleMaterialsBuffer"]),
		descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, descriptorSetBindings["sphereMaterialsBuffer"]),
		descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, descriptorSetBindings["quadLightsBuffer"])
	});

	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = descriptorSetLayoutCreateInfo(bindings);
	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCI, VK_NULL_HANDLE, &descriptorSetLayout));

	VkPipelineLayoutCreateInfo pPipelineLayoutCI = pipelineLayoutCreateInfo(&descriptorSetLayout, 1);
	VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pPipelineLayoutCI, VK_NULL_HANDLE, &pipelineLayout));

	std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

	// Ray generation group
	shaderStages.push_back(loadShader("shaders/raygen.rgen.spv", VK_SHADER_STAGE_RAYGEN_BIT_KHR));

	VkSpecializationMapEntry specializationMapEntry{};
	specializationMapEntry.constantID = 0;
	specializationMapEntry.offset = 0;
	specializationMapEntry.size = sizeof(uint32_t);
	VkSpecializationInfo specializationInfo{};
	specializationInfo.mapEntryCount = 1;
	specializationInfo.pMapEntries = &specializationMapEntry;
	specializationInfo.dataSize = sizeof(scene.depth);
	specializationInfo.pData = &scene.depth;
	shaderStages.back().pSpecializationInfo = &specializationInfo;

	VkRayTracingShaderGroupCreateInfoKHR rayGenShaderGroup{};
	rayGenShaderGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	rayGenShaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	rayGenShaderGroup.generalShader = static_cast<uint32_t>(shaderStages.size()) - 1;
	rayGenShaderGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
	rayGenShaderGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
	rayGenShaderGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
	shaderGroups.push_back(rayGenShaderGroup);

	// Miss group
	shaderStages.push_back(loadShader("shaders/miss.rmiss.spv", VK_SHADER_STAGE_MISS_BIT_KHR));
	VkRayTracingShaderGroupCreateInfoKHR missShaderGroup{};
	missShaderGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	missShaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	missShaderGroup.generalShader = static_cast<uint32_t>(shaderStages.size()) - 1;
	missShaderGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
	missShaderGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
	missShaderGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
	shaderGroups.push_back(missShaderGroup);
	// Second shader for shadows
	shaderStages.push_back(loadShader("shaders/shadow.rmiss.spv", VK_SHADER_STAGE_MISS_BIT_KHR));
	missShaderGroup.generalShader = static_cast<uint32_t>(shaderStages.size()) - 1;
	shaderGroups.push_back(missShaderGroup);

	// Closest hit group
	shaderStages.push_back(loadShader("shaders/closesthit.rchit.spv", VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR));
	VkRayTracingShaderGroupCreateInfoKHR closestHitShaderGroup{};
	closestHitShaderGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	closestHitShaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
	closestHitShaderGroup.generalShader = VK_SHADER_UNUSED_KHR;
	closestHitShaderGroup.closestHitShader = static_cast<uint32_t>(shaderStages.size()) - 1;
	closestHitShaderGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
	closestHitShaderGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
	shaderGroups.push_back(closestHitShaderGroup);

	// Intersection hit group (Closest hit + Intersection(procedural))
	shaderStages.push_back(loadShader("shaders/closesthit_spheres.rchit.spv", VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR));
	VkRayTracingShaderGroupCreateInfoKHR intersecShaderGroup{};
	intersecShaderGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	intersecShaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR;
	intersecShaderGroup.generalShader = VK_SHADER_UNUSED_KHR;
	intersecShaderGroup.closestHitShader = static_cast<uint32_t>(shaderStages.size()) - 1;
	intersecShaderGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
	intersecShaderGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
	shaderStages.push_back(loadShader("shaders/spheres.rint.spv", VK_SHADER_STAGE_INTERSECTION_BIT_KHR));
	intersecShaderGroup.intersectionShader = static_cast<uint32_t>(shaderStages.size()) - 1;
	shaderGroups.push_back(intersecShaderGroup);

	VkRayTracingPipelineCreateInfoKHR rayTracingPipelineCI{};
	rayTracingPipelineCI.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
	rayTracingPipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
	rayTracingPipelineCI.pStages = shaderStages.data();
	rayTracingPipelineCI.groupCount = static_cast<uint32_t>(shaderGroups.size());
	rayTracingPipelineCI.pGroups = shaderGroups.data();
	rayTracingPipelineCI.maxPipelineRayRecursionDepth = scene.depth;
	rayTracingPipelineCI.layout = pipelineLayout;
	VK_CHECK_RESULT(vkCreateRayTracingPipelinesKHR(device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &rayTracingPipelineCI, VK_NULL_HANDLE, &pipeline));
}

/*
	Create the uniform buffer used to pass data to the ray tracing ray generation shader
*/
void VulkanRaytracer::createUniformBuffers()
{
	VK_CHECK_RESULT(vulkanDevice->createBuffer(
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&uboData,
		sizeof(uniformData),
		&uniformData));
	VK_CHECK_RESULT(uboData.map());

	updateUniformBuffers();
}

/*
	If the window has been resized, we need to recreate the storage image and it's descriptor
*/
void VulkanRaytracer::handleResize()
{
	// Delete allocated resources
	vkDestroyImageView(device, storageImage.view, VK_NULL_HANDLE);
	vkDestroyImage(device, storageImage.image, VK_NULL_HANDLE);
	vkFreeMemory(device, storageImage.memory, VK_NULL_HANDLE);
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

		// Dispatch the ray tracing commands
		vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);
		vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelineLayout, 0, 1, &descriptorSet, 0, 0);

		const uint32_t handleSizeAligned = alignedSize(rayTracingPipelineProperties.shaderGroupHandleSize, rayTracingPipelineProperties.shaderGroupBaseAlignment);
		VkDeviceAddress sbtAddress = getBufferDeviceAddress(shaderBindingTable.buffer);

		VkStridedDeviceAddressRegionKHR raygenStride{};
		raygenStride.deviceAddress = sbtAddress + 0 * handleSizeAligned;
		raygenStride.stride = handleSizeAligned;
		raygenStride.size = 1 * handleSizeAligned;

		VkStridedDeviceAddressRegionKHR missStride{};
		missStride.deviceAddress = sbtAddress + 1 * handleSizeAligned;
		missStride.stride = handleSizeAligned;
		missStride.size = 2 * handleSizeAligned;

		VkStridedDeviceAddressRegionKHR hitStride{};
		hitStride.deviceAddress = sbtAddress + 3 * handleSizeAligned;
		hitStride.stride = handleSizeAligned;
		hitStride.size = 1 * handleSizeAligned;

		VkStridedDeviceAddressRegionKHR emptySbtEntry = {};

		vkCmdTraceRaysKHR(
			drawCmdBuffers[i],
			&raygenStride,
			&missStride,
			&hitStride,
			&emptySbtEntry,
			width,
			height,
			1);

		// Copy ray tracing output to swap chain image

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
	uniformData.pointLightsNum = scene.pointLights.size();
	uniformData.directLightsNum = scene.directLights.size();
	uniformData.quadLightsNum = scene.quadLights.size();
	memcpy(uboData.mapped, &uniformData, sizeof(uniformData));
}

void VulkanRaytracer::getEnabledFeatures()
{
	// Enable features required for ray tracing using feature chaining via pNext
	enabledBufferDeviceAddresFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
	enabledBufferDeviceAddresFeatures.bufferDeviceAddress = VK_TRUE;

	enabledRayTracingPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
	enabledRayTracingPipelineFeatures.rayTracingPipeline = VK_TRUE;
	enabledRayTracingPipelineFeatures.pNext = &enabledBufferDeviceAddresFeatures;

	enabledAccelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
	enabledAccelerationStructureFeatures.accelerationStructure = VK_TRUE;
	enabledAccelerationStructureFeatures.pNext = &enabledRayTracingPipelineFeatures;

	deviceCreatepNextChain = &enabledAccelerationStructureFeatures;
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

void VulkanRaytracer::updateTitle()
{
	std::stringstream ss;
	ss << applicationName << " " << (1000.0f / lastFPS) << " ms/frame" << " (" << lastFPS << " fps)";
	glfwSetWindowTitle(window, ss.str().c_str());
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
		app->camera.rotate(glm::vec2(dx * app->camera.rotationSpeed, dy * app->camera.rotationSpeed));
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
