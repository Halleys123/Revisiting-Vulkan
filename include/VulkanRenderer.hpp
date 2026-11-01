#pragma once

#include "GLFW/glfw3.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <array>

class VulkanRenderer {
private:
    GLFWwindow* window = nullptr;
    // required global properties
    uint32_t requiredVulkanVersion = VK_API_VERSION_1_3;

    std::vector<int> graphicsFamilyIndexes;
    std::vector<int> computeFamilyIndexes;
    std::vector<int> transferFamilyIndexes;
    std::vector<int> sparseFamilyIndexes;
    std::vector<int> presentFamilyIndexes;

    // Instance
    VkInstance vulkanInstance = VK_NULL_HANDLE;

    // Surface
    VkSurfaceKHR vulkanSurface;

    // Device related settings
    std::array<const char*, 1> requiredDeviceExtensions{{VK_KHR_SWAPCHAIN_EXTENSION_NAME}};

    VkPhysicalDevice vulkanPhysicalDevice = VK_NULL_HANDLE;
    VkDevice vulkanLogicalDevice = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;

    // Swapchain Setup
    VkSwapchainKHR vulkanSwapchain = VK_NULL_HANDLE;
    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    VkSurfaceFormatKHR selectedSurfaceFormat;
    VkPresentModeKHR selectedPresentMode;
    std::vector<VkImage> swapchainImages; // Linking images in swapchain we acquired during creation of swapchain to image views so we can actually use it
    std::vector<VkImageView> swapchainImageViews; // Linking images in swapchain we acquired during creation of swapchain to image views so we can actually use it

    // Render Pass
    VkRenderPass vulkanRenderPass = VK_NULL_HANDLE;
    VkPipeline vulkanGraphicsPipeline = VK_NULL_HANDLE;
public:
    VulkanRenderer(GLFWwindow* window);
    ~VulkanRenderer();
    void initialize();

private:
    // No Helper function should be called inside the
    // Cleanup functions
    void cleanup();

    // Instance related functions
    void createVulkanInstance();

    // Surface Creation
    void createSurface();

    // Setting Up logical and physical device
    // Primary Functions
    void getPhysicalDevice();
    void createLogicalDevice();

    // Helper functions
    int32_t isPhysicalDeviceSuitable(VkPhysicalDevice physicalDevice) noexcept; // This function primarly checks if the physical device supports the queue that we require in our application.

    // Swapchain Related functions
    // Helper functions
    void getSurfaceCapabilities();
    void getSurfaceFormats();
    void getPresentationModes();

    void getSwapchainImages();
    void createImageViews();

    // Actual function to create swapchain
    void createSwapchain();

    // Render Pass Implementation
    void createRenderPass();
    void createGraphicsPipeline();
};
