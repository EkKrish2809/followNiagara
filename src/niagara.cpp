#include <stdio.h>
#include <assert.h>

#include <GLFW/glfw3.h>

#include <vulkan/vulkan.h>

#define VK_CHECK(call) \
    do{ \
        VkResult result = call; \
        assert(result == VK_SUCCESS); \
    } while(0)



int main(){
    int ret = glfwInit();
    assert(ret);

    // VkInstanceCreateInfo createInfo;
    // VkInstance instance = 0;
    // VK_CHECK(vkCreateInstance(&createInfo, 0, &instance));


    GLFWwindow * window = glfwCreateWindow(1600, 900, "Following Niagara", 0, 0);
    assert(window);

    while (!glfwWindowShouldClose(window))
    {
        /* code */
        glfwPollEvents();
    }
    
    glfwDestroyWindow(window);

    return 0;
}