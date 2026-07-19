#define VK_USE_PLATFORM_XLIB_KHR
#include "common.h"
#include "swapchain.h"

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>

#include <stdio.h>
#include <algorithm>


#define VSYNC 1

VkSurfaceKHR createSurface(VkInstance instance, GLFWwindow *window){
#if defined(_WIN32)
    VkWin32SurfaceCreateInfoKHR createInfo = {VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
    createInfo.hinstance = GetModuleHandle(0);
    createInfo.hwnd = glfwGetWin32Window(window);

    VkSurfaceKHR surface = 0;
    VK_CHECK(vkCreateWin32SurfaceKHR(instance, &createInfo, 0, surface));
    return surface;
#else
    VkXlibSurfaceCreateInfoKHR createInfo = {VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR};
    createInfo.dpy = glfwGetX11Display();
    createInfo.window = glfwGetX11Window(window);

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VK_CHECK(vkCreateXlibSurfaceKHR(instance, &createInfo, nullptr, &surface));
    return surface;
#endif
}

VkFormat getSwapchainFormat(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface){

    uint32_t formatCounts = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCounts, 0);
    assert(formatCounts > 0); // TODO : this code might need to handle either formatCount being 0, or first element reporting format VK_FORMAT_UNDEFINED

    std::vector<VkSurfaceFormatKHR> formats(formatCounts);
    assert(formatCounts < ARRAYSIZE(formats));
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCounts, formats.data());

    printf("Format count : %d\n", formatCounts);
    if (formatCounts == 1 && formats[0].format == VK_FORMAT_UNDEFINED)
    {
        return VK_FORMAT_R8G8B8A8_UNORM;
    }

    // provides better picture quality in case of HDR pipelines
    // for (uint32_t i=0; i < formatCounts; ++i){
    //     if (formats[i].format == VK_FORMAT_A2R10G10B10_UNORM_PACK32 || formats[i].format == VK_FORMAT_A2B10G10R10_UNORM_PACK32){
    //         return formats[i].format;
    //     }
    // }

    for (uint32_t i = 0; i < formatCounts; ++i)
    {
        if (formats[i].format == VK_FORMAT_R8G8B8A8_UNORM || formats[i].format == VK_FORMAT_B8G8R8A8_UNORM)
        {
            return formats[i].format;
        }
    }

    return formats[0].format;
}

static VkSwapchainKHR createSwapchain(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface, VkSurfaceCapabilitiesKHR surfaceCaps, 
    uint32_t familyIndex, VkFormat format, uint32_t winWidth, uint32_t winHeight, VkSwapchainKHR oldSwapchain){

    VkCompositeAlphaFlagBitsKHR surfaceComposite =
        (surfaceCaps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) ? VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR : 
        (surfaceCaps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR) ? VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR : 
        (surfaceCaps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR)  ? VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR
                                                                                                                                                                                            : VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;

    VkSwapchainCreateInfoKHR createInfo = {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    createInfo.surface = surface;
    createInfo.minImageCount = std::max(2u, surfaceCaps.minImageCount);
    createInfo.imageFormat = format;
    createInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    createInfo.imageExtent.width = winWidth;
    createInfo.imageExtent.height = winHeight;
    // createInfo.imageExtent = surfaceCaps.currentExtent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT ; // TODO : remove color attachment
    createInfo.queueFamilyIndexCount = 1;
    createInfo.pQueueFamilyIndices = &familyIndex;
    // createInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    createInfo.preTransform = surfaceCaps.currentTransform;
    createInfo.compositeAlpha = surfaceComposite;
    createInfo.presentMode = VSYNC ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR;
    createInfo.oldSwapchain = oldSwapchain;

    VkSwapchainKHR swapchain = 0;
    VK_CHECK(vkCreateSwapchainKHR(device, &createInfo, 0, &swapchain));

    return swapchain;
}



void createSwapchain(Swapchain& result, VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface, uint32_t familyIndex, 
    VkFormat format, uint32_t width, uint32_t height, VkRenderPass renderPass, VkSwapchainKHR oldSwapchain ){

    VkSurfaceCapabilitiesKHR surfaceCaps;
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCaps));

    VkSwapchainKHR swapchain = createSwapchain(physicalDevice, device, surface, surfaceCaps, familyIndex, format, width, height, oldSwapchain);
    // VkSwapchainKHR swapchain = createSwapchain(physicalDevice, device, surface, surfaceCaps, familyIndex, format, surfaceCaps.currentExtent.width, surfaceCaps.currentExtent.height, oldSwapchain);

    uint32_t imageCount = 0;
    VK_CHECK(vkGetSwapchainImagesKHR(device, swapchain, &imageCount, 0));                      // TODO : workaround for Intel Graphics driver bug, clean this up
    std::vector<VkImage> images(imageCount);                                          // SHORTCUT
    VK_CHECK(vkGetSwapchainImagesKHR(device, swapchain, &imageCount, images.data())); // should be called before vkAcquireNextImageKHR

    result.swapchain = swapchain;
    result.images = images;
    result.width = width;
    result.height = height;
    result.imageCount = imageCount;

}

void destroySwapchain(VkDevice device, const Swapchain& swapchain){
    vkDestroySwapchainKHR(device, swapchain.swapchain, 0);
}

bool resizeSwapchainIfNecessary(Swapchain& result, VkDevice device, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t familyIndex, VkFormat format, VkRenderPass renderPass) {

    VkSurfaceCapabilitiesKHR surfaceCaps;
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCaps));

    uint32_t newWidth = surfaceCaps.currentExtent.width;
    uint32_t newHeight = surfaceCaps.currentExtent.height;

    if (result.width == newWidth && result.height == newHeight)
        return false;

    Swapchain old = result;

    createSwapchain(result, physicalDevice, device, surface, familyIndex, format, newWidth, newHeight, renderPass, old.swapchain);

    VK_CHECK(vkDeviceWaitIdle(device));

    destroySwapchain(device, old);

    return true;
}
