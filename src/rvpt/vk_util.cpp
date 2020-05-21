#include "vk_util.h"

#include <type_traits>
#include <utility>

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

Fence::Fence(VkDevice device, VkFenceCreateFlags flags) : device(device)
{
    VkFenceCreateInfo fenceCreateInfo{};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags = flags;
    VK_CHECK_RESULT(vkCreateFence(device, &fenceCreateInfo, nullptr, &fence));
}

Fence::~Fence()
{
    if (fence != nullptr) vkDestroyFence(device, fence, nullptr);
}

Fence::Fence(Fence&& other) noexcept : device(other.device), fence(other.fence)
{
    other.fence = nullptr;
}
Fence& Fence::operator=(Fence&& other) noexcept
{
    device = other.device;
    fence = other.fence;
    other.fence = nullptr;
    return *this;
}

bool Fence::check() const
{
    VkResult out = vkGetFenceStatus(device, fence);
    if (out == VK_SUCCESS)
        return true;
    else if (out == VK_NOT_READY)
        return false;
    assert(out == VK_SUCCESS || out == VK_NOT_READY);
    return false;
}

void Fence::wait(bool condition) const
{
    vkWaitForFences(device, 1, &fence, condition, DEFAULT_FENCE_TIMEOUT);
}

VkFence Fence::get() const { return fence; }

void Fence::reset() const { vkResetFences(device, 1, &fence); }

// Semaphore

Semaphore::Semaphore(VkDevice device) : device(device)
{
    VkSemaphoreCreateInfo semaphoreCreateInfo{};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VK_CHECK_RESULT(
        vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &semaphore));
}

Semaphore::~Semaphore()
{
    if (semaphore != nullptr) vkDestroySemaphore(device, semaphore, nullptr);
}

Semaphore::Semaphore(Semaphore&& other) noexcept
    : device(other.device), semaphore(other.semaphore)
{
    other.semaphore = nullptr;
}
Semaphore& Semaphore::operator=(Semaphore&& other) noexcept
{
    device = other.device;
    semaphore = other.semaphore;
    other.semaphore = nullptr;
    return *this;
}

VkSemaphore Semaphore::get() const { return semaphore; }

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

CommandPool::CommandPool(VkDevice device, Queue const& queue,
                         VkCommandPoolCreateFlags flags)
    : device(device)
{
    VkCommandPoolCreateInfo cmd_pool_info{};
    cmd_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmd_pool_info.queueFamilyIndex = queue.get_family();
    cmd_pool_info.flags = flags;
    VK_CHECK_RESULT(
        vkCreateCommandPool(device, &cmd_pool_info, nullptr, &pool));
}

CommandPool::~CommandPool()
{
    if (pool != nullptr) vkDestroyCommandPool(device, pool, nullptr);
}

CommandPool::CommandPool(CommandPool&& other) noexcept
    : device(other.device), pool(other.pool)
{
    other.pool = nullptr;
}
CommandPool& CommandPool::operator=(CommandPool&& other) noexcept
{
    device = other.device;
    pool = other.pool;
    other.pool = nullptr;
    return *this;
}

VkCommandBuffer CommandPool::allocate()
{
    VkCommandBuffer command_buffer;

    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1;

    VK_CHECK_RESULT(
        vkAllocateCommandBuffers(device, &alloc_info, &command_buffer));
    return command_buffer;
}
void CommandPool::free(VkCommandBuffer command_buffer)
{
    vkFreeCommandBuffers(device, pool, 1, &command_buffer);
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
// ShaderModule

}  // namespace VK