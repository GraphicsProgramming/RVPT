#pragma once

#include <cstdio>
#include <cassert>
#include <cstdint>

#include <iostream>
#include <mutex>
#include <variant>
#include <vector>
#include <utility>

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
    explicit HandleWrapper(VkDevice device, T handle, Deleter deleter)
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
    explicit FrameResources(VkDevice device, Queue& graphics_queue,
                            Queue& present_queue, VkSwapchainKHR swapchain);

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
    explicit DescriptorUse(uint32_t bind_point, uint32_t count,
                           VkDescriptorType type,
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
    explicit DescriptorSet(VkDevice device, VkDescriptorSet set,
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
   public:
    explicit DescriptorPool(
        VkDevice device,
        std::vector<VkDescriptorSetLayoutBinding> const& bindings,
        uint32_t count);

    DescriptorSet allocate();
    void free(DescriptorSet set);

   private:
    HandleWrapper<VkDescriptorSetLayout, PFN_vkDestroyDescriptorSetLayout>
        layout;
    HandleWrapper<VkDescriptorPool, PFN_vkDestroyDescriptorPool> pool;
    uint32_t max_sets = 0;
    uint32_t current_sets = 0;
};

VkRenderPass create_render_pass(VkDevice device,
                                VkFormat swapchain_image_format);
void destroy_render_pass(VkDevice device, VkRenderPass render_pass);
struct Framebuffer
{
    Framebuffer(VkDevice device, VkRenderPass render_pass, VkExtent2D extent,
                std::vector<VkImageView> image_views);
    HandleWrapper<VkFramebuffer, PFN_vkDestroyFramebuffer> framebuffer;
};

struct ShaderModule
{
    ShaderModule(VkDevice device, std::vector<uint32_t> const& spirv_code);
    HandleWrapper<VkShaderModule, PFN_vkDestroyShaderModule> module;
};

std::vector<uint32_t> load_spirv(std::string const& filename);

struct Pipeline
{
    VkPipelineLayout layout;
    VkPipeline pipeline;
};

struct PipelineBuilder
{
    PipelineBuilder() {}  // Must give it the device before using it.
    explicit PipelineBuilder(VkDevice device);

    Pipeline create_graphics_pipeline(
        std::string vert_shader, std::string frag_shader,
        std::vector<VkDescriptorSetLayout> descriptor_layouts,
        VkRenderPass render_pass, VkExtent2D extent);
    Pipeline create_compute_pipeline(
        std::string compute_shader,
        std::vector<VkDescriptorSetLayout> descriptor_layouts);

    void shutdown();

   private:
    VkDevice device = nullptr;
    VkPipelineCache cache = nullptr;

    std::vector<Pipeline> pipelines;

    void create_pipeline_layout(
        Pipeline& pipeline,
        std::vector<VkDescriptorSetLayout> const& descriptor_layouts);
};

enum class MemoryUsage
{
    gpu,
    cpu,
    transfer_to_gpu
};
class Memory
{
   public:
    Memory() {}
    explicit Memory(VkPhysicalDevice physical_device, VkDevice device);

    void shutdown();

    bool allocate_image(VkImage image, VkDeviceSize size, MemoryUsage usage);
    bool allocate_buffer(VkBuffer buffer, VkDeviceSize size, MemoryUsage usage);

    void free(VkImage image);
    void free(VkBuffer buffer);

   private:
    // Unused currently
    struct Pool
    {
        Pool(VkDevice device, VkDeviceMemory device_memory,
             VkDeviceSize max_size);
        HandleWrapper<VkDeviceMemory, PFN_vkFreeMemory> device_memory;
        VkDeviceSize max_size;
        struct Allocation
        {
            bool allocated = false;
            VkDeviceSize size;
            VkDeviceSize offset;
        };
        std::vector<Allocation> allocations;
    };

    VkPhysicalDevice physical_device;
    VkDevice device;
    VkPhysicalDeviceMemoryProperties memory_properties;

    std::vector<
        std::pair<VkImage, HandleWrapper<VkDeviceMemory, PFN_vkFreeMemory>>>
        image_allocations;
    std::vector<
        std::pair<VkBuffer, HandleWrapper<VkDeviceMemory, PFN_vkFreeMemory>>>
        buffer_allocations;

    VkMemoryPropertyFlags get_memory_property_flags(MemoryUsage usage);

    uint32_t find_memory_type(uint32_t typeFilter,
                              VkMemoryPropertyFlags properties);

    HandleWrapper<VkDeviceMemory, PFN_vkFreeMemory> create_device_memory(
        VkDeviceSize max_size, uint32_t memory_type);
    //        VkMemoryRequirements memRequirements, VkMemoryPropertyFlags
    //        properties);
};

class Image
{
   public:
    explicit Image(VkDevice device, Memory& memory, VkFormat format,
                   VkImageTiling tiling, uint32_t width, uint32_t height,
                   VkImageUsageFlags usage, VkDeviceSize size,
                   MemoryUsage memory_usage);
    ~Image();

    VkDescriptorImageInfo descriptor_info() const;

    Memory* memory_ptr;
    HandleWrapper<VkImage, PFN_vkDestroyImage> image;
    bool successfully_got_memory;
    HandleWrapper<VkImageView, PFN_vkDestroyImageView> image_view;
    HandleWrapper<VkSampler, PFN_vkDestroySampler> sampler;

    VkFormat format = VK_FORMAT_UNDEFINED;
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
};

class Buffer
{
   public:
    explicit Buffer(VkDevice device, Memory& memory, VkBufferUsageFlags usage,
                    VkDeviceSize size, MemoryUsage memory_usage);
    ~Buffer();

    VkDescriptorBufferInfo descriptor_info() const;

    Memory* memory_ptr;
    HandleWrapper<VkBuffer, PFN_vkDestroyBuffer> buffer;
    VkDeviceSize size;
};

void set_image_layout(
    VkCommandBuffer command_buffer, VkImage image,
    VkImageLayout old_image_layout, VkImageLayout new_image_layout,
    VkImageSubresourceRange subresource_range,
    VkPipelineStageFlags src_stage_mask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
    VkPipelineStageFlags dst_stage_mask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

}  // namespace VK