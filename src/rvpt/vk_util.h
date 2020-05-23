#pragma once

#include <cstdio>
#include <cassert>
#include <cstdint>

#include <iostream>
#include <mutex>
#include <variant>
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

template <typename T, typename Deleter>
class HandleWrapper
{
   public:
    HandleWrapper(VkDevice device, T handle, Deleter deleter)
        : device(device), handle(handle), deleter(deleter)
    {
    }
    ~HandleWrapper()
    {
        if (handle != nullptr)
        {
            deleter(device, handle, nullptr);
        }
    };
    HandleWrapper(HandleWrapper const& fence) = delete;
    HandleWrapper& operator=(HandleWrapper const& fence) = delete;

    HandleWrapper(HandleWrapper&& other) noexcept
        : device(other.device), handle(other.handle), deleter(other.deleter)
    {
        other.handle = nullptr;
    }
    HandleWrapper& operator=(HandleWrapper&& other) noexcept
    {
        device = other.device;
        handle = other.handle;
        deleter = other.deleter;
        other.handle = nullptr;
        return *this;
    }

    VkDevice device;
    T handle;
    Deleter deleter;
};

class Fence
{
   public:
    explicit Fence(VkDevice device, VkFenceCreateFlags flags = 0);

    bool check() const;
    void wait(bool condition = true) const;
    void reset() const;
    VkFence get() const;

   private:
    HandleWrapper<VkFence, PFN_vkDestroyFence> fence;
};

class Semaphore
{
   public:
    explicit Semaphore(VkDevice device);
    VkSemaphore get() const;

   private:
    HandleWrapper<VkSemaphore, PFN_vkDestroySemaphore> semaphore;
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

    VkCommandBuffer allocate();
    void free(VkCommandBuffer command_buffer);

   private:
    HandleWrapper<VkCommandPool, PFN_vkDestroyCommandPool> pool;
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

using DescriptorUseVector =
    std::variant<std::vector<VkDescriptorBufferInfo>,
                 std::vector<VkDescriptorImageInfo>, std::vector<VkBufferView>>;

class DescriptorUse
{
   public:
    DescriptorUse(uint32_t bindPoint, uint32_t count, VkDescriptorType type,
                  DescriptorUseVector descriptor_use_data);

    VkWriteDescriptorSet get_write_descriptor_set(VkDescriptorSet set);

    uint32_t bind_point;
    uint32_t count;
    VkDescriptorType type = VkDescriptorType::VK_DESCRIPTOR_TYPE_MAX_ENUM;
    DescriptorUseVector descriptor_use_data;
};

class DescriptorSet
{
   public:
    DescriptorSet(VkDevice device, VkDescriptorSet set,
                  VkDescriptorSetLayout layout);

    void update(std::vector<DescriptorUse> descriptors) const;
    void bind(VkCommandBuffer cmdBuf, VkPipelineBindPoint bind_point,
              VkPipelineLayout layout, uint32_t location) const;

    VkDevice device;
    VkDescriptorSet set;
    VkDescriptorSetLayout layout;
};

class DescriptorPool
{
    DescriptorPool(VkDevice device,
                   std::vector<VkDescriptorSetLayoutBinding> const& bindings,
                   uint32_t count);

    DescriptorSet allocate();
    void free(DescriptorSet set);

    HandleWrapper<VkDescriptorSetLayout, PFN_vkDestroyDescriptorSetLayout>
        layout;
    HandleWrapper<VkDescriptorPool, PFN_vkDestroyDescriptorPool> pool;
    uint32_t max_sets = 0;
    uint32_t current_sets = 0;
};

VkRenderPass create_render_pass(VkDevice device,
                                VkFormat swapchain_image_format);

struct PipelineBuilder
{
    PipelineBuilder() {}  // Must give it the device before using it.
    PipelineBuilder(VkDevice device);

    VkPipeline create_graphics_pipeline(std::string vert_shader,
                                        std::string frag_shader);
    VkPipeline create_compute_pipeline(std::string compute_shader);

   private:
    VkDevice device = nullptr;
    VkPipelineCache cache = nullptr;
};

}  // namespace VK