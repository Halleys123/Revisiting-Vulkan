## Vulkan Extension Guide

---

### 🟢 The "Most Used" (Essential)

These extensions are widely used because they drive window creation, debugging, and cross-vendor compatibility.

*   **VK_KHR_surface & VK_KHR_win32_surface:** Mandatory for any standard Windows application that renders graphics to a window. Without these, you cannot create a swapchain. This one is mostly obtained through whatever windowing system we are using like for glfw we might use `glfwGetRequiredInstanceExtensions `
*   **VK_EXT_debug_utils:** Mandatory for development. It runs the Vulkan Validation Layers, captures error messages, and lets you name objects for debugging.
*   **VK_KHR_portability_enumeration:** Mandatory if you want your code to run on Apple devices via MoltenVK (and highly recommended to always enable to prevent crashes on non-conformant drivers).
*   **VK_KHR_get_physical_device_properties2:** Highly Used / Core. Essential for checking modern GPU features (like ray tracing or mesh shading) before creating the device.

### 🟡 The "Conditional / Niche" (Used for specific features)

You only need to enable these if your application actively implements a specific advanced feature.

*   **VK_EXT_swapchain_colorspace:** Used if you are implementing HDR (High Dynamic Range) rendering.
*   **VK_KHR_get_surface_capabilities2 & VK_KHR_surface_protected_capabilities:** Used if you are handling DRM / Protected Content (like a Netflix app preventing screen capture).
*   **VK_LUNARG_direct_driver_loading:** Used mostly by developers creating custom engines or developer tools to force Vulkan to load a specific driver binary without installing it globally.
*   **VK_KHR_device_group_creation:** Used only if you are explicitly targeting Multi-GPU (SLI/Crossfire) rendering setups.
*   **VK_KHR_surface_maintenance1 (and VK_EXT_surface_maintenance1):** Used for advanced window resizing behaviors and min/max image control during crazy window layout changes.

### 🔴 The "Not Used / Obsolete" (Remove or avoid)

These should generally not be used in modern Vulkan code. They fall into two categories: completely replaced, or core functionality in modern Vulkan.

**1. Replaced by newer Extensions (Do not use)**

*   **VK_EXT_debug_report:** Obsolete. This is the old debugging extension. It has been entirely replaced by VK_EXT_debug_utils.

**2. Replaced by Vendor-Agnostic Standards (Do not use)**

*   **VK_NV_external_memory_capabilities:** Obsolete. This was an old, NVIDIA-specific way to handle external memory. Use the standard `VK_KHR_external_memory_capabilities` instead.

**3. Promoted to Core in Vulkan 1.1 (Only use if targeting Vulkan 1.0)**

If your application targets Vulkan 1.1 or higher, you should not manually enable these. They are built into the API by default. Only enable them if you are strictly targeting original Vulkan 1.0:
*   `VK_KHR_get_physical_device_properties2` (Core 1.1)
*   `VK_KHR_device_group_creation` (Core 1.1)
*   `VK_KHR_external_fence_capabilities` (Core 1.1)
*   `VK_KHR_external_memory_capabilities` (Core 1.1)
*   `VK_KHR_external_semaphore_capabilities` (Core 1.1)

**4. Headless / VR Only (Unused for normal windowed apps)**

*   **VK_KHR_display, VK_KHR_get_display_properties2, and VK_EXT_direct_mode_display:** These bypass the Windows desktop entirely to render directly to a raw screen output. Unless you are building an embedded system, a custom VR runtime, or a Linux arcade machine, you will not use these.

To help you narrow down the exact list to enable:
*   What Vulkan Version (e.g., 1.1, 1.3) are you initializing?
*   Is this list what your GPU supports, or what you are currently enabling in your code?
