#include <stdio.h>

#include <algorithm>

#include <GLFW/glfw3.h>

#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>

#define VK_USE_PLATFORM_XLIB_KHR
// #include <vulkan/vulkan.h>
#define VOLK_IMPLEMENTATION
#include "common.h"

#include "shaders.h"

#include <X11/Xlib.h>

#include "../include/fast_obj/fast_obj.h"

#include "../meshoptimizer/src/meshoptimizer.h"

#define _DEBUG 0

#define RTX 0



bool rtxEnabled = false;

// for validation layer
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
FILE *VkLogFile = stderr;
VkDebugUtilsMessengerEXT g_DebugUtilsMessenger = VK_NULL_HANDLE;

const char *GetDebugSeverityStr(VkDebugUtilsMessageSeverityFlagBitsEXT severity)
{
    switch (severity)
    {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
        return "VERBOSE";
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
        return "INFO";
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
        return "WARNING";
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
        return "ERROR";
    default:
        return "UNKNOWN";
    }
}

const char *GetDebugType(VkDebugUtilsMessageTypeFlagsEXT type)
{
    switch (type)
    {
    case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT:
        return "GENERAL";
    case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT:
        return "VALIDATION";
    case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT:
        return "PERFORMANCE";
    case VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT:
        return "DEVICE_ADDRESS_BINDING";
    default:
        return "UNKNOWN";
    }
}
// debug callback function
static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                                    VkDebugUtilsMessageTypeFlagsEXT type,
                                                    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                                                    void *pUserData)
{
    fprintf(VkLogFile, "Debug Callback : %s\n", pCallbackData->pMessage);
    fprintf(VkLogFile, "\tSeverity : %s\n", GetDebugSeverityStr(severity));
    fprintf(VkLogFile, "\tType : %s\n", GetDebugType(type));
    fprintf(VkLogFile, "\tObjects : ");

    for (uint32_t i = 0; i < pCallbackData->objectCount; i++)
    {
        if (severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT || severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        {
            fprintf(VkLogFile, "\t%llx \n", pCallbackData->pObjects[i].objectHandle);
            fprintf(VkLogFile, "\tObject %d : %s\n", i, pCallbackData->pObjects[i].pObjectName ? pCallbackData->pObjects[i].pObjectName : "No Name");
        }
    }

    return VK_FALSE; // return false to continue with the Vulkan operation
}

void createDebugCallback(VkInstance instance)
{
    VkDebugUtilsMessengerCreateInfoEXT messengerCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .pNext = nullptr,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = &DebugCallback,
        .pUserData = nullptr};

    PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessenger = VK_NULL_HANDLE;
    vkCreateDebugUtilsMessenger = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (!vkCreateDebugUtilsMessenger)
    {
        printf("Failed to get address for vkCreateDebugUtilsMessengerEXT\n");
        fprintf(VkLogFile, "FROM LOGS : Failed to get address for vkCreateDebugUtilsMessengerEXT\n");
        return;
    }

    if (vkCreateDebugUtilsMessenger(instance, &messengerCreateInfo, nullptr, &g_DebugUtilsMessenger) != VK_SUCCESS)
    {
        printf("Failed to create debug utils messenger\n");
        return;
    }

    printf("Debug callback created successfully\n");
}

void destroyDebugUtilsMessenger(VkInstance instance)
{
    PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessenger = VK_NULL_HANDLE;
    vkDestroyDebugUtilsMessenger = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (!vkDestroyDebugUtilsMessenger)
    {
        printf("Failed to get address for vkCreateDebugUtilsMessengerEXT\n");
        return;
    }

    vkDestroyDebugUtilsMessenger(instance, g_DebugUtilsMessenger, nullptr);
    printf("\nDebug callback destroyed successfully\n");
}

//////////////////////////////////////////////////////////////////////////////////////////
VkInstance createInstance(){
    // This is SHORTCUT, in real prod application we should first check if 1.1 is available using  vkEnumerateInstanceVersion
    VkApplicationInfo appInfo = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    appInfo.apiVersion = VK_API_VERSION_1_1;

    VkInstanceCreateInfo createInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    createInfo.pApplicationInfo = &appInfo;

#ifdef _DEBUG
    // validation layers
    const char *debugLayers[] = {
        "VK_LAYER_KHRONOS_validation"};

    createInfo.ppEnabledLayerNames = debugLayers;
    createInfo.enabledLayerCount = sizeof(debugLayers) / sizeof(debugLayers[0]);

#endif

    const char *extensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
#if defined(_WIN32)
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#else
        VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
#endif
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
        // VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
    };
    createInfo.ppEnabledExtensionNames = extensions;
    createInfo.enabledExtensionCount = sizeof(extensions) / sizeof(extensions[0]);

    VkInstance instance = 0;
    VK_CHECK(vkCreateInstance(&createInfo, 0, &instance));

    return instance;
}

uint32_t getGraphicsFamilyIndex(VkPhysicalDevice physicalDevice){
    uint32_t queueFamilyPropertyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropertyCount, 0);

    std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyPropertyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropertyCount, queueFamilyProperties.data());

    for (uint32_t i = 0; i < queueFamilyPropertyCount; ++i)
    {
        if (queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            return i;
        }
    }

    return VK_QUEUE_FAMILY_IGNORED;
}

