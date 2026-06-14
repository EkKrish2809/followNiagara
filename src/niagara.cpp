#include <stdio.h>
#include <assert.h>

#include <GLFW/glfw3.h>

#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>

#define VK_USE_PLATFORM_XLIB_KHR
#include <vulkan/vulkan.h>

#include <X11/Xlib.h>

#include <vector>
#include <algorithm>

#define _DEBUG 0

#ifndef ARRAYSIZE
#define ARRAYSIZE(array) (sizeof(array) / sizeof(array[0]))
#endif

// #define VK_USE_PLATFORM_WIN32_KHR
#define VK_CHECK(call)             \
    do                             \
    {                              \
        VkResult res = call;       \
        assert(res == VK_SUCCESS); \
    } while (0)

// for validation layer
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
FILE *VkLogFile = NULL;
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
VkInstance createInstance()
{
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

uint32_t getGraphicsFamilyIndex(VkPhysicalDevice physicalDevice)
{
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

VkPhysicalDevice pickPhysicalDevice(VkPhysicalDevice *physicalDevices, uint32_t physicalDeviceCount, GLFWwindow* window)
{

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


VkDevice createDevice(VkInstance instance, VkPhysicalDevice physicalDevice, uint32_t familyIndex)
{

    // *familyIndex = 0; // SHORTCUT : this needs to be compute from queue property // TODO : this produces validation error

    float queueProperties[] = {1.0f};

    VkDeviceQueueCreateInfo queueInfo = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queueInfo.queueFamilyIndex = familyIndex; // SHORTCUT : should be get from queueProperties // TODO: this gives alidation error
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = queueProperties;

    const char *extensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    VkDeviceCreateInfo pDeviceCreateInfo = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    pDeviceCreateInfo.queueCreateInfoCount = 1;
    pDeviceCreateInfo.pQueueCreateInfos = &queueInfo;
    pDeviceCreateInfo.ppEnabledExtensionNames = extensions;
    pDeviceCreateInfo.enabledExtensionCount = sizeof(extensions) / sizeof(extensions[0]);

    VkDevice device = 0;
    VK_CHECK(vkCreateDevice(physicalDevice, &pDeviceCreateInfo, 0, &device));

    return device;
}

VkSurfaceKHR createSurface(VkInstance instance, GLFWwindow *window)
{
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

VkFormat getSwapchainFormat(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface)
{

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

VkSwapchainKHR createSwapchain(VkDevice device, VkSurfaceKHR surface, VkSurfaceCapabilitiesKHR caps, uint32_t familyIndex, VkFormat format, uint32_t winWidth, uint32_t winHeight)
{

    VkCompositeAlphaFlagBitsKHR surfaceComposite =
        (caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) ? VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR : (caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR) ? VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR
                                                                                                             : (caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR)  ? VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR
                                                                                                                                                                                            : VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;

    VkSwapchainCreateInfoKHR createInfo = {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    createInfo.surface = surface;
    createInfo.minImageCount = std::max(2u, caps.minImageCount);
    createInfo.imageFormat = format;
    createInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    createInfo.imageExtent.width = winWidth;
    createInfo.imageExtent.height = winHeight;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.queueFamilyIndexCount = 1;
    createInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    createInfo.compositeAlpha = surfaceComposite;
    createInfo.pQueueFamilyIndices = &familyIndex;
    createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;

    VkSwapchainKHR swapchain = 0;
    VK_CHECK(vkCreateSwapchainKHR(device, &createInfo, 0, &swapchain));

    return swapchain;
}

VkSemaphore createSemaphore(VkDevice device)
{
    VkSemaphoreCreateInfo createInfo = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkSemaphore semaphore = 0;
    vkCreateSemaphore(device, &createInfo, 0, &semaphore);

    return semaphore;
}

VkCommandPool createCommandPool(VkDevice device, uint32_t familyIndex)
{

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

VkFramebuffer createFramebuffer(VkDevice device, VkRenderPass renderPass, VkImageView imageView, uint32_t width, uint32_t height)
{

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

VkImageView createImageView(VkDevice device, VkImage swapchainImage, VkFormat format)
{

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

VkShaderModule loadShader(VkDevice device, const char *path)
{

    FILE *file = fopen(path, "rb");
    assert(file);

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    assert(length >= 0);
    fseek(file, 0, SEEK_SET);

    char *buffer = new char[length];
    assert(buffer);

    size_t ret = fread(buffer, 1, length, file);
    assert(ret == size_t(length));
    fclose(file);

    VkShaderModuleCreateInfo createInfo = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    createInfo.codeSize = length;
    createInfo.pCode = reinterpret_cast<const uint32_t *>(buffer);

    VkShaderModule shaderModule = 0;
    VK_CHECK(vkCreateShaderModule(device, &createInfo, 0, &shaderModule));

    return shaderModule;
}

VkPipelineLayout createPipelineLayout(VkDevice device)
{

    VkPipelineLayoutCreateInfo createInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    VkPipelineLayout layout = 0;
    VK_CHECK(vkCreatePipelineLayout(device, &createInfo, 0, &layout));

    return layout;
}

VkPipeline createGraphicsPipeline(VkDevice device, VkPipelineCache pipelineCache, VkRenderPass renderPass, VkShaderModule VS, VkShaderModule FS, VkPipelineLayout layout)
{

    VkGraphicsPipelineCreateInfo createInfo = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};

    // shader stages
    VkPipelineShaderStageCreateInfo stages[2] = {};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = VS;
    stages[0].pName = "main";

    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = FS;
    stages[1].pName = "main";

    createInfo.stageCount = sizeof(stages) / sizeof(stages[0]);
    createInfo.pStages = stages;

    VkPipelineVertexInputStateCreateInfo vertexInputState = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    createInfo.pVertexInputState = &vertexInputState;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    createInfo.pInputAssemblyState = &inputAssembly;

    VkPipelineViewportStateCreateInfo viewportStateInfo = {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewportStateInfo.viewportCount = 1;
    viewportStateInfo.scissorCount = 1;
    createInfo.pViewportState = &viewportStateInfo;

    VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    // rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
    // rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
    // rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
    // rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
    // rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    // rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
    // float                                      depthBiasConstantFactor;
    // float                                      depthBiasClamp;
    // float                                      depthBiasSlopeFactor;
    // float                                      lineWidth;
    rasterizationStateCreateInfo.lineWidth = 1.0f;

    createInfo.pRasterizationState = &rasterizationStateCreateInfo;

    VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    createInfo.pMultisampleState = &multisampleStateCreateInfo;

    VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = {VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    createInfo.pDepthStencilState = &depthStencilStateCreateInfo;

    VkPipelineColorBlendAttachmentState colorAttachmentState = {};
    colorAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    colorBlendStateCreateInfo.attachmentCount = 1;
    colorBlendStateCreateInfo.pAttachments = &colorAttachmentState;

    createInfo.pColorBlendState = &colorBlendStateCreateInfo;

    VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

    VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamicStateCreateInfo.pDynamicStates = dynamicStates;
    dynamicStateCreateInfo.dynamicStateCount = sizeof(dynamicStates) / sizeof(dynamicStates[0]);

    createInfo.pDynamicState = &dynamicStateCreateInfo;

    createInfo.layout = layout;
    createInfo.renderPass = renderPass;

    VkPipeline pipeline = 0;
    VK_CHECK(vkCreateGraphicsPipelines(device, pipelineCache, 1, &createInfo, 0, &pipeline));

    return pipeline;
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

int main()
{
    VkLogFile = fopen("validation.log", "wa");
    assert(VkLogFile);

    int ret = glfwInit();
    assert(ret);

    VkInstance instance = createInstance();
    assert(instance);

    // enable validation layer output
    createDebugCallback(instance);

    // create window
    GLFWwindow *window = glfwCreateWindow(1024, 768, "Following Niagara", 0, 0);
    assert(window);

    // physical device selection
    VkPhysicalDevice physicalDevices[16];
    uint32_t physicalDeviceCount = sizeof(physicalDevices) / sizeof(physicalDevices[0]);
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices));

    VkPhysicalDevice physicalDevice = pickPhysicalDevice(physicalDevices, physicalDeviceCount, window);
    assert(physicalDevice);

    // create logical device
    uint32_t familyIndex = getGraphicsFamilyIndex(physicalDevice);
    assert(familyIndex != VK_QUEUE_FAMILY_IGNORED);
    VkDevice device = createDevice(instance, physicalDevice, familyIndex);
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

    // swapchain
    VkSwapchainKHR swapchain = createSwapchain(device, surface, surfaceCaps, familyIndex, swapchainFormat, winWidth, winHeight);
    assert(swapchain);

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
    VkShaderModule triangleVS = loadShader(device, "src/triangle.vert.spv");
    assert(triangleVS);
    VkShaderModule triangleFS = loadShader(device, "src/triangle.frag.spv");
    assert(triangleFS);

    // graphics pipeline layout
    VkPipelineLayout triangleLayout = createPipelineLayout(device);
    // create graphics pipeline
    VkPipelineCache pipelineCache = 0;
    VkPipeline trianglePipeline = createGraphicsPipeline(device, pipelineCache, renderPass, triangleVS, triangleFS, triangleLayout);

    uint32_t swapchainImageCount = 0;
    VK_CHECK(vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, 0));                      // TODO : workaround for Intel Graphics driver bug, clean this up
    std::vector<VkImage> swapchainImages(swapchainImageCount);                                          // SHORTCUT
    VK_CHECK(vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, swapchainImages.data())); // should be called before vkAcquireNextImageKHR

    // create swapchain image views
    std::vector<VkImageView> swapchainImageViews(swapchainImageCount);
    for (uint32_t i = 0; i < swapchainImageCount; ++i)
    {
        swapchainImageViews[i] = createImageView(device, swapchainImages[i], swapchainFormat);
        assert(swapchainImageViews[i]);
    }

    // create framebuffers
    std::vector<VkFramebuffer> swapchainFramebuffers(swapchainImageCount);
    for (uint32_t i = 0; i < swapchainImageCount; ++i)
    {
        swapchainFramebuffers[i] = createFramebuffer(device, renderPass, swapchainImageViews[i], winWidth, winHeight);
    }

    // commandpool
    VkCommandPool commandPool = createCommandPool(device, familyIndex);
    assert(commandPool);

    VkCommandBufferAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffers = 0;
    VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, &commandBuffers));

    fprintf(VkLogFile, "\n==================================================================================================================\n");
    fprintf(VkLogFile, "Begin Rendering");
    fprintf(VkLogFile, "\n==================================================================================================================\n");

    while (!glfwWindowShouldClose(window))
    {
        /* code */
        glfwPollEvents();

        uint32_t imageIndex = 0;
        VK_CHECK(vkAcquireNextImageKHR(device, swapchain, ~0ull, acquireSemaphore, VK_NULL_HANDLE, &imageIndex));

        VK_CHECK(vkResetCommandPool(device, commandPool, 0));

        VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        VK_CHECK(vkBeginCommandBuffer(commandBuffers, &beginInfo));

        VkImageMemoryBarrier renderBeginBarrier = imageBarrier(swapchainImages[imageIndex], 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                                               VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        vkCmdPipelineBarrier(commandBuffers, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                             VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, &renderBeginBarrier);

        VkClearColorValue color = {48.0f / 255.0f, 10.0f / 255.0f, 36.0f / 255.0f, 1};
        VkClearValue clearColor = {color};

        VkRenderPassBeginInfo passBeginInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        passBeginInfo.renderPass = renderPass;
        passBeginInfo.framebuffer = swapchainFramebuffers[imageIndex];
        passBeginInfo.renderArea.extent.width = winWidth;
        passBeginInfo.renderArea.extent.height = winHeight;
        passBeginInfo.clearValueCount = 1;
        passBeginInfo.pClearValues = &clearColor;

        vkCmdBeginRenderPass(commandBuffers, &passBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport = {0, float(winHeight), float(winWidth), -float(winHeight), 0, 1};
        VkRect2D scissor = {{0, 0}, {uint32_t(winWidth), uint32_t(winHeight)}};

        vkCmdSetViewport(commandBuffers, 0, 1, &viewport);
        vkCmdSetScissor(commandBuffers, 0, 1, &scissor);

        // draw calls go here
        vkCmdBindPipeline(commandBuffers, VK_PIPELINE_BIND_POINT_GRAPHICS, trianglePipeline);
        vkCmdDraw(commandBuffers, 3, 1, 0, 0);

        vkCmdEndRenderPass(commandBuffers);

        VkImageMemoryBarrier renderEndBarrier = imageBarrier(swapchainImages[imageIndex], VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0,
                                                             VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
        vkCmdPipelineBarrier(commandBuffers, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, &renderEndBarrier);

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
        presentInfo.pSwapchains = &swapchain;
        presentInfo.pImageIndices = &imageIndex;

        VK_CHECK(vkQueuePresentKHR(queue, &presentInfo));

        VK_CHECK(vkDeviceWaitIdle(device));

        glfwWaitEvents();
    }

    fprintf(VkLogFile, "\n==================================================================================================================\n");
    fprintf(VkLogFile, "Begin Uninitializing");
    fprintf(VkLogFile, "\n==================================================================================================================\n");

    VK_CHECK(vkDeviceWaitIdle(device));

    glfwDestroyWindow(window);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffers);
    vkDestroyCommandPool(device, commandPool, 0);

    vkQueueWaitIdle(queue);
    for (uint32_t i = 0; i < swapchainImageCount; ++i)
    {
        vkDestroyFramebuffer(device, swapchainFramebuffers[i], 0);
    }

    for (uint32_t i = 0; i < swapchainImageCount; ++i)
    {
        vkDestroyImageView(device, swapchainImageViews[i], 0);
    }

    for (uint32_t i = 0; i < swapchainImageCount; ++i)
    {
        // vkDestroyImage(device, swapchainImages[i], 0);
    }

    vkDestroyPipeline(device, trianglePipeline, 0);
    vkDestroyPipelineLayout(device, triangleLayout, 0);
    vkDestroyShaderModule(device, triangleFS, 0);
    vkDestroyShaderModule(device, triangleVS, 0);

    vkDestroyRenderPass(device, renderPass, 0);
    vkDestroySemaphore(device, acquireSemaphore, 0);
    vkDestroySemaphore(device, releaseSemaphore, 0);
    vkDestroySwapchainKHR(device, swapchain, 0);
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
    return 0;
}