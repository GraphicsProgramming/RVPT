#include "vk_util.h"

#include <type_traits>
#include <utility>
#include <unordered_map>
namespace VK
{
const char* error_str(const VkResult result)
{
    switch (result)
    {
#define STR(r)   \
    case VK_##r: \
        return #r
        STR(NOT_READY);
        STR(TIMEOUT);
        STR(EVENT_SET);
        STR(EVENT_RESET);
        STR(INCOMPLETE);
        STR(ERROR_OUT_OF_HOST_MEMORY);
        STR(ERROR_OUT_OF_DEVICE_MEMORY);
        STR(ERROR_INITIALIZATION_FAILED);
        STR(ERROR_DEVICE_LOST);
        STR(ERROR_MEMORY_MAP_FAILED);
        STR(ERROR_LAYER_NOT_PRESENT);
        STR(ERROR_EXTENSION_NOT_PRESENT);
        STR(ERROR_FEATURE_NOT_PRESENT);
        STR(ERROR_INCOMPATIBLE_DRIVER);
        STR(ERROR_TOO_MANY_OBJECTS);
        STR(ERROR_FORMAT_NOT_SUPPORTED);
        STR(ERROR_SURFACE_LOST_KHR);
        STR(ERROR_NATIVE_WINDOW_IN_USE_KHR);
        STR(SUBOPTIMAL_KHR);
        STR(ERROR_OUT_OF_DATE_KHR);
        STR(ERROR_INCOMPATIBLE_DISPLAY_KHR);
        STR(ERROR_VALIDATION_FAILED_EXT);
        STR(ERROR_INVALID_SHADER_NV);
#undef STR
        default:
            return "UNKNOWN_ERROR";
    }
}

template <typename T>
auto gather_vk_types(std::vector<T> const& values)
{
    std::vector<decltype(std::declval<T>().get())> types;
    for (auto const& val : values) types.push_back(val.get());
    return types;
}

// Fence

constexpr long DEFAULT_FENCE_TIMEOUT = 1000000000;
namespace detail
{
auto create_fence(VkDevice device, VkFenceCreateFlags flags)
{
    VkFenceCreateInfo fenceCreateInfo{};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags = flags;
    VkFence handle;
    VK_CHECK_RESULT(vkCreateFence(device, &fenceCreateInfo, nullptr, &handle));
    return HandleWrapper(device, handle, vkDestroyFence);
}
}  // namespace detail
Fence::Fence(VkDevice device, VkFenceCreateFlags flags)
    : fence(detail::create_fence(device, flags))
{
}

bool Fence::check() const
{
    VkResult out = vkGetFenceStatus(fence.device, fence.handle);
    if (out == VK_SUCCESS)
        return true;
    else if (out == VK_NOT_READY)
        return false;
    assert(out == VK_SUCCESS || out == VK_NOT_READY);
    return false;
}

void Fence::wait(bool condition) const
{
    vkWaitForFences(fence.device, 1, &fence.handle, condition,
                    DEFAULT_FENCE_TIMEOUT);
}

VkFence Fence::get() const { return fence.handle; }

void Fence::reset() const { vkResetFences(fence.device, 1, &fence.handle); }

// Semaphore
namespace detail
{
auto create_semaphore(VkDevice device)
{
    VkSemaphoreCreateInfo semaphoreCreateInfo{};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkSemaphore semaphore;
    VK_CHECK_RESULT(
        vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &semaphore));
    return HandleWrapper(device, semaphore, vkDestroySemaphore);
}
}  // namespace detail
Semaphore::Semaphore(VkDevice device)
    : semaphore(detail::create_semaphore(device))
{
}

VkSemaphore Semaphore::get() const { return semaphore.handle; }

// Queue

Queue::Queue(VkDevice device, uint32_t queue_family, uint32_t queue_index)
    : queue_family(queue_family)
{
    vkGetDeviceQueue(device, queue_family, queue_index, &queue);
}

void Queue::submit(CommandBuffer const& command_buffer, Fence& fence)
{
    // auto cmd_bufs = gather_vk_types({command_buffer});
    VkCommandBuffer cmd_buf = command_buffer.get();

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd_buf;

    submit(submit_info, fence);
}

void Queue::submit(CommandBuffer const& command_buffer, Fence const& fence,
                   Semaphore const& wait_semaphore,
                   Semaphore const& signal_semaphore,
                   VkPipelineStageFlags const stage_mask)
{
    VkCommandBuffer cmd_buf = command_buffer.get();
    VkSemaphore signal_sem = signal_semaphore.get();
    VkSemaphore wait_sem = wait_semaphore.get();

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd_buf;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &signal_sem;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &wait_sem;
    submit_info.pWaitDstStageMask = &stage_mask;

    submit(submit_info, fence);
}

void Queue::submit(VkSubmitInfo const& submitInfo, Fence const& fence)
{
    std::lock_guard lock(submit_mutex);
    VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, fence.get()));
}

