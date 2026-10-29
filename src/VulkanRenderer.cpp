#include "VulkanRenderer.hpp"
#include "GLFW/glfw3.h"
#include "Logger.hpp"

#include <algorithm>
#include <cstring>
#include <propidl.h>
#include <vulkan/vk_enum_string_helper.h>

#include <stdexcept>
#include <vector>

VulkanRenderer::VulkanRenderer(GLFWwindow *window) {
    this->window = window;
}


VulkanRenderer::~VulkanRenderer() {
    this->cleanup();
}

void VulkanRenderer::initialize() {
    createVulkanInstance();
    createSurface();
    getPhysicalDevice();
    createLogicalDevice();
}

void VulkanRenderer::cleanup() {
    if(vulkanLogicalDevice != VK_NULL_HANDLE) vkDestroyDevice(vulkanLogicalDevice, nullptr);
    if(vulkanSurface != VK_NULL_HANDLE) vkDestroySurfaceKHR(vulkanInstance, vulkanSurface, nullptr);
    if(vulkanInstance != VK_NULL_HANDLE) vkDestroyInstance(vulkanInstance, nullptr);
}

void VulkanRenderer::createVulkanInstance() {
  VkApplicationInfo applicationInfo = {};

  applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  applicationInfo.applicationVersion = 1;
  applicationInfo.apiVersion = requiredVulkanVersion;
  applicationInfo.pApplicationName = "Learning Vulkan";

  uint32_t glfwExtensionCount = 0;
  const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

  VkInstanceCreateInfo VulkanInstanceCreateInfoStruct = {};

  VulkanInstanceCreateInfoStruct.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  // TODO: Need to change this based on future information I gain about the
  // program This contains extensions like swapchain and surface I don't
  // remember properly, but Vulkan API needs to know what are the extensions
  // that we are going to use
  VulkanInstanceCreateInfoStruct.enabledExtensionCount = glfwExtensionCount;
  VulkanInstanceCreateInfoStruct.enabledLayerCount = 0;
  // This is the layers like validation layer and other layers that we
  // might need in the application
  VulkanInstanceCreateInfoStruct.ppEnabledExtensionNames = glfwExtensions;
  VulkanInstanceCreateInfoStruct.ppEnabledLayerNames = nullptr;
  VulkanInstanceCreateInfoStruct.pApplicationInfo = &applicationInfo;

  uint32_t propertyCount;
  vkEnumerateInstanceExtensionProperties(nullptr, &propertyCount, nullptr);

  std::vector<VkExtensionProperties> availableInstanceProperties(propertyCount);
  vkEnumerateInstanceExtensionProperties(nullptr, &propertyCount, availableInstanceProperties.data());

  // Just printing the supported extensions
  // TODO Remove this part later on
  for(int i = 0;i < propertyCount;i += 1) {
      LOG_INFO("%s %d", availableInstanceProperties[i].extensionName, availableInstanceProperties[i].specVersion);
  }

  bool allExtensionsSupported = true;
  for(int i = 0;i < glfwExtensionCount;i += 1) {
      bool isSupported = false;
      for(int j = 0;j < availableInstanceProperties.size();j += 1) {
          if(strcmp(availableInstanceProperties[j].extensionName, glfwExtensions[i]) == 0) {
              LOG_SUCCESS("%s required instance extension is supported", glfwExtensions[i]);
              isSupported = true;
          }
      }

      if(isSupported == false) {
          allExtensionsSupported = false;
          LOG_ERROR("%s required instance extension is not supported", glfwExtensions[i]);
      }
  }

  if(allExtensionsSupported == false) {
      LOG_CRITICAL("Required Instance level extension not supported");
      throw std::runtime_error("The requried extensions to create the Vulkan Instance are not supported");
  }

  VkResult result = vkCreateInstance(&VulkanInstanceCreateInfoStruct, nullptr, &vulkanInstance);
  if (result != VK_SUCCESS) {
    throw std::runtime_error("Unable to create Vulkan Instance");
  }
}
void VulkanRenderer::createSurface() {
    glfwCreateWindowSurface(vulkanInstance, this->window, nullptr, &vulkanSurface);
}

