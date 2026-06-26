#pragma once

#include <assert.h>
#include "../include/volk/volk.h"

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
