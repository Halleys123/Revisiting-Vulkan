#pragma once

#include <vulkan/vulkan.h>
#include <array>

class VulkanRenderer {
private:
    // required global properties
    uint32_t requiredVulkanVersion = VK_API_VERSION_1_3;
    int graphicsFamilyIndex = -1;

    // Instance
    VkInstance vulkanInstance = VK_NULL_HANDLE;

    // Device related settings
    std::array<const char*, 1> requiredDeviceExtensions{{VK_KHR_SWAPCHAIN_EXTENSION_NAME}};

    VkPhysicalDevice vulkanPhysicalDevice = VK_NULL_HANDLE;
    VkDevice vulkanLogicalDevice = VK_NULL_HANDLE;

public:
    ~VulkanRenderer();
    void initialize();

private:
    // Cleanup functions
    void cleanup();

    // Instance related functions
    void createVulkanInstance();

    // Setting Up logical and physical device
    // Primary Functions
    void getPhysicalDevice();
    void createLogicalDevice();

    // Helper functions
    int32_t isPhysicalDeviceSuitable(VkPhysicalDevice physicalDevice) noexcept; // This function primarly checks if the physical device supports the queue that we require in our application.
};