void VulkanRenderer::getPhysicalDevice() {
    uint32_t physicalDeviceCount;
    vkEnumeratePhysicalDevices(vulkanInstance, &physicalDeviceCount, nullptr);

    // If not physical device is found that is an error
    if(physicalDeviceCount == 0) {
        LOG_ERROR("%s", "Unable to detect any Physical Device");
        throw std::runtime_error("Unable to detect any physical Device, try restarting the application");
    }

    std::vector<VkPhysicalDevice> availablePhysicalDevices(physicalDeviceCount);
    vkEnumeratePhysicalDevices(vulkanInstance, &physicalDeviceCount, availablePhysicalDevices.data());

    LOG_INFO("Found %d Physical Devices", physicalDeviceCount);

    // Get properties of each physical device check which of the device support what we need.
    LOG_TRACE("NOW PRINTING DEVICE PROPERTIES");
    for(auto& curPhysicalDevice : availablePhysicalDevices) {
        isPhysicalDeviceSuitable(curPhysicalDevice);
    }

    if(vulkanPhysicalDevice == VK_NULL_HANDLE) {
        LOG_CRITICAL("Unable to get a GPU with required features");
        throw std::runtime_error("Unable to obtain a GPU with required features.");
    }
}

void VulkanRenderer::createLogicalDevice() {
    // Default: first graphics family and first present family
    auto graphicsQueueFamily = static_cast<uint32_t>(graphicsFamilyIndexes[0]);
    auto presentQueueFamily  = static_cast<uint32_t>(presentFamilyIndexes[0]);

    // Two-pointer search for a family that appears in both sorted lists
    size_t i = 0, j = 0;
    while (i < graphicsFamilyIndexes.size() && j < presentFamilyIndexes.size()) {
        if (graphicsFamilyIndexes[i] == presentFamilyIndexes[j]) {
            graphicsQueueFamily = static_cast<uint32_t>(graphicsFamilyIndexes[i]);
            presentQueueFamily  = graphicsQueueFamily;
            break;
        }
        else if (graphicsFamilyIndexes[i] < presentFamilyIndexes[j]) {
            i += 1;
        }
        else {
            j += 1;
        }
    }

    float priority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

    VkDeviceQueueCreateInfo graphicsInfo = {};
    graphicsInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    graphicsInfo.queueFamilyIndex = graphicsQueueFamily;
    graphicsInfo.queueCount = 1;
    graphicsInfo.pQueuePriorities = &priority;
    queueCreateInfos.push_back(graphicsInfo);

    if (presentQueueFamily != graphicsQueueFamily) {
        LOG_NOTICE("Presentation Family queue and Graphics family queue did not match, now creating separate queue for presentation");
        VkDeviceQueueCreateInfo presentInfo = {};
        presentInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        presentInfo.queueFamilyIndex = presentQueueFamily;
        presentInfo.queueCount = 1;
        presentInfo.pQueuePriorities = &priority;
        queueCreateInfos.push_back(presentInfo);
    }

    VkDeviceCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    info.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    info.pQueueCreateInfos = queueCreateInfos.data();
    info.enabledExtensionCount = static_cast<uint32_t>(requiredDeviceExtensions.size());
    info.ppEnabledExtensionNames = requiredDeviceExtensions.data();

    VkResult result = vkCreateDevice(vulkanPhysicalDevice, &info, nullptr, &vulkanLogicalDevice);
    if (result != VK_SUCCESS) {
        LOG_CRITICAL("Unable to create Vulkan Logical Device");
        throw std::runtime_error("Unable to create Vulkan Logical Device");
    }

    vkGetDeviceQueue(vulkanLogicalDevice, graphicsQueueFamily, 0, &graphicsQueue);
    vkGetDeviceQueue(vulkanLogicalDevice, presentQueueFamily,  0, &presentQueue);

    LOG_SUCCESS("Successfully created the logical device");
}

