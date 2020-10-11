#include "vulkanexamplebase.h"


std::vector<const char*> VulkanRaytracer::args;

VkResult VulkanRaytracer::createInstance(bool enableValidation)
{
	this->settings.validation = enableValidation;

	// Validation can also be forced via a define
#if defined(_VALIDATION)
	this->settings.validation = true;
#endif

	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = name.c_str();
	appInfo.pEngineName = name.c_str();
	appInfo.apiVersion = apiVersion;

	std::vector<const char*> instanceExtensions;
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
	//std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
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

std::string VulkanRaytracer::getWindowTitle()
{
	std::string device(deviceProperties.deviceName);
	std::string windowTitle;
	windowTitle = title + " - " + device;
	if (!settings.overlay) {
		windowTitle += " - " + std::to_string(frameCounter) + " fps";
	}
	return windowTitle;
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

std::string VulkanRaytracer::getShadersPath() const
{
	return getAssetPath() + "shaders/" + shaderDir + "/";
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
		viewChanged();
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
	// TODO: Cap UI overlay update rates
	updateOverlay();
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

void VulkanRaytracer::updateOverlay()
{
	if (!settings.overlay)
		return;

	buildCommandBuffers();
}

void VulkanRaytracer::drawUI(const VkCommandBuffer commandBuffer)
{
	//if (settings.overlay) {
	//	const VkViewport viewport = vks::initializers::viewport((float)width, (float)height, 0.0f, 1.0f);
	//	const VkRect2D scissor = vks::initializers::rect2D(width, height, 0, 0);
	//	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
	//	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

	//	UIOverlay.draw(commandBuffer);
	//}
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

VulkanRaytracer::VulkanRaytracer(bool enableValidation)
{
	// Check for a valid asset path
	//struct stat info;
	//if (stat(getAssetPath().c_str(), &info) != 0)
	//{
	//	std::string msg = "Could not locate asset path in \"" + getAssetPath() + "\" !";
	//	MessageBox(NULL, msg.c_str(), "Fatal error", MB_OK | MB_ICONERROR);
	//	exit(-1);
	//}

	settings.validation = enableValidation;

	char* numConvPtr;

	settings.validation = true; //for tests
	// Parse command line arguments
	for (size_t i = 0; i < args.size(); i++)
	{
		if (args[i] == std::string("-validation")) {
			settings.validation = true;
		}
		if (args[i] == std::string("-vsync")) {
			settings.vsync = true;
		}
		if ((args[i] == std::string("-f")) || (args[i] == std::string("--fullscreen"))) {
			settings.fullscreen = true;
		}
		if ((args[i] == std::string("-w")) || (args[i] == std::string("-width"))) {
			uint32_t w = strtol(args[i + 1], &numConvPtr, 10);
			if (numConvPtr != args[i + 1]) { width = w; };
		}
		if ((args[i] == std::string("-h")) || (args[i] == std::string("-height"))) {
			uint32_t h = strtol(args[i + 1], &numConvPtr, 10);
			if (numConvPtr != args[i + 1]) { height = h; };
		}
		// Select between glsl and hlsl shaders
		if ((args[i] == std::string("-s")) || (args[i] == std::string("--shaders"))) {
			std::string type;
			if (args.size() > i + 1) {
				type = args[i + 1];
			}
			if (type == "glsl" || type == "hlsl") {
				shaderDir = type;
			} else {
				std::cerr << args[i] << " must be one of 'glsl' or 'hlsl'" << std::endl;
			}
		}
		// Benchmark
		if ((args[i] == std::string("-b")) || (args[i] == std::string("--benchmark"))) {
			benchmark.active = true;
			vks::tools::errorModeSilent = true;
		}
		// Warmup time (in seconds)
		if ((args[i] == std::string("-bw")) || (args[i] == std::string("--benchwarmup"))) {
			if (args.size() > i + 1) {
				uint32_t num = strtol(args[i + 1], &numConvPtr, 10);
				if (numConvPtr != args[i + 1]) {
					benchmark.warmup = num;
				} else {
					std::cerr << "Warmup time for benchmark mode must be specified as a number!" << "\n";
				}
			}
		}
		// Benchmark runtime (in seconds)
		if ((args[i] == std::string("-br")) || (args[i] == std::string("--benchruntime"))) {
			if (args.size() > i + 1) {
				uint32_t num = strtol(args[i + 1], &numConvPtr, 10);
				if (numConvPtr != args[i + 1]) {
					benchmark.duration = num;
				}
				else {
					std::cerr << "Benchmark run duration must be specified as a number!" << "\n";
				}
			}
		}
		// Bench result save filename (overrides default)
		if ((args[i] == std::string("-bf")) || (args[i] == std::string("--benchfilename"))) {
			if (args.size() > i + 1) {
				if (args[i + 1][0] == '-') {
					std::cerr << "Filename for benchmark results must not start with a hyphen!" << "\n";
				} else {
					benchmark.filename = args[i + 1];
				}
			}
		}
		// Output frame times to benchmark result file
		if ((args[i] == std::string("-bt")) || (args[i] == std::string("--benchframetimes"))) {
			benchmark.outputFrameTimes = true;
		}
	}

	// Enable console if validation is active
	// Debug message callback will output to it
	if (this->settings.validation)
	{
		//setupConsole("Vulkan validation output");
	}
	//setupDPIAwareness();
}

VulkanRaytracer::~VulkanRaytracer()
{
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

	//if (settings.overlay) {
	//	UIOverlay.freeResources();
	//}

	delete vulkanDevice;

	if (settings.validation)
	{
		vks::debug::freeDebugCallback(instance);
	}

	vkDestroyInstance(instance, nullptr);

	glfwDestroyWindow(window);
	glfwTerminate();
}

bool VulkanRaytracer::initVulkan()
{
	VkResult err;

	// Vulkan instance
	err = createInstance(settings.validation);
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

	// GPU selection

	// Select physical device to be used for the Vulkan example
	// Defaults to the first device unless specified by command line
	uint32_t selectedDevice = 0;

#if !defined(VK_USE_PLATFORM_ANDROID_KHR)
	// GPU selection via command line argument
	for (size_t i = 0; i < args.size(); i++)
	{
		// Select GPU
		if ((args[i] == std::string("-g")) || (args[i] == std::string("-gpu")))
		{
			char* endptr;
			uint32_t index = strtol(args[i + 1], &endptr, 10);
			if (endptr != args[i + 1])
			{
				if (index > gpuCount - 1)
				{
					std::cerr << "Selected device index " << index << " is out of range, reverting to device 0 (use -listgpus to show available Vulkan devices)" << "\n";
				}
				else
				{
					std::cout << "Selected Vulkan device " << index << "\n";
					selectedDevice = index;
				}
			};
			break;
		}
		// List available GPUs
		if (args[i] == std::string("-listgpus"))
		{
			uint32_t gpuCount = 0;
			VK_CHECK_RESULT(vkEnumeratePhysicalDevices(instance, &gpuCount, nullptr));
			if (gpuCount == 0)
			{
				std::cerr << "No Vulkan devices found!" << "\n";
			}
			else
			{
				// Enumerate devices
				std::cout << "Available Vulkan devices" << "\n";
				std::vector<VkPhysicalDevice> devices(gpuCount);
				VK_CHECK_RESULT(vkEnumeratePhysicalDevices(instance, &gpuCount, devices.data()));
				for (uint32_t j = 0; j < gpuCount; j++) {
					VkPhysicalDeviceProperties deviceProperties;
					vkGetPhysicalDeviceProperties(devices[j], &deviceProperties);
					std::cout << "Device [" << j << "] : " << deviceProperties.deviceName << std::endl;
					std::cout << " Type: " << vks::tools::physicalDeviceTypeString(deviceProperties.deviceType) << "\n";
					std::cout << " API: " << (deviceProperties.apiVersion >> 22) << "." << ((deviceProperties.apiVersion >> 12) & 0x3ff) << "." << (deviceProperties.apiVersion & 0xfff) << "\n";
				}
			}
		}
	}
#endif

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

// Win32 : Sets up a console window and redirects standard output to it
void VulkanRaytracer::setupConsole(std::string title)
{
	AllocConsole();
	AttachConsole(GetCurrentProcessId());
	FILE *stream;
	freopen_s(&stream, "CONOUT$", "w+", stdout);
	freopen_s(&stream, "CONOUT$", "w+", stderr);
	SetConsoleTitle(TEXT(title.c_str()));
}

void VulkanRaytracer::setupWindow()
{
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	window = glfwCreateWindow(width, height, "Vulkan", nullptr, nullptr);
	glfwSetWindowUserPointer(window, this);
	glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
	glfwSetCursorPosCallback(window, cursorPositionCallback);
	glfwSetMouseButtonCallback(window, mouseButtonCallback);
}

//void VulkanExampleBase::handleMessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
//{
//	switch (uMsg)
//	{
//	case WM_CLOSE:
//		prepared = false;
//		DestroyWindow(hWnd);
//		PostQuitMessage(0);
//		break;
//	case WM_PAINT:
//		ValidateRect(window, NULL);
//		break;
//	case WM_KEYDOWN:
//		switch (wParam)
//		{
//		case KEY_P:
//			paused = !paused;
//			break;
//		case KEY_F1:
//			if (settings.overlay) {
//				UIOverlay.visible = !UIOverlay.visible;
//			}
//			break;
//		case KEY_ESCAPE:
//			PostQuitMessage(0);
//			break;
//		}
//
//		if (camera.type == Camera::firstperson)
//		{
//			switch (wParam)
//			{
//			case KEY_W:
//				camera.keys.up = true;
//				break;
//			case KEY_S:
//				camera.keys.down = true;
//				break;
//			case KEY_A:
//				camera.keys.left = true;
//				break;
//			case KEY_D:
//				camera.keys.right = true;
//				break;
//			}
//		}
//
//		keyPressed((uint32_t)wParam);
//		break;
//	case WM_KEYUP:
//		if (camera.type == Camera::firstperson)
//		{
//			switch (wParam)
//			{
//			case KEY_W:
//				camera.keys.up = false;
//				break;
//			case KEY_S:
//				camera.keys.down = false;
//				break;
//			case KEY_A:
//				camera.keys.left = false;
//				break;
//			case KEY_D:
//				camera.keys.right = false;
//				break;
//			}
//		}
//		break;
//	case WM_LBUTTONDOWN:
//		mousePos = glm::vec2((float)LOWORD(lParam), (float)HIWORD(lParam));
//		mouseButtons.left = true;
//		break;
//	case WM_RBUTTONDOWN:
//		mousePos = glm::vec2((float)LOWORD(lParam), (float)HIWORD(lParam));
//		mouseButtons.right = true;
//		break;
//	case WM_MBUTTONDOWN:
//		mousePos = glm::vec2((float)LOWORD(lParam), (float)HIWORD(lParam));
//		mouseButtons.middle = true;
//		break;
//	case WM_LBUTTONUP:
//		mouseButtons.left = false;
//		break;
//	case WM_RBUTTONUP:
//		mouseButtons.right = false;
//		break;
//	case WM_MBUTTONUP:
//		mouseButtons.middle = false;
//		break;
//	case WM_MOUSEWHEEL:
//	{
//		short wheelDelta = GET_WHEEL_DELTA_WPARAM(wParam);
//		camera.translate(glm::vec3(0.0f, 0.0f, (float)wheelDelta * 0.005f));
//		viewUpdated = true;
//		break;
//	}
//	case WM_MOUSEMOVE:
//	{
//		handleMouseMove(LOWORD(lParam), HIWORD(lParam));
//		break;
//	}
//	case WM_SIZE:
//		if ((prepared) && (wParam != SIZE_MINIMIZED))
//		{
//			if ((resizing) || ((wParam == SIZE_MAXIMIZED) || (wParam == SIZE_RESTORED)))
//			{
//				destWidth = LOWORD(lParam);
//				destHeight = HIWORD(lParam);
//				windowResize();
//			}
//		}
//		break;
//	case WM_GETMINMAXINFO:
//	{
//		LPMINMAXINFO minMaxInfo = (LPMINMAXINFO)lParam;
//		minMaxInfo->ptMinTrackSize.x = 64;
//		minMaxInfo->ptMinTrackSize.y = 64;
//		break;
//	}
//	case WM_ENTERSIZEMOVE:
//		resizing = true;
//		break;
//	case WM_EXITSIZEMOVE:
//		resizing = false;
//		break;
//	}
//}

void VulkanRaytracer::viewChanged() {}

void VulkanRaytracer::keyPressed(uint32_t) {}

void VulkanRaytracer::mouseMoved(double x, double y, bool & handled) {}

void VulkanRaytracer::buildCommandBuffers() {}

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

void VulkanRaytracer::getEnabledFeatures() {}

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

	//if ((width > 0.0f) && (height > 0.0f)) {
	//	if (settings.overlay) {
	//		UIOverlay.resize(width, height);
	//	}
	//}

	// Command buffers need to be recreated as they may store
	// references to the recreated frame buffer
	destroyCommandBuffers();
	createCommandBuffers();
	buildCommandBuffers();

	vkDeviceWaitIdle(device);

	if ((width > 0.0f) && (height > 0.0f)) {
		camera.updateAspectRatio((float)width / (float)height);
	}

	// Notify derived class
	windowResized();
	viewChanged();

	prepared = true;
}

//void VulkanExampleBase::handleMouseMove(int32_t x, int32_t y)
//{
//	int32_t dx = (int32_t)mousePos.x - x;
//	int32_t dy = (int32_t)mousePos.y - y;
//
//	bool handled = false;
//
//	if (settings.overlay) {
//		ImGuiIO& io = ImGui::GetIO();
//		handled = io.WantCaptureMouse;
//	}
//	mouseMoved((float)x, (float)y, handled);
//
//	if (handled) {
//		mousePos = glm::vec2((float)x, (float)y);
//		return;
//	}
//
//	if (mouseButtons.left) {
//		camera.rotate(glm::vec3(dy * camera.rotationSpeed, -dx * camera.rotationSpeed, 0.0f));
//		viewUpdated = true;
//	}
//	if (mouseButtons.right) {
//		camera.translate(glm::vec3(-0.0f, 0.0f, dy * .005f));
//		viewUpdated = true;
//	}
//	if (mouseButtons.middle) {
//		camera.translate(glm::vec3(-dx * 0.01f, -dy * 0.01f, 0.0f));
//		viewUpdated = true;
//	}
//	mousePos = glm::vec2((float)x, (float)y);
//}

void VulkanRaytracer::windowResized() {}

void VulkanRaytracer::initSwapchain()
{
	swapChain.initSurface(window);
}

void VulkanRaytracer::setupSwapChain()
{
	swapChain.create(&width, &height, settings.vsync);
}