bool supportsPresentation(VkPhysicalDevice physicalDevice, GLFWwindow* window, uint32_t familyIndex){

    Display* dpy = glfwGetX11Display();
    Window x11_window = glfwGetX11Window(window);
    VisualID visualID;

    XWindowAttributes attrs;
    if (XGetWindowAttributes(dpy, x11_window, &attrs)){
        visualID = XVisualIDFromVisual(attrs.visual);
    }

    return vkGetPhysicalDeviceXlibPresentationSupportKHR(physicalDevice, familyIndex, dpy, visualID);
}

VkPhysicalDevice pickPhysicalDevice(VkPhysicalDevice *physicalDevices, uint32_t physicalDeviceCount, GLFWwindow* window){

    VkPhysicalDevice discrete = 0;
    VkPhysicalDevice fallback = 0;

    for (uint32_t i = 0; i < physicalDeviceCount; ++i)
    {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(physicalDevices[i], &props);

        // printf("GPU %d : %s\n", i, props.deviceName);

        uint32_t familyIndex = getGraphicsFamilyIndex(physicalDevices[i]);
        if (familyIndex == VK_QUEUE_FAMILY_IGNORED){
            continue;
        }
        if (!supportsPresentation(physicalDevices[i], window, familyIndex)){
            continue;
        }

        if (props.apiVersion < VK_API_VERSION_1_1)
            continue;

        if (!discrete && props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        {
            // printf("discrete GPU : %s\n", props.deviceName);
            discrete = physicalDevices[i];
        }
        if (!fallback){
            // printf("fallback GPU : %s\n", props.deviceName);
            fallback = physicalDevices[i];
        }
    }

    VkPhysicalDevice result = discrete ? discrete : fallback;
    if (result){
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(result, &props);

        printf("Selected GPU : %s\n", props.deviceName);
    }
    else{
        printf("ERROR : NO GPU found\n");
    }

    return result;
}


VkDevice createDevice(VkInstance instance, VkPhysicalDevice physicalDevice, uint32_t familyIndex, bool rtxSupported){

    // *familyIndex = 0; // SHORTCUT : this needs to be compute from queue property // TODO : this produces validation error

    float queueProperties[] = {1.0f};

    VkDeviceQueueCreateInfo queueInfo = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queueInfo.queueFamilyIndex = familyIndex; // SHORTCUT : should be get from queueProperties // TODO: this gives alidation error
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = queueProperties;

    std::vector<const char*> extensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
        VK_KHR_16BIT_STORAGE_EXTENSION_NAME,
        VK_KHR_8BIT_STORAGE_EXTENSION_NAME,
    };
    if (rtxSupported){
        extensions.push_back(VK_NV_MESH_SHADER_EXTENSION_NAME);
    }

    // Adding features 2 for enabling 8 bit data type access in shaders
    VkPhysicalDeviceFeatures2 features = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    features.features.vertexPipelineStoresAndAtomics = true;

    VkPhysicalDevice16BitStorageFeatures feature16 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES};
    feature16.storageBuffer16BitAccess = true;
    feature16.uniformAndStorageBuffer16BitAccess = VK_TRUE;

    VkPhysicalDevice8BitStorageFeaturesKHR features8 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES_KHR};
    features8.storageBuffer8BitAccess = true;
    features8.uniformAndStorageBuffer8BitAccess = true; // TODO : this might be glslang bug , this is solving validation error from
                                                        // Day 4 code, but is this necessary and if yes, why ?

    // this will only be used when RTX supports mesh shader
    VkPhysicalDeviceMeshShaderFeaturesNV featureMesh = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_NV};
    featureMesh.meshShader = true;

    VkDeviceCreateInfo pDeviceCreateInfo = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    pDeviceCreateInfo.queueCreateInfoCount = 1;
    pDeviceCreateInfo.pQueueCreateInfos = &queueInfo;
    pDeviceCreateInfo.ppEnabledExtensionNames = extensions.data();
    pDeviceCreateInfo.enabledExtensionCount = uint32_t(extensions.size());
    // pDeviceCreateInfo.pEnabledFeatures = &features;
    pDeviceCreateInfo.pNext = &features;
    features.pNext = &feature16;
    feature16.pNext = &features8;

    if (rtxSupported)
        features8.pNext = &featureMesh;

    VkDevice device = 0;
    VK_CHECK(vkCreateDevice(physicalDevice, &pDeviceCreateInfo, 0, &device));

    return device;
}

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

VkSwapchainKHR createSwapchain(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface, VkSurfaceCapabilitiesKHR surfaceCaps, 
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
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.queueFamilyIndexCount = 1;
    createInfo.pQueueFamilyIndices = &familyIndex;
    // createInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    createInfo.preTransform = surfaceCaps.currentTransform;
    createInfo.compositeAlpha = surfaceComposite;
    createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    createInfo.oldSwapchain = oldSwapchain;

    VkSwapchainKHR swapchain = 0;
    VK_CHECK(vkCreateSwapchainKHR(device, &createInfo, 0, &swapchain));

    return swapchain;
}

VkSemaphore createSemaphore(VkDevice device){
    VkSemaphoreCreateInfo createInfo = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkSemaphore semaphore = 0;
    vkCreateSemaphore(device, &createInfo, 0, &semaphore);

    return semaphore;
}

