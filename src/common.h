#pragma once

#include <stdio.h>
#include <assert.h>

// #ifndef VOLK_IMPLEMENTATION
#include "../include/volk/volk.h"
// #endif

#include <vector>


#define VK_CHECK(call)             \
do                             \
{                              \
    VkResult res = call;       \
    assert(res == VK_SUCCESS); \
} while (0)

#ifndef ARRAYSIZE
#define ARRAYSIZE(array) (sizeof(array) / sizeof(array[0]))
#endif

extern FILE *VkLogFile;