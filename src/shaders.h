#pragma once

struct Shader {
    VkShaderModule module;
    VkShaderStageFlagBits stage;

    uint32_t storageBufferMask;
};

bool loadShader(Shader& shader, VkDevice device, const char *path);
void destroyShaderModule(Shader& shader, VkDevice device);

VkPipelineLayout createPipelineLayout(VkDevice device,  const Shader& VS, const Shader& FS);
VkDescriptorUpdateTemplate createUpdateTemplate(VkDevice device,VkPipelineBindPoint bindPoint, VkPipelineLayout layout,  const Shader& VS, const Shader& FS);
VkPipeline createGraphicsPipeline(VkDevice device, VkPipelineCache pipelineCache, VkRenderPass renderPass, const Shader& VS, const Shader& FS, VkPipelineLayout layout);


struct DescriptorInfo {
    union  
    {
        VkDescriptorImageInfo imageInfo;
        VkDescriptorBufferInfo bufferInfo;
    };

    DescriptorInfo(){}
    
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
