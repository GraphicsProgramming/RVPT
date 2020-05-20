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
    explicit Queue(VkQueue queue, uint32_t family);

    void submit(std::vector<CommandBuffer> const& command_buffers,
                Fence& fence);
    void submit(std::vector<CommandBuffer> const& command_buffers,
                Fence const& fence,
                std::vector<Semaphore> const& wait_semaphores,
                std::vector<Semaphore> const& signal_semaphores,
                VkPipelineStageFlags const stage_mask);

    void wait_idle();
    VkResult presentation_submit(VkPresentInfoKHR present_info);

    VkQueue get() const;
    int get_family() const;

   private:
    void submit(VkSubmitInfo const& submitInfo, Fence const& fence);

    std::mutex submit_mutex;
    VkDevice device;
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
    FrameResources(VkDevice, Queue& queue, VkSwapchainKHR swapchain);
    ~FrameResources();
    FrameResources(FrameResources const& fence) = delete;
    FrameResources& operator=(FrameResources const& fence) = delete;
    FrameResources(FrameResources&& other) noexcept;
    FrameResources& operator=(FrameResources&& other) noexcept;

    void prepare_next_frame();
    void present_next_frame();

    Semaphore image_avail_sem;
    Semaphore render_finish_sem;

    CommandBuffer command_buffer;
};
}  // namespace VK