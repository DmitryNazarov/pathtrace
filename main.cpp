#include "VulkanRaytracer.h"

int main() {
	try {
		auto raytracer = std::make_unique<VulkanRaytracer>();
		raytracer->initAPIs();
		raytracer->setupWindow();
		raytracer->prepare();
		raytracer->renderLoop();
	}
	catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
