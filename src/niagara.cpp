#include <stdio.h>
#include <assert.h>

#include <GLFW/glfw3.h>

#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>

#define VK_USE_PLATFORM_XLIB_KHR
#include <vulkan/vulkan.h>

#include <X11/Xlib.h>

#define _DEBUG 0
// #define VK_USE_PLATFORM_WIN32_KHR 
#define VK_CHECK(call)                \
    do                                \
    {                                 \
        VkResult res = call;       \
        assert(res == VK_SUCCESS); \
    } while (0)


// for validation layer
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
FILE *VkLogFile = stderr;
VkDebugUtilsMessengerEXT g_DebugUtilsMessenger = VK_NULL_HANDLE;

const char* GetDebugSeverityStr(VkDebugUtilsMessageSeverityFlagBitsEXT severity)
{
	switch (severity)
	{
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: return "VERBOSE";
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT: return "INFO";
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: return "WARNING";
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT: return "ERROR";
	default: return "UNKNOWN";
	}
}
const char* GetDebugType(VkDebugUtilsMessageTypeFlagsEXT type)
{
	switch (type)
	{
	case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT: return "GENERAL";
	case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT: return "VALIDATION";
	case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT: return "PERFORMANCE";
	case VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT: return "DEVICE_ADDRESS_BINDING";
	default: return "UNKNOWN";
	}
}
// debug callback function
static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
													VkDebugUtilsMessageTypeFlagsEXT type,
													const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
													void* pUserData)
{
	// fprintf(VkLogFile, "Debug Callback : %s\n", pCallbackData->pMessage);
	// fprintf(VkLogFile, "\tSeverity : %s\n", GetDebugSeverityStr(severity));
	// fprintf(VkLogFile, "\tType : %s\n", GetDebugType(type));
	// fprintf(VkLogFile, "\tObjects ");

	for (uint32_t i=0; i < pCallbackData->objectCount; i++)
	{
		if (/*severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT || */severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
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
		.pUserData = nullptr
	};

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
        "VK_LAYER_KHRONOS_validation"
    };

    createInfo.ppEnabledLayerNames = debugLayers;
    createInfo.enabledLayerCount = sizeof(debugLayers) / sizeof(debugLayers[0]);

#endif

    const char* extensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
    #if defined(_WIN32)
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
    #else
        VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
    #endif
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
    };
    createInfo.ppEnabledExtensionNames = extensions;
    createInfo.enabledExtensionCount = sizeof(extensions) / sizeof(extensions[0]);

    VkInstance instance = 0;
    VK_CHECK(vkCreateInstance(&createInfo, 0, &instance));

    return instance;
}

VkPhysicalDevice pickPhysicalDevice(VkPhysicalDevice* physicalDevices, uint32_t physicalDeviceCount){

    for (uint32_t i = 0; i < physicalDeviceCount; ++i){
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(physicalDevices[i], &props);

        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU){
            printf("Picking discrete GPU %s\n", props.deviceName);
            return physicalDevices[i];
        }
    }

    if (physicalDeviceCount > 0){
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(physicalDevices[0], &props);

        printf("Picking fallback GPU %s\n", props.deviceName);
        return physicalDevices[0];
    }

    printf("No physical device found\n");
    return VK_NULL_HANDLE;
}

VkDevice createDevice(VkInstance instance, VkPhysicalDevice physicalDevice, uint32_t *familyIndex){

    *familyIndex = 0; // SHORTCUT : this needs to be compute from queue property // TODO : this produces validation error

    float queueProperties[] = {1.0f};

    VkDeviceQueueCreateInfo queueInfo = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queueInfo.queueFamilyIndex = *familyIndex; // SHORTCUT : should be get from queueProperties // TODO: this gives alidation error
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = queueProperties;

    const char* extensions[] = {
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

VkSurfaceKHR createSurface(VkInstance instance, GLFWwindow* window){
    #if defined( _WIN32)
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

VkSwapchainKHR createSwapchain(VkDevice device, VkSurfaceKHR surface, uint32_t familyIndex, uint32_t winWidth, uint32_t winHeight){
    VkSwapchainCreateInfoKHR createInfo = {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    createInfo.surface = surface;
    createInfo.minImageCount = 2;
    createInfo.imageFormat = VK_FORMAT_R8G8B8A8_UNORM;
    createInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    createInfo.imageExtent.width = winWidth;
    createInfo.imageExtent.height = winHeight;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.queueFamilyIndexCount = 1;
    createInfo.pQueueFamilyIndices = &familyIndex;
    createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;    
   
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


int main()
{
    int ret = glfwInit();
    assert(ret);

    VkInstance instance = createInstance();
    assert(instance);

    // enable validation layer output
    createDebugCallback(instance);

    // physical device selection
    VkPhysicalDevice physicalDevices[16];
    uint32_t physicalDeviceCount = sizeof(physicalDevices) / sizeof(physicalDevices[0]);
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices));

    VkPhysicalDevice physicalDevice = pickPhysicalDevice(physicalDevices, physicalDeviceCount);
    assert(physicalDevice);

    // create logical device
    uint32_t familyIndex = 0;
    VkDevice device = createDevice(instance, physicalDevice, &familyIndex);
    
    // create window
    GLFWwindow *window = glfwCreateWindow(1024, 768, "Following Niagara", 0, 0);
    assert(window);

    // creating surface
    VkSurfaceKHR surface = createSurface(instance, window);
    assert(surface);

    int winWidth = 0, winHeight = 0;
    glfwGetWindowSize(window, &winWidth, &winHeight);

    // swapchain
    VkSwapchainKHR swapchain = createSwapchain(device, surface, familyIndex, winWidth, winHeight);
    assert(swapchain);

    // create semaphore
    VkSemaphore acquireSemaphore = createSemaphore(device);
    assert(acquireSemaphore);

    VkSemaphore releaseSemaphore = createSemaphore(device);
    assert(releaseSemaphore);

    // get queue
    VkQueue queue = 0;
    vkGetDeviceQueue(device, familyIndex, 0, &queue);

    VkImage swapchainImages[16]; // SHORTCUT
    uint32_t swapchainImageCount = sizeof(swapchainImages) / sizeof(swapchainImages[0]);
    VK_CHECK(vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, swapchainImages));    // should be called before vkAcquireNextImageKHR

    // commandpool
    VkCommandPool commandPool = createCommandPool(device, familyIndex);
    assert(commandPool);

    VkCommandBufferAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffers = 0;
    VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, &commandBuffers));


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

        VkClearColorValue color = {1, 0, 1, 1};
        VkImageSubresourceRange range = {};
        range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        range.levelCount = 1;
        range.layerCount = 1;
        vkCmdClearColorImage(commandBuffers, swapchainImages[imageIndex], VK_IMAGE_LAYOUT_GENERAL, &color, 1, &range);

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
    }

    glfwDestroyWindow(window);

    destroyDebugUtilsMessenger(instance);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffers);
    vkDestroyCommandPool(device, commandPool, 0);
    vkDestroySemaphore(device, acquireSemaphore, 0);
    vkDestroySemaphore(device, releaseSemaphore, 0);
    vkDestroySwapchainKHR(device, swapchain, 0);
    vkDestroySurfaceKHR(instance, surface, 0);
    vkDestroyDevice(device, 0);
    vkDestroyInstance(instance, 0);

    return 0;
}