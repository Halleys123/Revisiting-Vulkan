#include "VulkanRenderer.hpp"
#include "GLFW/glfw3.h"
#include "Logger.hpp"

#include <cstring>
#include <vulkan/vk_enum_string_helper.h>

#include <stdexcept>
#include <vector>

VulkanRenderer::~VulkanRenderer() {
    this->cleanup();
}

void VulkanRenderer::initialize() {
    createVulkanInstance();
    getPhysicalDevice();
    createLogicalDevice();
}

void VulkanRenderer::cleanup() {
    vkDestroyDevice(vulkanLogicalDevice, nullptr);
    vkDestroyInstance(vulkanInstance, nullptr);
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
    VkDeviceCreateInfo logicalDeviceCreateInfo = {};

    logicalDeviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    logicalDeviceCreateInfo.enabledExtensionCount = this->requiredDeviceExtensions.size();
    logicalDeviceCreateInfo.ppEnabledExtensionNames = this->requiredDeviceExtensions.data();
    logicalDeviceCreateInfo.enabledLayerCount = 0;
    logicalDeviceCreateInfo.ppEnabledLayerNames = nullptr;
    // logicalDeviceCreateInfo.flags = This is not used now as per vulkan reference and maybe used in future


    // VkResult result = vkCreateDevice(vulkanPhysicalDevice, &logicalDeviceCreateInfo, nullptr, &vulkanLogicalDevice);

    // if(result != VK_SUCCESS) {
    //     LOG_CRITICAL("Unable to create Vulkan Logical Device");
    //     LOG_CRITICAL("Exiting program");
    //     throw std::runtime_error("Unable to create Vulkan Logical Device");
    // }
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

    bool graphicsFamilySupport = false;

    LOG_INFO("Queue Family details for %s", properties.deviceName);
    std::string table = "Queue families:\n";
    for(int i = 0;i < totalQueueFamiliesSupported;i += 1) {
        auto temp = string_VkQueueFlags(queueFamilyList[i].queueFlags);
        char row[160];
        std::snprintf(row, sizeof(row), "#%u  count=%-2u  %s\n", i, queueFamilyList[i].queueCount, string_VkQueueFlags(queueFamilyList[i].queueFlags).c_str());
        table += row;

        if(queueFamilyList[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphicsFamilyIndex = i;
            graphicsFamilySupport = true;
        }
    }

    LOG_INFO("%s", table.c_str());

    if(graphicsFamilySupport == false) {
        LOG_ERROR("%s does not support graphics family", properties.deviceName);
        return -1;
    }

    if(vulkanPhysicalDevice == VK_NULL_HANDLE) {
        vulkanPhysicalDevice = physicalDevice;
    }
    else {
        if(properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            vulkanPhysicalDevice = physicalDevice;
        }
    }

    return 0;
}
