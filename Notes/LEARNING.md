On a higher level when creating a vulkan rendered we just need to remember that:

### Step 1 - Setting up Vulkan Instance

1. We start by creating a Vulkan Instance, rest information i.e. what structs we need to make can be deduced from the primary function (`vkCreateInstance`) itself. 
2. This step asks us to clarify on extensions and layers to enable at instance level, one of important extension is `VK_KHR_win32_surface`, to know more about this read [Instance Level Extensions](./Instance%20Extensions.md)

# Step 2 - Get Surface

As Vulkan is platform indipendent, so it does not know anything about where to display images or about window etc, so surface act as a platform-neutral handle where vulkan can tell to draw images. It is just a connection between Vulkan and Window (not OS but the place where we draw things).

We can simply use `glfwCreateWindowSurface` function to create the `VkSurfaceKHR` handle

# Step 3 - Physical and Logical Device Setup

## Physical Device

There are many features that can be checked when creating a physical device that includes hardware limits, device properties, memory properties. More information [here](./Physical%20Device%20Properties.md)

There are three primary things we need to confirm when selecting a physical device

The primary functions that are used on physical device are

1. `vkGetPhysicalDeviceProperties`
2. `vkGetPhysicalDeviceFeatures`
3. `vkGetPhysicalDeviceQueueFamilyProperties`
4. `vkEnumerateDeviceExtensionProperties`
5. `vkGetPhysicalDeviceSurfaceSupportKHR`

### Extension Support

Does the physical device support the mandatory `VK_KHR_swapchain` extension, this is the primary extension that allows displaying to the window on screen. Primary function used in this is `vkEnumerateDeviceExtensionProperties`

b. For setting up device we simply pass the extensions that we might need at device level, while layers are deprecated at logical device level. Most used extension at device level is `VK_KHR_swapchain`, for getting more details about the device level extensions read [Device Level Extensions](./Device%20Extensions.md)

### Queue Families

Find physical device available on machine (get means enumerate) which supports the `queue families` that we need in our application using `vkGetPhysicalDeviceQueueFamilyProperties`. 

More information about queues that might be supported by GPU's are [Physical Device](./Queue%20List.md)


### Surface - Presentation Support

We need to check is whether the graphics queue family we selected in GPU have support to display to window or not, many server GPU are able to render but can't support display to window directly and are rather suppposed to move the rendered content to a file system or server or database. Function used to check this is `vkGetPhysicalDeviceSurfaceSupportKHR`

## Logical Device

In this section we primarily pass information about which queue we want to use and what are their index, we get this information from physical device itself.

In this part we have two important things to do:

1. Create Logical Device using `vkCreateDevice`
2. Get Queue Handles using `vkGetDeviceQueue`

# Swapchain Creation

Vulkan does not have any infrastructure prebuilt that handles the images we create, so we need to create an infrastructure that will own the buffers we will render to before we visualize them on the screen. This is known as the `swapchain`.

swapchain is a queue of images that the display system owns and lends to you. Without one, you would render straight into the image the monitor is currently reading, and you’d see tearing and flicker.
it’s a device object whose parameters are dictated by the surface.

The general purpose of the swap chain is to synchronize the presentation of images with the refresh rate of the screen.

When creating a swapchain we need to confirm three things:

1. Basic surface capabilities (min/max number of images in swap chain, min/max width and height of images)
2. Surface formats (pixel format, color space)
3. Available presentation modes

So after reading a lot about this I found that swapchain can be considered has a set of image (not framebuffers) which are finally rendered to the screen, and because these are being rendered to swapchain (not to screen, displayin to screen is a separate process) we need to check compatibility with the screen or more specifically the surface. We need to query three properties (min/max images, min/max width and height of images, surface formats [pixel format, color space], presentation modes supported by surface). The queries return what the GPU and surface combination offers, and you then choose from those offers. 
During GPU selection you only reject a GPU if the list of formats or the list of present modes is empty

`Summary`: So this step is mostly about querying what is present on current system and then selecting from one of the available options, while handling edge cases (like having infinite max image)
