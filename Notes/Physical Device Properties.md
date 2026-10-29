# Physical Device Properties in Vulkan

When you query physical device properties, you are pulling static hardware specs, vendor details, and a massive sub-struct of hardware limits (`VkPhysicalDeviceLimits`) that your engine code must respect to avoid validation errors or crashes.

## Part 1: General Device Information (`VkPhysicalDeviceProperties`)

This section tells you what the physical hardware is and who manufactured it.

- **`apiVersion` & `driverVersion`:** The highest Vulkan version supported by the driver and the specific vendor driver version integer.
- **`vendorID` & `deviceID`:** PCI vendor and device IDs (e.g., identifying NVIDIA (`0x10DE`), AMD (`0x1002`), or Intel (`0x8086`)). Used frequently for vendor-specific workarounds.
- **`deviceName`:** A null-terminated string containing the human-readable name of the GPU (e.g., "NVIDIA GeForce RTX 3050" or "AMD Radeon Graphics").
- **`deviceType` (`VkPhysicalDeviceType`):**
  - `VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU`: Dedicated graphics card (usually preferred for high-performance rendering).
  - `VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU`: Integrated graphics sharing system RAM.
  - `VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU`: Virtualized/Cloud GPU.
  - `VK_PHYSICAL_DEVICE_TYPE_CPU`: Software fallback/CPU rasterizer (like Lavapipe).
- **`pipelineCacheUUID`:** A universally unique identifier for the pipeline cache, ensuring you don't load binary pipelines compiled for a completely different GPU architecture or driver version.

## Part 2: The Critical Hardware Limits (`VkPhysicalDeviceLimits`)

This is the largest and most important part of the properties struct. If your renderer exceeds any of these hardware-enforced limits, the validation layers will flag it immediately.

### 1. Alignment & Memory Limits

- **`minUniformBufferOffsetAlignment`:** (Extremely Important) The byte alignment required when binding dynamic uniform buffers (`VkDescriptorBufferInfo` offsets). If your dynamic uniform buffer offsets aren't a multiple of this value (e.g., 256 bytes on many desktop GPUs), your bindings will fail.
- **`minStorageBufferOffsetAlignment`:** Same as above, but for storage buffers (SSBOs).
- **`bufferImageGranularity`:** The byte boundary separation required when packing linear buffers and optimal-tiling images in the same memory allocation.

### 2. Resource Dimension Limits

- **`maxImageDimension1D`, `maxImageDimension2D`, `maxImageDimension3D`:** Maximum width, height, and depth for textures (e.g., 16384 or 32768 pixels on modern GPUs).
- **`maxImageArrayLayers`:** Maximum number of layers in a texture array.
- **`maxBoundDescriptorSets`:** The maximum number of descriptor sets that can be bound simultaneously at once (guaranteed to be at least 4).
- **`maxPushConstantsSize`:** (Crucial for fast data passing) The maximum size in bytes for push constants (guaranteed minimum is 128 bytes, but modern GPUs usually offer 256 bytes).

### 3. Shader & Workgroup Limits

- **`maxComputeWorkGroupCount [X, Y, Z]`:** Maximum number of local workgroups dispatchable in a single compute dispatch call.
- **`maxComputeWorkGroupSize [X, Y, Z]`:** Maximum number of threads allowed inside a single compute workgroup (often 1024 total threads per workgroup).
- **`maxPerStageDescriptorSamplers`, `maxPerStageDescriptorUniformBuffers`, etc.:** Limits on how many descriptors of each type can be accessed in a single shader stage.

### 4. Drawing & Geometry Limits

- **`maxDrawIndexedIndexValue`:** Maximum index value for indexed draw calls (typically `UINT32_MAX`).
- **`maxMemoryAllocationCount`:** The maximum number of individual `vkAllocateMemory` calls allowed concurrently (usually tens of thousands; this is why professional engines use custom sub-allocation memory allocators instead of allocating per-resource).

## Part 3: Sparse Memory Properties (`VkPhysicalDeviceSparseProperties`)

Contained within properties, these flags tell you if the GPU supports sparse resource virtualization (useful if you are writing custom virtual texturing or terrain streaming engines).

- **`residencyStandard2DBlockShape`:** Supports sparse binding for standard 2D textures using block shapes.
- **`residencyAlignedMipSize`:** Whether images with non-power-of-two dimensions can use sparse residency requirements.

## Part 4: Modern Properties via `vkGetPhysicalDeviceProperties2`

In modern Vulkan, standard `vkGetPhysicalDeviceProperties` is often chained with `VkPhysicalDeviceProperties2` using the `pNext` chain to fetch extended properties introduced in newer Vulkan versions or extensions.

- **`VkPhysicalDeviceDriverProperties`:** Provides official driver names (e.g., "NVIDIA") and driver build details independently of vendor IDs.
- **`VkPhysicalDeviceSubgroupProperties`:** Details hardware SIMD execution widths (subgroup size, supported subgroup operations).
- **`VkPhysicalDeviceTimelineSemaphoreProperties`:** Verifies hardware support limits for timeline semaphores.
