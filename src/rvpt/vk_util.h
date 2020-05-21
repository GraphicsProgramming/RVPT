#pragma once

#include <cstdio>
#include <cassert>

#include <iostream>
#include <mutex>
#include <vector>

#include <vulkan/vulkan.h>

namespace VK
{
const char* error_str(const VkResult result);

#define VK_CHECK_RESULT(f)                                                \
    {                                                                     \
        VkResult res = (f);                                               \
        if (res != VK_SUCCESS)                                            \
        {                                                                 \
            std::cerr << "Fatal : VkResult is" << error_str(res) << "in " \
                      << __FILE__ << "at line" << __LINE__ << '\n';       \
            assert(res == VK_SUCCESS);                                    \
        }                                                                 \
    }

class Fence
{
   public:
    explicit Fence(VkDevice device, VkFenceCreateFlags flags = 0);
    ~Fence();

    Fence(Fence const& fence) = delete;
    Fence& operator=(Fence const& fence) = delete;

    Fence(Fence&& other) noexcept;
    Fence& operator=(Fence&& other) noexcept;

    bool check() const;
    void wait(bool condition = true) const;
    void reset() const;
    VkFence get() const;

   private:
    VkDevice device;
    VkFence fence;
};

class Semaphore
{
   public:
    explicit Semaphore(VkDevice device);
    ~Semaphore();

    Semaphore(Semaphore const& fence) = delete;
    Semaphore& operator=(Semaphore const& fence) = delete;

    Semaphore(Semaphore&& other) noexcept;
    Semaphore& operator=(Semaphore&& other) noexcept;

    VkSemaphore get() const;

   private:
    VkDevice device;
    VkSemaphore semaphore;
};

class CommandBuffer;

class Queue
{
   public:
    explicit Queue(VkDevice device, uint32_t family, uint32_t queue_index = 0);

    void submit(CommandBuffer const& command_buffer, Fence& fence);
    void submit(CommandBuffer const& command_buffer, Fence const& fence,
                Semaphore const& wait_semaphore,
                Semaphore const& signal_semaphore,
                VkPipelineStageFlags const stage_mask);

    void wait_idle();
    VkResult presentation_submit(VkPresentInfoKHR present_info);

    VkQueue get() const;
    int get_family() const;

   private:
    void submit(VkSubmitInfo const& submitInfo, Fence const& fence);

    std::mutex submit_mutex;
    VkQueue queue;
    int queue_family;
};

class CommandPool
{
   public:
    explicit CommandPool(VkDevice device, Queue const& queue,
                         VkCommandPoolCreateFlags flags = 0);
    ~CommandPool();

    CommandPool(CommandPool const& fence) = delete;
    CommandPool& operator=(CommandPool const& fence) = delete;
    CommandPool(CommandPool&& other) noexcept;
    CommandPool& operator=(CommandPool&& other) noexcept;

    VkCommandBuffer allocate();
    void free(VkCommandBuffer command_buffer);

   private:
    VkDevice device;
    VkCommandPool pool;
};

class CommandBuffer
{
   public:
    explicit CommandBuffer(VkDevice device, Queue const& queue);
    ~CommandBuffer();

    CommandBuffer(CommandBuffer const& fence) = delete;
    CommandBuffer& operator=(CommandBuffer const& fence) = delete;
    CommandBuffer(CommandBuffer&& other) noexcept;
    CommandBuffer& operator=(CommandBuffer&& other) noexcept;

    VkCommandBuffer get() const;

    void begin(VkCommandBufferUsageFlags flags = 0);
    void end();
    void reset();

   private:
    VkDevice device;
    Queue const* queue;
    CommandPool pool;
    VkCommandBuffer command_buffer;
};

class FrameResources
{
   public:
    FrameResources(VkDevice device, Queue& graphics_queue, Queue& present_queue,
                   VkSwapchainKHR swapchain);

    void submit();
    VkResult present(uint32_t image_index);

    Queue& graphics_queue;
    Queue& present_queue;
    VkSwapchainKHR swapchain;

    Semaphore image_avail_sem;
    Semaphore render_finish_sem;

    Fence command_fence;
    CommandBuffer command_buffer;
};

enum class shader_type
{
    vertex,
    fragment
};

class ShaderModule
{
    ShaderModule(shader_type type, std::vector<uint32_t> const& code);
    ~ShaderModule();
    ShaderModule(ShaderModule const& fence) = delete;
    ShaderModule& operator=(ShaderModule const& fence) = delete;
    ShaderModule(ShaderModule&& other) noexcept;
    ShaderModule& operator=(ShaderModule&& other) noexcept;

    VkShaderModule get() const;
};

}  // namespace VK