int Queue::get_family() const { return queue_family; }
VkQueue Queue::get() const { return queue; }

VkResult Queue::presentation_submit(VkPresentInfoKHR present_info)
{
    std::lock_guard lock(submit_mutex);
    return vkQueuePresentKHR(queue, &present_info);
}

void Queue::wait_idle()
{
    std::lock_guard lock(submit_mutex);
    vkQueueWaitIdle(queue);
}

// Command Pool
namespace detail
{
auto create_command_pool(VkDevice device, Queue const& queue,
                         VkCommandPoolCreateFlags flags)
{
    VkCommandPoolCreateInfo cmd_pool_info{};
    cmd_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmd_pool_info.queueFamilyIndex = queue.get_family();
    cmd_pool_info.flags = flags;
    VkCommandPool pool;
    VK_CHECK_RESULT(
        vkCreateCommandPool(device, &cmd_pool_info, nullptr, &pool));
    return HandleWrapper(device, pool, vkDestroyCommandPool);
}
}  // namespace detail
CommandPool::CommandPool(VkDevice device, Queue const& queue,
                         VkCommandPoolCreateFlags flags)
    : pool(detail::create_command_pool(device, queue, flags))
{
}

VkCommandBuffer CommandPool::allocate()
{
    VkCommandBuffer command_buffer;

    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = pool.handle;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1;

    VK_CHECK_RESULT(
        vkAllocateCommandBuffers(pool.device, &alloc_info, &command_buffer));
    return command_buffer;
}
void CommandPool::free(VkCommandBuffer command_buffer)
{
    vkFreeCommandBuffers(pool.device, pool.handle, 1, &command_buffer);
}

// Command Buffer
CommandBuffer::CommandBuffer(VkDevice device, Queue const& queue)
    : device(device),
      queue(&queue),
      pool(device, queue, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT)
{
    command_buffer = pool.allocate();
}
CommandBuffer::~CommandBuffer()
{
    if (command_buffer != nullptr) pool.free(command_buffer);
}

CommandBuffer::CommandBuffer(CommandBuffer&& other) noexcept
    : device(other.device),
      queue(other.queue),
      pool(std::move(other.pool)),
      command_buffer(other.command_buffer)
{
    other.command_buffer = nullptr;
}
CommandBuffer& CommandBuffer::operator=(CommandBuffer&& other) noexcept
{
    device = other.device;
    queue = other.queue;
    pool = std::move(other.pool);
    command_buffer = other.command_buffer;
    other.command_buffer = VK_NULL_HANDLE;
    return *this;
}

VkCommandBuffer CommandBuffer::get() const { return command_buffer; }

void CommandBuffer::begin(VkCommandBufferUsageFlags flags)
{
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = flags;

    VK_CHECK_RESULT(vkBeginCommandBuffer(command_buffer, &beginInfo));
}
void CommandBuffer::end()
{
    VK_CHECK_RESULT(vkEndCommandBuffer(command_buffer));
}
void CommandBuffer::reset()
{
    VK_CHECK_RESULT(vkResetCommandBuffer(command_buffer, {}));
}

// Frame Resources
FrameResources::FrameResources(VkDevice device, Queue& graphics_queue,
                               Queue& present_queue, VkSwapchainKHR swapchain)
    : graphics_queue(graphics_queue),
      present_queue(present_queue),
      swapchain(swapchain),
      image_avail_sem(device),
      render_finish_sem(device),
      command_fence(device, VK_FENCE_CREATE_SIGNALED_BIT),
      command_buffer(device, graphics_queue)
{
}

void FrameResources::submit()
{
    graphics_queue.submit(command_buffer, command_fence, image_avail_sem,
                          render_finish_sem,
                          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
}
VkResult FrameResources::present(uint32_t image_index)
{
    VkSemaphore wait_sem = render_finish_sem.get();
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &wait_sem;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain;
    presentInfo.pImageIndices = &image_index;

    return present_queue.presentation_submit(presentInfo);
}

// DescriptorUse
DescriptorUse::DescriptorUse(uint32_t bindPoint, uint32_t count,
                             VkDescriptorType type,
                             DescriptorUseVector descriptor_use_data)
    : bind_point(bind_point),
      count(count),
      type(type),
      descriptor_use_data(descriptor_use_data)
{
}

VkWriteDescriptorSet DescriptorUse::get_write_descriptor_set(
    VkDescriptorSet set)
{
    VkWriteDescriptorSet writeDescriptorSet{};
    writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSet.dstSet = set;
    writeDescriptorSet.descriptorType = type;
    writeDescriptorSet.dstBinding = bind_point;
    writeDescriptorSet.descriptorCount = count;

    if (descriptor_use_data.index() == 0)
        writeDescriptorSet.pBufferInfo =
            std::get<std::vector<VkDescriptorBufferInfo>>(descriptor_use_data)
                .data();
    else if (descriptor_use_data.index() == 1)
        writeDescriptorSet.pImageInfo =
            std::get<std::vector<VkDescriptorImageInfo>>(descriptor_use_data)
                .data();
    else if (descriptor_use_data.index() == 2)
        writeDescriptorSet.pTexelBufferView =
            std::get<std::vector<VkBufferView>>(descriptor_use_data).data();
    return writeDescriptorSet;
}

// DescriptorSet

DescriptorSet::DescriptorSet(VkDevice device, VkDescriptorSet set,
                             VkDescriptorSetLayout layout)
    : device(device), set(set), layout(layout)
{
}
void DescriptorSet::update(std::vector<DescriptorUse> descriptors) const
{
    std::vector<VkWriteDescriptorSet> writes;
    for (auto& descriptor : descriptors)
    {
        writes.push_back(descriptor.get_write_descriptor_set(set));
    }

    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()),
                           writes.data(), 0, nullptr);
}
void DescriptorSet::bind(VkCommandBuffer cmdBuf, VkPipelineBindPoint bind_point,
                         VkPipelineLayout layout, uint32_t location) const
{
    vkCmdBindDescriptorSets(cmdBuf, bind_point, layout, location, 1, &set, 0,
                            nullptr);
}