VkCommandPool createCommandPool(VkDevice device, uint32_t familyIndex){

    VkCommandPoolCreateInfo createInfo = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    createInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    createInfo.queueFamilyIndex = familyIndex;

    VkCommandPool commandPool = 0;
    VK_CHECK(vkCreateCommandPool(device, &createInfo, 0, &commandPool));

    return commandPool;
}

/* Day 2 */
VkRenderPass createRenderPass(VkDevice device, VkFormat format)
{

    VkAttachmentDescription attachments[1] = {};
    attachments[0].format = format;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachments = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subPass = {};
    subPass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subPass.colorAttachmentCount = 1;
    subPass.pColorAttachments = &colorAttachments;

    VkRenderPassCreateInfo createInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    createInfo.attachmentCount = sizeof(attachments) / sizeof(attachments[0]);
    createInfo.pAttachments = attachments;
    createInfo.subpassCount = 1;
    createInfo.pSubpasses = &subPass;

    VkRenderPass renderPass = 0;
    VK_CHECK(vkCreateRenderPass(device, &createInfo, 0, &renderPass));

    return renderPass;
}

VkFramebuffer createFramebuffer(VkDevice device, VkRenderPass renderPass, VkImageView imageView, uint32_t width, uint32_t height){

    VkFramebufferCreateInfo createInfo = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    createInfo.renderPass = renderPass;
    createInfo.attachmentCount = 1;
    createInfo.pAttachments = &imageView;
    createInfo.width = width;
    createInfo.height = height;
    createInfo.layers = 1;

    VkFramebuffer framebuffer = 0;
    vkCreateFramebuffer(device, &createInfo, 0, &framebuffer);

    return framebuffer;
}

VkImageView createImageView(VkDevice device, VkImage swapchainImage, VkFormat format){

    VkImageViewCreateInfo createInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    createInfo.image = swapchainImage;
    createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    createInfo.format = format;
    createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    createInfo.subresourceRange.levelCount = 1;
    createInfo.subresourceRange.layerCount = 1;

    VkImageView view = 0;
    VK_CHECK(vkCreateImageView(device, &createInfo, 0, &view));

    return view;
}


VkImageMemoryBarrier imageBarrier(VkImage image, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, VkImageLayout oldLayout, VkImageLayout newLayout)
{

    VkImageMemoryBarrier result = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};

    result.srcAccessMask = srcAccessMask;
    result.dstAccessMask = dstAccessMask;
    result.oldLayout = oldLayout;
    result.newLayout = newLayout;
    result.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    result.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    result.image = image;
    result.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    result.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
    result.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;

    return result;
}


VkBufferMemoryBarrier bufferBarrier(VkBuffer buffer, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask) {

    VkBufferMemoryBarrier result = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    result.srcAccessMask = srcAccessMask;
    result.dstAccessMask = dstAccessMask;
    result.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    result.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    result.buffer = buffer;
    result.offset = 0;
    result.size = VK_WHOLE_SIZE;

    return result;
}
// Day 3
struct Swapchain{
    VkSwapchainKHR swapchain;

    std::vector<VkImage> images;
    std::vector<VkImageView> imageViews;
    std::vector<VkFramebuffer> framebuffers;

    uint32_t width, height;
    uint32_t imageCount;
};

void createSwapchain(Swapchain& result, VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface, uint32_t familyIndex, 
    VkFormat format, uint32_t width, uint32_t height, VkRenderPass renderPass, VkSwapchainKHR oldSwapchain = 0){

    VkSurfaceCapabilitiesKHR surfaceCaps;
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCaps));

    VkSwapchainKHR swapchain = createSwapchain(physicalDevice, device, surface, surfaceCaps, familyIndex, format, width, height, oldSwapchain);
    // VkSwapchainKHR swapchain = createSwapchain(physicalDevice, device, surface, surfaceCaps, familyIndex, format, surfaceCaps.currentExtent.width, surfaceCaps.currentExtent.height, oldSwapchain);

    uint32_t imageCount = 0;
    VK_CHECK(vkGetSwapchainImagesKHR(device, swapchain, &imageCount, 0));                      // TODO : workaround for Intel Graphics driver bug, clean this up
    std::vector<VkImage> images(imageCount);                                          // SHORTCUT
    VK_CHECK(vkGetSwapchainImagesKHR(device, swapchain, &imageCount, images.data())); // should be called before vkAcquireNextImageKHR

    // create swapchain image views
    std::vector<VkImageView> imageViews(imageCount);
    for (uint32_t i = 0; i < imageCount; ++i)
    {
        imageViews[i] = createImageView(device, images[i], format);
        assert(imageViews[i]);
    }

    // create framebuffers
    std::vector<VkFramebuffer> framebuffers(imageCount);
    for (uint32_t i = 0; i < imageCount; ++i)
    {
        framebuffers[i] = createFramebuffer(device, renderPass, imageViews[i], width, height);
        // framebuffers[i] = createFramebuffer(device, renderPass, imageViews[i], surfaceCaps.currentExtent.width, surfaceCaps.currentExtent.height);
        assert(framebuffers[i]);
    }

    result.swapchain = swapchain;
    result.images = images;
    result.imageViews = imageViews;
    result.framebuffers = framebuffers;
    result.width = width;
    // result.width = surfaceCaps.currentExtent.width;
    result.height = height;
    // result.height = surfaceCaps.currentExtent.height;
    result.imageCount = imageCount;

}

