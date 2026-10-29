#include <stdio.h>
#include "GLFW/glfw3.h"

#include "Logger.hpp"
#include "VulkanRenderer.hpp"

int main() {
    auto& log = Logger::get();
    // log.setTimeMode(TimeMode::WallClock);  // [14:03:22.123]

    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE); // making window non resizable because that would require swapchain recreation

    GLFWwindow* myWindow = glfwCreateWindow(1280, 800, "Vulkan Revisiting", nullptr, nullptr);

    VulkanRenderer render(myWindow);
    render.initialize();

    while(!glfwWindowShouldClose(myWindow)) {
        glfwPollEvents();
    }

    glfwTerminate();
    printf("Working\n");
    return 0;
}
