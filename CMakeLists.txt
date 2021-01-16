cmake_minimum_required(VERSION 3.5.1)

project(vulkan_pathtracer VERSION 1.0.0)

set(GLFW_DIR "glfw")
set(GLM_INCLUDE_DIR "glm")

set(SOURCE
  main.cpp
  VulkanRaytracer.cpp
  VulkanBuffer.cpp
  VulkanDebug.cpp
  VulkanDevice.cpp
  VulkanSwapChain.cpp
  VulkanTools.cpp
  Transform.cpp
  SceneLoader.cpp
)

find_package(OpenGL REQUIRED)
find_package(Vulkan REQUIRED)

add_executable(${PROJECT_NAME} ${SOURCE})

set(GLFW_BUILD_EXAMPLES OFF)
set(GLFW_BUILD_DOCS OFF)
set(GLFW_BUILD_TESTS OFF)
add_subdirectory(${GLFW_DIR})

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC ${GLFW_DIR}/include Vulkan::Vulkan ${GLM_INCLUDE_DIR})

target_link_libraries(${PROJECT_NAME} PUBLIC glfw ${OPENGL_gl_LIBRARY} ${GLFW_LIBRARIES} Vulkan::Vulkan)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)


# ------------------------- Compile Shaders -------------------------
message(STATUS "Start Shaders...")
message(STATUS "Compiling for x64 : $ENV{VULKAN_SDK}/Bin/glslangValidator.exe")
set(GLSL_VALIDATOR "$ENV{VULKAN_SDK}/Bin/glslangValidator.exe")

file(GLOB_RECURSE GLSL_SOURCE_FILES
        "shaders/*.frag"
        "shaders/*.vert"
        "shaders/*.rgen"
        "shaders/*.rmiss"
        "shaders/*.rchit"
        "shaders/*.rint"
        )

message(STATUS "Shaders compiling...")
foreach(GLSL ${GLSL_SOURCE_FILES})
    message(STATUS "${GLSL}")
    get_filename_component(FILE_NAME ${GLSL} NAME)
    set(SPIRV "${PROJECT_BINARY_DIR}/shaders/${FILE_NAME}.spv")
    add_custom_command(
            OUTPUT ${SPIRV}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/shaders/"
            COMMAND ${GLSL_VALIDATOR} --target-env vulkan1.2 -g ${GLSL} -o ${SPIRV}
            DEPENDS ${GLSL})
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
    message(STATUS "${GLSL} -> ${SPIRV}")
endforeach(GLSL)

add_custom_target(
        Shaders
        DEPENDS ${SPIRV_BINARY_FILES}
)

add_dependencies(${PROJECT_NAME} Shaders)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders/"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PROJECT_BINARY_DIR}/shaders"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders"
        )
# ------------------------- Compile Shaders -------------------------