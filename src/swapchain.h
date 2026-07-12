#pragma once

// Day 3
struct Swapchain{
    VkSwapchainKHR swapchain;

    std::vector<VkImage> images;

    uint32_t width, height;
    uint32_t imageCount;
};

typedef struct GLFWwindow GLFWwindow;

VkSurfaceKHR createSurface(VkInstance instance, GLFWwindow *window);
VkFormat getSwapchainFormat(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface);
void createSwapchain(Swapchain& result, VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface, uint32_t familyIndex, VkFormat format, uint32_t width, uint32_t height, VkRenderPass renderPass, VkSwapchainKHR oldSwapchain = 0);
void destroySwapchain(VkDevice device, const Swapchain& swapchain);
bool resizeSwapchainIfNecessary(Swapchain& result, VkDevice device, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t familyIndex, VkFormat format, VkRenderPass renderPass);