// DescriptorPool
namespace detail
{
auto create_descriptor_set_layout(
    VkDevice device, std::vector<VkDescriptorSetLayoutBinding> const& bindings)
{
    VkDescriptorSetLayoutCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    create_info.bindingCount = static_cast<uint32_t>(bindings.size());
    create_info.pBindings = bindings.data();

    VkDescriptorSetLayout layout;
    VK_CHECK_RESULT(
        vkCreateDescriptorSetLayout(device, &create_info, nullptr, &layout));
    return HandleWrapper(device, layout, vkDestroyDescriptorSetLayout);
}
auto create_descriptor_pool(
    VkDevice device, std::vector<VkDescriptorSetLayoutBinding> const& bindings,
    uint32_t max_sets)
{
    std::unordered_map<VkDescriptorType, uint32_t> descriptor_map;
    for (auto& binding : bindings)
    {
        descriptor_map[binding.descriptorType] += binding.descriptorCount;
    }
    std::vector<VkDescriptorPoolSize> pool_sizes;
    for (auto& [type, count] : descriptor_map)
    {
        pool_sizes.push_back(VkDescriptorPoolSize{type, max_sets * count});
    }

    VkDescriptorPoolCreateInfo create_info;
    create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    create_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
    create_info.pPoolSizes = pool_sizes.data();
    create_info.maxSets = max_sets;

    VkDescriptorPool pool;
    VK_CHECK_RESULT(
        vkCreateDescriptorPool(device, &create_info, nullptr, &pool));
    return HandleWrapper(device, pool, vkDestroyDescriptorPool);
}

}  // namespace detail
DescriptorPool::DescriptorPool(
    VkDevice device, std::vector<VkDescriptorSetLayoutBinding> const& bindings,
    uint32_t count)
    : layout(detail::create_descriptor_set_layout(device, bindings)),
      pool(detail::create_descriptor_pool(device, bindings, count)),
      max_sets(count)
{
}

DescriptorSet DescriptorPool::allocate()
{
    assert(current_sets < max_sets);
    VkDescriptorSet set;
    VkResult res = vkAllocateDescriptorSets(pool.device, nullptr, &set);
    assert(res == VK_SUCCESS);

    return {pool.device, set, layout.handle};
}
void DescriptorPool::free(DescriptorSet set)
{
    if (current_sets > 0)
        vkFreeDescriptorSets(pool.device, pool.handle, 1, &set.set);
}

// Render Pass

VkRenderPass create_render_pass(VkDevice device,
                                VkFormat swapchain_image_format)
{
    VkAttachmentDescription color_attachment{};
    color_attachment.format = swapchain_image_format;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_attachment_ref{};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo render_pass_info{};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = 1;
    render_pass_info.pAttachments = &color_attachment;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;
    render_pass_info.dependencyCount = 1;
    render_pass_info.pDependencies = &dependency;

    VkRenderPass render_pass;

    VK_CHECK_RESULT(
        vkCreateRenderPass(device, &render_pass_info, nullptr, &render_pass));
    return render_pass;
}

PipelineBuilder::PipelineBuilder(VkDevice device) : device(device) {}
VkPipeline PipelineBuilder::create_graphics_pipeline(std::string vert_shader,
                                                     std::string frag_shader)
{
    VkGraphicsPipelineCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;

    VkPipeline pipeline;
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, cache, 1, &create_info,
                                              nullptr, &pipeline));

    return pipeline;
}

VkPipeline PipelineBuilder::create_compute_pipeline(std::string compute_shader)
{
    VkComputePipelineCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;

    VkPipeline pipeline;
    VK_CHECK_RESULT(vkCreateComputePipelines(device, cache, 1, &create_info,
                                             nullptr, &pipeline));
    return pipeline;
}

}  // namespace VK