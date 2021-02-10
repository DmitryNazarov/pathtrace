#include "VulkanRaytracer.h"

int main(int argc, char* argv[]) {
	try {
		std::vector<std::string> args(argv, argv + argc);
		auto raytracer = std::make_unique<VulkanRaytracer>(args);
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
