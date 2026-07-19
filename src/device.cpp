#define VK_USE_PLATFORM_XLIB_KHR
#include "common.h"

#include "device.h"

#include <stdio.h>

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>

#include <vulkan/vulkan.h>
#include <X11/Xlib.h>

// for validation layer
//////////////////////////////////////////////////////////////////////////////////////////////////////////////

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
    createInfo.enabledExtensionCount = sizeof(extensions) / sizeof(extensions[0]);
    createInfo.ppEnabledExtensionNames = extensions;

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

static bool supportsPresentation(VkPhysicalDevice physicalDevice, GLFWwindow* window, uint32_t familyIndex){

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
        VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME,
    };
    if (rtxSupported){
        extensions.push_back(VK_NV_MESH_SHADER_EXTENSION_NAME);
    }

    // Adding features 2 for enabling 8 bit data type access in shaders
    VkPhysicalDeviceFeatures2 features = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    features.features.vertexPipelineStoresAndAtomics = true;
    features.features.multiDrawIndirect = VK_TRUE;

    VkPhysicalDevice16BitStorageFeatures feature16 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES};
    feature16.storageBuffer16BitAccess = true;
    feature16.uniformAndStorageBuffer16BitAccess = VK_TRUE;

    VkPhysicalDevice8BitStorageFeaturesKHR features8 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES_KHR};
    features8.storageBuffer8BitAccess = true;
    features8.uniformAndStorageBuffer8BitAccess = true; // TODO : this might be glslang bug , this is solving validation error from
                                                        // Day 4 code, but is this necessary and if yes, why ?
                                                        
    // enabling shaderDrawParameter for using gl_DrawIdxxx inside shader
    // VkPhysicalDeviceVulkan11Features features11 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};
    // features11.shaderDrawParameters = VK_TRUE;
    // features11.pNext =

    // this will only be used when RTX supports mesh shader
    VkPhysicalDeviceMeshShaderFeaturesNV featureMesh = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_NV};
    featureMesh.meshShader = true;
    featureMesh.taskShader = true;
    // featureMesh.pNext = &features11;

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