void destroySwapchain(VkDevice device, const Swapchain& swapchain){
    for (uint32_t i = 0; i < swapchain.imageCount; ++i)
    {
        vkDestroyFramebuffer(device, swapchain.framebuffers[i], 0);
    }

    for (uint32_t i = 0; i < swapchain.imageCount; ++i)
    {
        vkDestroyImageView(device, swapchain.imageViews[i], 0);
    }

    vkDestroySwapchainKHR(device, swapchain.swapchain   , 0);
}

void resizeSwapchain(Swapchain& result, VkDevice device, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t familyIndex, VkFormat format, VkRenderPass renderPass) {

    VkSurfaceCapabilitiesKHR surfaceCaps;
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCaps));

    uint32_t newWidth = surfaceCaps.currentExtent.width;
    uint32_t newHeight = surfaceCaps.currentExtent.height;

    if (result.width == newWidth && result.height == newHeight)
        return;

    Swapchain old = result;

    createSwapchain(result, physicalDevice, device, surface, familyIndex, format, newWidth, newHeight, renderPass, old.swapchain);

    VK_CHECK(vkDeviceWaitIdle(device));

    destroySwapchain(device, old);
}

VkQueryPool createQueryPool(VkDevice device, uint32_t queryCount){

    VkQueryPoolCreateInfo createInfo = {VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
    createInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
    createInfo.queryCount = queryCount;

    VkQueryPool queryPool = 0;
    VK_CHECK(vkCreateQueryPool(device, &createInfo, 0, &queryPool));

    return queryPool;
}

// Day 4 -> Mesh loading

// For mesh shader
struct Meshlet{
    uint32_t vertices[64];
    uint8_t indices[126 * 3];   // up to 126 triangles
    uint8_t triangleCount;
    uint8_t vertexCount;
};

struct Vertex{
    uint16_t vx, vy, vz, vw;
    uint8_t nx, ny, nz, nw;
    uint16_t tu, tv;
};


struct Mesh{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<Meshlet> meshlets;
};

bool loadMesh(Mesh& result, const char* path){
    fastObjMesh* objMesh = fast_obj_read(path);;
	if (!objMesh){
        printf("Can not found the specified OBJ file\n");
		return false;
	}

	// size_t index_count = file.f_size / 3;
	
	std::vector<Vertex> vertices;
    vertices.reserve(objMesh->index_count);
	// std::vector<uint32_t> indices(objMesh->index_count);

    uint32_t indexOffset = 0;   // running offset into mesh->indices[]
	
    // Loop over all faces
    for (unsigned int f = 0; f < objMesh->face_count; f++) {
        // Get the number of vertices in this face (usually 3 for triangles)
        unsigned int vertex_count = objMesh->face_vertices[f];
        
        // Get the starting index for this face in the indices array
        // unsigned int index_offset = objMesh->face_offsets[f];

        // Loop over vertices in the face
        for (unsigned int v = 1; v + 1 < vertex_count; ++v) {

            // The three corners of this triangle within the face
            uint32_t corners[3] = { 0, v, v + 1 };

            for (uint32_t c : corners){
                fastObjIndex idx = objMesh->indices[indexOffset + c];

                Vertex vtx{};

                // ── Position ──────────────────────────────────────────────
                // fast-obj stores positions starting at slot 1 (slot 0 is a
                // dummy zero-initialised entry).  Each position is 3 floats.
                if (idx.p > 0) {
                    vtx.vx = meshopt_quantizeHalf(objMesh->positions[3 * idx.p + 0]);
                    vtx.vy = meshopt_quantizeHalf(objMesh->positions[3 * idx.p + 1]);
                    vtx.vz = meshopt_quantizeHalf(objMesh->positions[3 * idx.p + 2]);
                }

                // ── Normal ────────────────────────────────────────────────
                if (idx.n > 0) {
                    float nx = objMesh->normals[3 * idx.n + 0];
                    float ny = objMesh->normals[3 * idx.n + 1];
                    float nz = objMesh->normals[3 * idx.n + 2];
                    // TODO : fix rounding
                    vtx.nx = uint8_t(nx * 127.f + 127.f);
                    vtx.ny = uint8_t(ny * 127.f + 127.f);
                    vtx.nz = uint8_t(nz * 127.f + 127.f);
                }

                // ── Texture coordinate ────────────────────────────────────
                // fast-obj stores UVs as 2 floats per entry.
                // OBJ convention: V=0 is bottom; Vulkan: V=0 is top.
                // Flip V so textures appear correctly.
                if (idx.t > 0) {
                    vtx.tu =       meshopt_quantizeHalf(objMesh->texcoords[2 * idx.t + 0]);
                    vtx.tv = meshopt_quantizeHalf(1.0f - objMesh->texcoords[2 * idx.t + 1]); // flip V
                }

                // printf("x = %f, y = %f, z = %f\n", vtx.nx, vtx.ny, vtx.nz);

                vertices.push_back(vtx);
            }
            
            // Use px, py, pz, nx, ny, nz, u, v_coord to build your vertex buffer
        }
        indexOffset += vertex_count;   // advance past all corners of this face
    }
	/*for (size_t i=0; i < index_count; ++i) {
		Vertex& v = vertices[i];

		int vi = file.f[i * 3 + 0];
		int vti = file.f[i * 3 + 1];
		int vni = file.f[i * 3 + 2];

		v.vx = file.v[vi * 3 + 0];
		v.vy = file.v[vi * 3 + 1];
		v.vz = file.v[vi * 3 + 2];
		v.nx = vni < 0 ? 0.f : file.vn[vni * 3 + 0];
		v.ny = vni < 0 ? 0.f : file.vn[vni * 3 + 1];
		v.nz = vni < 0 ? 1.f : file.vn[vni * 3 + 2];
		v.tu = vti < 0 ? 0.f : file.vt[vti * 3 + 0];
		v.tv = vti < 0 ? 0.f : file.vt[vti * 3 + 1];
	}*/

	// if (0){
	// 	result.vertices = vertices;
	// 	result.indices.resize(objMesh->index_count);

	// 	for (size_t i=0; i < objMesh->index_count; ++i){
	// 		result.indices[i] = (uint32_t)i;
	// 	}
	// } else {
		std::vector<uint32_t> remap(objMesh->index_count);
		size_t vertex_count = meshopt_generateVertexRemap(remap.data(), 0, objMesh->index_count, vertices.data(), objMesh->index_count, sizeof(Vertex));

		result.vertices.resize(vertex_count);
		result.indices.resize(objMesh->index_count);

		meshopt_remapVertexBuffer(result.vertices.data(), vertices.data(), objMesh->index_count, sizeof(Vertex), remap.data());
		meshopt_remapIndexBuffer(result.indices.data(), 0, objMesh->index_count, remap.data());

		// TODO : optimize the mesh for more efficient GPU rendering
	// }

    if (1){
        meshopt_optimizeVertexCache(result.indices.data(), result.indices.data(), objMesh->index_count, vertex_count);
        meshopt_optimizeVertexFetch(result.vertices.data(), result.indices.data(), objMesh->index_count, result.vertices.data(), vertex_count, sizeof(Vertex));
    }


    fast_obj_destroy(objMesh);

    return true;
}

void buildMeshlets(Mesh& mesh){
    Meshlet meshlet = {};

    std::vector<uint8_t> meshletVertices(mesh.vertices.size(), 0xff);

    for (size_t i=0; i < mesh.indices.size(); i += 3){
        unsigned int a = mesh.indices[i + 0];
        unsigned int b = mesh.indices[i + 1];
        unsigned int c = mesh.indices[i + 2];

        uint8_t& av = meshletVertices[a];
        uint8_t& bv = meshletVertices[b];
        uint8_t& cv = meshletVertices[c];

        if (meshlet.vertexCount + (av == 0xff) + (bv == 0xff) + (cv == 0xff) > 64 || meshlet.triangleCount >= 126){
            mesh.meshlets.push_back(meshlet);
            meshlet = {};
            memset(meshletVertices.data(), 0xff, mesh.vertices.size());
        }

        if (av == 0xff){
            av = meshlet.vertexCount;
            meshlet.vertices[meshlet.vertexCount++] = a;
        }

        if (bv == 0xff){
            bv = meshlet.vertexCount;
            meshlet.vertices[meshlet.vertexCount++] = b;
        }
        if (cv == 0xff){
            cv = meshlet.vertexCount;
            meshlet.vertices[meshlet.vertexCount++] = c;
        }

        meshlet.indices[meshlet.triangleCount * 3 + 0] = av;
        meshlet.indices[meshlet.triangleCount * 3 + 1] = bv;
        meshlet.indices[meshlet.triangleCount * 3 + 2] = cv;
        meshlet.triangleCount++;
    }

    if (meshlet.triangleCount)
        mesh.meshlets.push_back(meshlet);
}

struct Buffer{
    VkBuffer buffer;
    VkDeviceMemory memory;
    void* data;
    size_t size;
};

uint32_t selectMemoryType(const VkPhysicalDeviceMemoryProperties& memoryProperties, uint32_t memoryTypeBits, VkMemoryPropertyFlags flags){
    for (uint32_t i=0; i < memoryProperties.memoryTypeCount; ++i){
        if ((memoryTypeBits & (1 << i)) != 0 && (memoryProperties.memoryTypes[i].propertyFlags & flags) == flags){
            return i;
        }
    }

    assert(!"No compatible memory type found");
    return ~0u;
}



void createBuffer(Buffer& result, VkDevice device, const VkPhysicalDeviceMemoryProperties& memoryProperties, size_t size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryFlags){
    // create Vk buffer container without memory
    VkBufferCreateInfo createInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    createInfo.size = size;
    createInfo.usage = usage;

    VkBuffer buffer = 0;
    vkCreateBuffer(device, &createInfo, 0, &buffer);

    // ask memory requirements for the above created buffer to the Vulkan
    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memoryRequirements);

    uint32_t memoryTypeIndex = selectMemoryType(memoryProperties, memoryRequirements.memoryTypeBits, memoryFlags);
    assert(memoryTypeIndex != ~0u);

    VkMemoryAllocateInfo allocateInfo = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocateInfo.allocationSize = memoryRequirements.size;
    allocateInfo.memoryTypeIndex = memoryTypeIndex;

    VkDeviceMemory deviceMemory = 0;
    VK_CHECK(vkAllocateMemory(device, &allocateInfo, 0, &deviceMemory));

    VK_CHECK(vkBindBufferMemory(device, buffer, deviceMemory, 0));

    void* data = 0;
    if (memoryFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
        VK_CHECK(vkMapMemory(device, deviceMemory, 0, size, 0, &data));

    result.buffer = buffer;
    result.data = data;
    result.memory = deviceMemory;
    result.size = size;
}

void uploadBuffer(VkDevice device, VkCommandPool commandPool, VkCommandBuffer commandBuffer, VkQueue queue, const Buffer& buffer, const Buffer& scratch, const void* data, size_t size){
    
    // TODO : this function is submitting a commandbuffer and waiting for device idle for each buffer upload; this is obviously suboptimal and we'd need to batch this later 
    assert(scratch.data);
    assert(scratch.size >= size);
    memcpy(scratch.data, data, size);

    VK_CHECK(vkResetCommandPool(device, commandPool, 0));

    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

    VkBufferCopy region = {0, 0, VkDeviceSize(size)};
    vkCmdCopyBuffer(commandBuffer, scratch.buffer, buffer.buffer, 1, &region);

    VkBufferMemoryBarrier copyBarrier = bufferBarrier(buffer.buffer, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 1, &copyBarrier, 0, 0);

    VK_CHECK(vkEndCommandBuffer(commandBuffer));

    VkSubmitInfo submitInfo = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));

    VK_CHECK(vkDeviceWaitIdle(device));
}

