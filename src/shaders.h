#pragma once

VkShaderModule loadShader(VkDevice device, const char *path);
VkPipelineLayout createPipelineLayout(VkDevice device, bool rtxEnabled);
VkDescriptorUpdateTemplate createUpdateTemplate(VkDevice device,VkPipelineBindPoint bindPoint, VkPipelineLayout layout, bool rtxEnabled);
VkPipeline createGraphicsPipeline(VkDevice device, VkPipelineCache pipelineCache, VkRenderPass renderPass, VkShaderModule VS, VkShaderModule FS, VkPipelineLayout layout, bool rtxEnabled);


struct DescriptorInfo {
    union  
    {
        VkDescriptorImageInfo imageInfo;
        VkDescriptorBufferInfo bufferInfo;
    };

    DescriptorInfo(VkSampler sampler, VkImageView imageView, VkImageLayout imageLayout){
        imageInfo.sampler = sampler;
        imageInfo.imageView = imageView;
        imageInfo.imageLayout = imageLayout;
    }
    DescriptorInfo(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range){
        bufferInfo.buffer = buffer;
        bufferInfo.offset = offset;
        bufferInfo.range = range;
    }

    DescriptorInfo(VkBuffer buffer){
        bufferInfo.buffer = buffer;
        bufferInfo.offset = 0;
        bufferInfo.range = VK_WHOLE_SIZE;
    }

};
