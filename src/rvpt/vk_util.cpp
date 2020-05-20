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

Queue::Queue(VkQueue queue, uint32_t queue_family)
    : device(device), queue(queue), queue_family(queue_family)
{
}

void Queue::submit(std::vector<CommandBuffer> const& command_buffers,
                   Fence& fence)
{
    auto cmd_bufs = gather_vk_types(command_buffers);

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = static_cast<uint32_t>(cmd_bufs.size());
    submit_info.pCommandBuffers = cmd_bufs.data();

    submit(submit_info, fence);
}

void Queue::submit(std::vector<CommandBuffer> const& command_buffers,
                   Fence const& fence,
                   std::vector<Semaphore> const& wait_semaphores,
                   std::vector<Semaphore> const& signal_semaphores,
                   VkPipelineStageFlags const stage_mask)
{
    auto cmd_bufs = gather_vk_types(command_buffers);
    auto signal_sems = gather_vk_types(signal_semaphores);
    auto wait_sems = gather_vk_types(wait_semaphores);

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = static_cast<uint32_t>(cmd_bufs.size());
    submit_info.pCommandBuffers = cmd_bufs.data();
    submit_info.signalSemaphoreCount =
        static_cast<uint32_t>(signal_sems.size());
    submit_info.pSignalSemaphores = signal_sems.data();
    submit_info.waitSemaphoreCount = static_cast<uint32_t>(wait_sems.size());
    submit_info.pWaitSemaphores = wait_sems.data();
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
    : pool(device, queue, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT)
{
    command_buffer = pool.allocate();
}
CommandBuffer::~CommandBuffer() { pool.free(command_buffer); }
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
}  // namespace VK