void destroyBuffer(const Buffer& buffer, VkDevice device){
    vkFreeMemory(device, buffer.memory, 0);
    vkDestroyBuffer(device, buffer.buffer, 0);
}


void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods){
    if (action == GLFW_PRESS){
        if (key == GLFW_KEY_R){
            rtxEnabled = !rtxEnabled;
        }
    }
}

int main(int argc, const char** argv)
{
	if (argc < 2){
		printf("Usage : %s [mesh]\n", argv[0]);
		return 1;
	}

    VkLogFile = fopen("validation.log", "wa");
    assert(VkLogFile);

    int ret = glfwInit();
    assert(ret);

    VK_CHECK(volkInitialize());

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    VkInstance instance = createInstance();
    assert(instance);

    volkLoadInstance(instance);

    // enable validation layer output
    createDebugCallback(instance);

    // create window
    GLFWwindow *window = glfwCreateWindow(1024, 768, "Following Niagara", 0, 0);
    assert(window);

    glfwSetKeyCallback(window, keyCallback);

    // physical device selection
    VkPhysicalDevice physicalDevices[16];
    uint32_t physicalDeviceCount = sizeof(physicalDevices) / sizeof(physicalDevices[0]);
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices));

    VkPhysicalDevice physicalDevice = pickPhysicalDevice(physicalDevices, physicalDeviceCount, window);
    assert(physicalDevice);

    uint32_t extensionCount = 0;
    VK_CHECK(vkEnumerateDeviceExtensionProperties(physicalDevice, 0, &extensionCount, 0));

    std::vector<VkExtensionProperties> extensions(extensionCount);
    VK_CHECK(vkEnumerateDeviceExtensionProperties(physicalDevice, 0, &extensionCount, extensions.data()));

    bool rtxSupported = false;
    for (auto& ext : extensions){
        if (strcmp(ext.extensionName, "VK_NV_mesh_shader") == 0){
            rtxSupported = true;
            break;
        }
    }

    rtxEnabled = rtxSupported;

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(physicalDevice, &props);
    assert(props.limits.timestampComputeAndGraphics);

    // create logical device
    uint32_t familyIndex = getGraphicsFamilyIndex(physicalDevice);
    assert(familyIndex != VK_QUEUE_FAMILY_IGNORED);
    VkDevice device = createDevice(instance, physicalDevice, familyIndex, rtxSupported);
    assert(device);

    // creating surface
    VkSurfaceKHR surface = createSurface(instance, window);
    assert(surface);

    int winWidth = 0, winHeight = 0;
    glfwGetWindowSize(window, &winWidth, &winHeight);

    // swapchain format
    VkFormat swapchainFormat = getSwapchainFormat(physicalDevice, surface);

    VkSurfaceCapabilitiesKHR surfaceCaps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCaps);

    // create semaphore
    VkSemaphore acquireSemaphore = createSemaphore(device);
    assert(acquireSemaphore);

    VkSemaphore releaseSemaphore = createSemaphore(device);
    assert(releaseSemaphore);

    // get queue
    VkQueue queue = 0;
    vkGetDeviceQueue(device, familyIndex, 0, &queue);

    // create renderpass
    VkRenderPass renderPass = createRenderPass(device, swapchainFormat);
    assert(renderPass);

    // shader module
    VkShaderModule meshletVS = 0;
    if (rtxSupported){
        meshletVS = loadShader(device, "src/shaders/meshlet.mesh.spv");
        assert(meshletVS);
    } 

    VkShaderModule meshVS = loadShader(device, "src/shaders/mesh.vert.spv");
    assert(meshVS);

    VkShaderModule meshFS = loadShader(device, "src/shaders/mesh.frag.spv");
    assert(meshFS);

    // graphics pipeline layout
    VkPipelineLayout meshLayout = createPipelineLayout(device, /* rtxEnables*/ false);
    assert(meshLayout);

    // create descriptor update template
    VkDescriptorUpdateTemplate meshUpdateTemplate = createUpdateTemplate(device, VK_PIPELINE_BIND_POINT_GRAPHICS, meshLayout, /*rtxEnabled*/ false );
    assert(meshUpdateTemplate);

    VkPipelineLayout meshLayoutRTX = 0;
    VkDescriptorUpdateTemplate meshUpdateTemplateRTX = 0;
    if (rtxSupported){
         meshLayoutRTX = createPipelineLayout(device, /* rtxEnables*/ true);
        assert(meshLayoutRTX);

        meshUpdateTemplateRTX = createUpdateTemplate(device, VK_PIPELINE_BIND_POINT_GRAPHICS, meshLayoutRTX, /*rtxEnabled*/ true );
        assert(meshUpdateTemplateRTX);
    }

    // create graphics pipeline
    VkPipelineCache pipelineCache = 0;
    VkPipeline meshPipeline = createGraphicsPipeline(device, pipelineCache, renderPass, meshVS, meshFS, meshLayout, /* rtxSupported*/ false);
    assert(meshPipeline);

    VkPipeline meshPipelineRTX = 0;
    if (rtxSupported){
        meshPipelineRTX = createGraphicsPipeline(device, pipelineCache, renderPass, meshletVS, meshFS, meshLayoutRTX, /* rtxSupported*/ true);
        assert(meshPipelineRTX);
    }

    // swapchain creation code
    Swapchain swapchain;
    createSwapchain(swapchain, physicalDevice, device, surface, familyIndex, swapchainFormat, winWidth, winHeight, renderPass);

    VkQueryPool queryPool = createQueryPool(device, 128);
    assert(queryPool);

    // commandpool
    VkCommandPool commandPool = createCommandPool(device, familyIndex);
    assert(commandPool);

    VkCommandBufferAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffers = 0;
    VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, &commandBuffers));

    VkPhysicalDeviceMemoryProperties memoryProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

	// Mesh loading code
	Mesh mesh;
	bool rcm = loadMesh(mesh, argv[1]);
    assert(rcm);

    if (rtxSupported){
        buildMeshlets(mesh);
    }

    Buffer scratch = {};
    createBuffer(scratch, device, memoryProperties, 128 * 1024 * 1024, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    Buffer vb = {};
    createBuffer(vb, device, memoryProperties, 128 * 1024 * 1024, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    Buffer ib = {};
    createBuffer(ib, device, memoryProperties, 128 * 1024 * 1024, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    Buffer mb = {};
    if (rtxSupported){
        createBuffer(mb, device, memoryProperties, 128 * 1024 * 1024, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    }

    uploadBuffer(device, commandPool, commandBuffers, queue, vb, scratch, mesh.vertices.data(), mesh.vertices.size() * sizeof(Vertex));
    
    uploadBuffer(device, commandPool, commandBuffers, queue, ib, scratch, mesh.indices.data(), mesh.indices.size() * sizeof(uint32_t));

    if (rtxSupported) {
        uploadBuffer(device, commandPool, commandBuffers, queue, mb, scratch, mesh.meshlets.data(), mesh.meshlets.size() * sizeof(Meshlet));
    }

    fprintf(VkLogFile, "\n==================================================================================================================\n");
    fprintf(VkLogFile, "Begin Rendering");
    fprintf(VkLogFile, "\n==================================================================================================================\n");

    while (!glfwWindowShouldClose(window))
    {
        /* code */
        double frameBegin = glfwGetTime() * 1000.0;

        glfwPollEvents();

        resizeSwapchain(swapchain, device, physicalDevice, surface,  familyIndex, swapchainFormat, renderPass);

        uint32_t imageIndex = 0;
        VkResult acquireResult = vkAcquireNextImageKHR(device, swapchain.swapchain, ~0ull, acquireSemaphore, VK_NULL_HANDLE, &imageIndex);
        if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR)
        {
            // surface changed since last resizeSwapchain() check — skip this frame, rebuild next iteration
            continue;
        }
        assert(acquireResult == VK_SUCCESS || acquireResult == VK_SUBOPTIMAL_KHR);

        VK_CHECK(vkResetCommandPool(device, commandPool, 0));

        VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        VK_CHECK(vkBeginCommandBuffer(commandBuffers, &beginInfo));

        vkCmdResetQueryPool(commandBuffers, queryPool, 0, 128);
        vkCmdWriteTimestamp(commandBuffers, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queryPool, 0);

        VkImageMemoryBarrier renderBeginBarrier = imageBarrier(swapchain.images[imageIndex], 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                                               VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        vkCmdPipelineBarrier(commandBuffers, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                             VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, &renderBeginBarrier);

        VkClearColorValue color = {48.0f / 255.0f, 10.0f / 255.0f, 36.0f / 255.0f, 1};
        VkClearValue clearColor = {color};

        VkRenderPassBeginInfo passBeginInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        passBeginInfo.renderPass = renderPass;
        passBeginInfo.framebuffer = swapchain.framebuffers[imageIndex];
        passBeginInfo.renderArea.extent.width = swapchain.width;
        passBeginInfo.renderArea.extent.height = swapchain.height;
        passBeginInfo.clearValueCount = 1;
        passBeginInfo.pClearValues = &clearColor;

        vkCmdBeginRenderPass(commandBuffers, &passBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport = {0, float(swapchain.height), float(swapchain.width), -float(swapchain.height), 0, 1};
        VkRect2D scissor = {{0, 0}, {uint32_t(swapchain.width), uint32_t(swapchain.height)}};

        vkCmdSetViewport(commandBuffers, 0, 1, &viewport);
        vkCmdSetScissor(commandBuffers, 0, 1, &scissor);

        // draw calls go here
        

        if (rtxEnabled){
            vkCmdBindPipeline(commandBuffers, VK_PIPELINE_BIND_POINT_GRAPHICS, meshPipelineRTX);

            DescriptorInfo descriptors[] = {vb.buffer, mb.buffer};
            vkCmdPushDescriptorSetWithTemplateKHR(commandBuffers, meshUpdateTemplateRTX, meshLayoutRTX, 0, descriptors);
            
            vkCmdDrawMeshTasksNV(commandBuffers, uint32_t(mesh.meshlets.size()), 0);
        }
        else {
            vkCmdBindPipeline(commandBuffers, VK_PIPELINE_BIND_POINT_GRAPHICS, meshPipeline);

            DescriptorInfo descriptors[] = {vb.buffer};
            vkCmdPushDescriptorSetWithTemplateKHR(commandBuffers, meshUpdateTemplate, meshLayout, 0, descriptors);

            vkCmdBindIndexBuffer(commandBuffers, ib.buffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(commandBuffers, uint32_t(mesh.indices.size()), 1, 0, 0, 0);
            
        }
        vkCmdEndRenderPass(commandBuffers);

        VkImageMemoryBarrier renderEndBarrier = imageBarrier(swapchain.images[imageIndex], VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0,
                                                             VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
        vkCmdPipelineBarrier(commandBuffers, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, &renderEndBarrier);

        vkCmdWriteTimestamp(commandBuffers, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queryPool, 1);

        VK_CHECK(vkEndCommandBuffer(commandBuffers));

        VkPipelineStageFlags stageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        VkSubmitInfo submitInfo = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &acquireSemaphore;
        submitInfo.pWaitDstStageMask = &stageMask;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffers;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &releaseSemaphore;

        VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));

        VkPresentInfoKHR presentInfo = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &releaseSemaphore;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &swapchain.swapchain;
        presentInfo.pImageIndices = &imageIndex;

        VkResult presentResult = vkQueuePresentKHR(queue, &presentInfo);
        if (presentResult != VK_ERROR_OUT_OF_DATE_KHR && presentResult != VK_SUBOPTIMAL_KHR)
        {
            VK_CHECK(presentResult);
        }

        VK_CHECK(vkDeviceWaitIdle(device));

        uint64_t queryResult[2];
        vkGetQueryPoolResults(device, queryPool, 0, ARRAYSIZE(queryResult), sizeof(queryResult), queryResult, sizeof(queryResult[0]), VK_QUERY_RESULT_64_BIT);

        double frameGpuBegin = double(queryResult[0]) * props.limits.timestampPeriod * 1e-6;
        double frameGpuEnd = double(queryResult[1]) * props.limits.timestampPeriod * 1e-6;

        
        double frameEnd = glfwGetTime() * 1000.0;
        
        char title[256];
        sprintf(title, "cpu: %.3f ms; gpu: %.3f ms; triangles: %d; meshlets: %d; RTX: %s", (frameEnd - frameBegin) , (frameGpuEnd - frameGpuBegin), int(mesh.indices.size() / 3), mesh.meshlets.size(), rtxEnabled ? "ON" : "OFF");
        glfwSetWindowTitle(window, title);
    }

    fprintf(VkLogFile, "\n==================================================================================================================\n");
    fprintf(VkLogFile, "Begin Uninitializing");
    fprintf(VkLogFile, "\n==================================================================================================================\n");

    VK_CHECK(vkDeviceWaitIdle(device));

    if (rtxSupported) {
        destroyBuffer(mb, device);
    }
    
    destroyBuffer(vb, device);
    destroyBuffer(ib, device);
    
    destroyBuffer(scratch, device);

    glfwDestroyWindow(window);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffers);
    vkDestroyCommandPool(device, commandPool, 0);

    vkDestroyQueryPool(device, queryPool, 0);

    vkQueueWaitIdle(queue);

    destroySwapchain(device, swapchain);

    vkDestroyPipeline(device, meshPipeline, 0);
    vkDestroyDescriptorUpdateTemplate(device, meshUpdateTemplate, 0);
    vkDestroyPipelineLayout(device, meshLayout, 0);
    
    if (rtxSupported) {
        vkDestroyPipeline(device, meshPipelineRTX, 0);
        vkDestroyDescriptorUpdateTemplate(device, meshUpdateTemplateRTX, 0);
        vkDestroyPipelineLayout(device, meshLayoutRTX, 0);
    }

    vkDestroyShaderModule(device, meshFS, 0);
    vkDestroyShaderModule(device, meshVS, 0);

    if (rtxSupported) {
        vkDestroyShaderModule(device, meshletVS, 0);
    }

    vkDestroyRenderPass(device, renderPass, 0);
    vkDestroySemaphore(device, acquireSemaphore, 0);
    vkDestroySemaphore(device, releaseSemaphore, 0);

    vkDestroySurfaceKHR(instance, surface, 0);
    vkDestroyDevice(device, 0);

    destroyDebugUtilsMessenger(instance);

    vkDestroyInstance(instance, 0);

    if (VkLogFile)
    {
        printf("\nLogFile closed \n");
        fclose(VkLogFile);
        VkLogFile = NULL;
    }
    return (0);
}
