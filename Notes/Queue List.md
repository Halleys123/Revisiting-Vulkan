# 🟢 The "Core & Universal" Queues

These are the primary execution engines present on virtually every modern consumer GPU (NVIDIA, AMD, Intel, Apple/MoltenVK).

### `VK_QUEUE_GRAPHICS_BIT`

- **What it handles:** Vertex/fragment shaders, rasterization, drawing, and framebuffers.
- **The Rule:** By Vulkan specification, any queue family that exposes the graphics bit must also implicitly support transfer operations (`VK_QUEUE_TRANSFER_BIT`). On almost all modern hardware, it also supports compute operations. This is your primary "do-it-all" queue.

### `VK_QUEUE_COMPUTE_BIT`

- **What it handles:** Compute shaders (parallel number crunching, physics, particle simulations, image filters).
- **Usage:** Can exist as part of the graphics queue, or as a completely separate, dedicated compute family for asynchronous workloads.

### `VK_QUEUE_TRANSFER_BIT`

- **What it handles:** Memory copying operations (buffer-to-buffer, buffer-to-image, clearing memory).
- **Usage:** Used for background staging and asset uploading without stalling the graphics pipeline. (Note: Even if a queue doesn't have a dedicated transfer family, your graphics queue can always handle transfers).

---

# 🟡 The "Advanced & Specialized" Queues

These handle niche system-level features or hardware capabilities.

### `VK_QUEUE_SPARSE_BINDING_BIT`

- **What it handles:** Sparse memory management (virtual texturing).
- **Usage:** Allows the GPU to dynamically map and unmap small physical memory pages to a massive virtual resource on the fly without reallocating the whole buffer/image. Essential for massive open-world terrain streaming.

### `VK_QUEUE_PROTECTED_BIT`

- **What it handles:** Hardware-protected memory queues.
- **Usage:** Used primarily for DRM-protected content pipelines (like playing encrypted 4K streaming video or commercial media where screen-recording hardware protection is legally required).

### `VK_QUEUE_VIDEO_DECODE_BIT_KHR` & `VK_QUEUE_VIDEO_ENCODE_BIT_KHR`

- **What it handles:** Hardware-accelerated video decoding and encoding (using the GPU's fixed-function NVDEC/NVENC or equivalent blocks).
- **Usage:** Used in media playback software, video editors, or game streaming applications to process video streams natively on the GPU without using CPU cycles.

---

# 🔴 Vendor-Specific Extension Queues

### `VK_QUEUE_OPTICAL_FLOW_BIT_NV` (NVIDIA-only)

- **What it handles:** Hardware optical flow and motion vector estimation (used heavily in technologies like DLSS frame generation).
- **Usage:** Accessed via NVIDIA-specific extensions (`VK_NV_optical_flow`) to calculate pixel motion between frames on dedicated hardware blocks.
