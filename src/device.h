#pragma once

typedef struct GLFWwindow GLFWwindow;

VkInstance createInstance();
uint32_t getGraphicsFamilyIndex(VkPhysicalDevice physicalDevice);
VkPhysicalDevice pickPhysicalDevice(VkPhysicalDevice *physicalDevices, uint32_t physicalDeviceCount, GLFWwindow* window);
VkDevice createDevice(VkInstance instance, VkPhysicalDevice physicalDevice, uint32_t familyIndex, bool rtxSupported);

void createDebugCallback(VkInstance instance);
void destroyDebugUtilsMessenger(VkInstance instance);