int32_t VulkanRenderer::isPhysicalDeviceSuitable(VkPhysicalDevice physicalDevice) noexcept {
    // Checking in Chain flow for now or maybe we can divide the features that we want into optional and mandatory features
    // All mandatory features should be available in hardware
    // More option features the better, we will rather return an unsigned integer of count of optional feature in return
    // if all mandatory feature are not passed then we will just return -1 that means not passed.
    //
    // Now listing out the mandatory features as well as Optional features that I might want

    // Mandatory features include:
    // Graphics Queue Support
    // Extension Support
    // Presentation Support
    // Vulkan support of required version
    // For now there are no optional features that we need
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceFeatures features;

    vkGetPhysicalDeviceProperties(physicalDevice, &properties);
    vkGetPhysicalDeviceFeatures(physicalDevice, &features);


    LOG_NOTICE("Checking if %s is suitable for use", properties.deviceName);

    // ---------------------------------------------------------------
    // Starting with checking if GPU supports required vulkan version
    if(properties.apiVersion < this->requiredVulkanVersion) {
        LOG_ERROR("%s: Vulkan %u.%u.%u supported, need %u.%u.%u", properties.deviceName,
                  VK_API_VERSION_MAJOR(properties.apiVersion), VK_API_VERSION_MINOR(properties.apiVersion), VK_API_VERSION_PATCH(properties.apiVersion),
                  VK_API_VERSION_MAJOR(this->requiredVulkanVersion), VK_API_VERSION_MINOR(this->requiredVulkanVersion), VK_API_VERSION_PATCH(this->requiredVulkanVersion));
        return -1;
    }
    // ----------------------------------------------------------------
    // Starting with Checking supported extension
    // First getting supported extensions.
    uint32_t deviceSupportedPropertiesCount;
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &deviceSupportedPropertiesCount, nullptr);

    std::vector<VkExtensionProperties> logicalDeviceSupportedProperties(deviceSupportedPropertiesCount);
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &deviceSupportedPropertiesCount, logicalDeviceSupportedProperties.data());

    // Check if required extensions are supported or not.
    bool allExtensionsSupported = true;
    for(int i = 0;i < this->requiredDeviceExtensions.size();i += 1) {
        bool isSupported = false;
        for(int j = 0;j < logicalDeviceSupportedProperties.size();j += 1) {
            if(strcmp(logicalDeviceSupportedProperties[j].extensionName, this->requiredDeviceExtensions[i]) == 0) {
                LOG_SUCCESS("%s required device extension is supported by %s", this->requiredDeviceExtensions[i], properties.deviceName);
                isSupported = true;
            }
        }

        if(isSupported == false) {
            allExtensionsSupported = false;
            LOG_ERROR("%s required device extension is not supported", this->requiredDeviceExtensions[i]);
        }
    }

    if(allExtensionsSupported == false) {
        LOG_ERROR("Required Device level extensions are not supported by device %s", properties.deviceName);
        return -1;
    }
    // ----------------------------------------------------------------
    // Now checking queue family support for graphics queue
    uint32_t totalQueueFamiliesSupported;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &totalQueueFamiliesSupported, nullptr);

    if(totalQueueFamiliesSupported == 0) {
        LOG_ERROR("0 Queue families supported, cannot continue execution");
        return -1;
    }

    std::vector<VkQueueFamilyProperties> queueFamilyList(totalQueueFamiliesSupported);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &totalQueueFamiliesSupported, queueFamilyList.data());

    LOG_INFO("Queue Family details for %s", properties.deviceName);
    std::string table = "Queue families:\n";

    std::vector<int> tempGraphicsFamilyIndexes;
    std::vector<int> tempComputeFamilyIndexes;
    std::vector<int> tempTransferFamilyIndexes;
    std::vector<int> tempSparseFamilyIndexes;
    std::vector<int> tempPresentFamilyIndexes;

    for(int i = 0;i < totalQueueFamiliesSupported;i += 1) {
        auto temp = string_VkQueueFlags(queueFamilyList[i].queueFlags);
        char row[160];
        std::snprintf(row, sizeof(row), "#%u  count=%-2u  %s\n", i, queueFamilyList[i].queueCount, string_VkQueueFlags(queueFamilyList[i].queueFlags).c_str());
        table += row;

        VkBool32 presentSupported = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, vulkanSurface, &presentSupported);

        if(queueFamilyList[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            tempGraphicsFamilyIndexes.push_back(i);
        }
        if(queueFamilyList[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
            tempComputeFamilyIndexes.push_back(i);
        }
        if(queueFamilyList[i].queueFlags & VK_QUEUE_TRANSFER_BIT) {
            tempTransferFamilyIndexes.push_back(i);
        }
        if(queueFamilyList[i].queueFlags & VK_QUEUE_SPARSE_BINDING_BIT) {
            tempSparseFamilyIndexes.push_back(i);
        }
        if(presentSupported) {
            tempPresentFamilyIndexes.push_back(i);
        }
    }

    LOG_INFO("%s", table.c_str());

    // LOG_INFO("%s GPU have %d Graphics Queue Family and %d Present Queue Family", properties.deviceName, graphicsFamilyIndexes.size())

    if(tempGraphicsFamilyIndexes.size() == 0) {
        LOG_ERROR("%s does not support graphics family", properties.deviceName);
        return -1;
    }
    if(tempPresentFamilyIndexes.size() == 0) {
        LOG_ERROR("%s does not support presentation queue family.", properties.deviceName);
        return -1;
    }

    if(vulkanPhysicalDevice == VK_NULL_HANDLE && properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        vulkanPhysicalDevice = physicalDevice;

        graphicsFamilyIndexes   = std::move(tempGraphicsFamilyIndexes);
        computeFamilyIndexes    = std::move(tempComputeFamilyIndexes);
        sparseFamilyIndexes     = std::move(tempSparseFamilyIndexes);
        transferFamilyIndexes   = std::move(tempTransferFamilyIndexes);
        presentFamilyIndexes    = std::move(tempPresentFamilyIndexes);
    }

    return 0;
}
