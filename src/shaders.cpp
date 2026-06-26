#include <stdio.h>

#include "common.h"
#include "shaders.h"

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


VkDescriptorSetLayout createSetLayout(VkDevice device, bool rtxEnabled) {

    std::vector<VkDescriptorSetLayoutBinding> setBindings = {};
    if (rtxEnabled){
        setBindings.resize(2);
        setBindings[0].binding = 0;
        setBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        setBindings[0].descriptorCount = 1;
        setBindings[0].stageFlags = VK_SHADER_STAGE_MESH_BIT_NV;
        setBindings[1].binding = 1;
        setBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        setBindings[1].descriptorCount = 1;
        setBindings[1].stageFlags = VK_SHADER_STAGE_MESH_BIT_NV;
    }
    else {
        setBindings.resize(1);
        setBindings[0].binding = 0;
        setBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        setBindings[0].descriptorCount = 1;
        setBindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    }
    VkDescriptorSetLayoutCreateInfo setCreateInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    setCreateInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
    setCreateInfo.bindingCount = uint32_t(setBindings.size());
    setCreateInfo.pBindings = setBindings.data();

    VkDescriptorSetLayout setLayout = 0;
    VK_CHECK(vkCreateDescriptorSetLayout(device, &setCreateInfo, 0, &setLayout));

    return setLayout;
}


VkDescriptorUpdateTemplate createUpdateTemplate(VkDevice device, VkPipelineBindPoint bindPoint, VkPipelineLayout layout, bool rtxEnabled){

    VkDescriptorSetLayout setLayout = createSetLayout(device, rtxEnabled);

    std::vector<VkDescriptorUpdateTemplateEntry> entries;

    if (rtxEnabled){
        entries.resize(2);
        entries[0].dstBinding = 0;
        entries[0].dstArrayElement = 0;
        entries[0].descriptorCount = 1;
        entries[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        entries[0].offset = sizeof(DescriptorInfo) * 0;
        entries[0].stride = sizeof(DescriptorInfo);

        entries[1].dstBinding = 1;
        entries[1].dstArrayElement = 0;
        entries[1].descriptorCount = 1;
        entries[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        entries[1].offset = sizeof(DescriptorInfo) * 1;
        entries[1].stride = sizeof(DescriptorInfo);
    } else {
        entries.resize(1);
        entries[0].dstBinding = 0;
        entries[0].dstArrayElement = 0;
        entries[0].descriptorCount = 1;
        entries[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        entries[0].offset = sizeof(DescriptorInfo) * 0;
        entries[0].stride = sizeof(DescriptorInfo);
    }

    VkDescriptorUpdateTemplateCreateInfo createInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO};
    createInfo.descriptorUpdateEntryCount = uint32_t(entries.size());
    createInfo.pDescriptorUpdateEntries = entries.data();
    createInfo.templateType = VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_PUSH_DESCRIPTORS_KHR;
    createInfo.descriptorSetLayout = setLayout;
    createInfo.pipelineBindPoint = bindPoint;
    createInfo.pipelineLayout = layout;

    VkDescriptorUpdateTemplate updateTemplate = 0;
    vkCreateDescriptorUpdateTemplate(device, &createInfo, 0, &updateTemplate);

    // TODO : Is this safe? or needs to add at uninitialization // Doing it in uninitialize()
    vkDestroyDescriptorSetLayout(device, setLayout, 0);

    return updateTemplate;
}

VkPipelineLayout createPipelineLayout(VkDevice device, bool rtxEnabled)
{
    VkDescriptorSetLayout setLayout = createSetLayout(device, rtxEnabled);

    VkPipelineLayoutCreateInfo createInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    createInfo.setLayoutCount = 1;
    createInfo.pSetLayouts = &setLayout;

    VkPipelineLayout layout = 0;
    VK_CHECK(vkCreatePipelineLayout(device, &createInfo, 0, &layout));


    // TODO : Is this safe? or needs to add at uninitialization // Doing it in uninitialize()
    // vkDestroyDescriptorSetLayout(device, setLayout, 0);

    return layout;
}



VkPipeline createGraphicsPipeline(VkDevice device, VkPipelineCache pipelineCache, VkRenderPass renderPass, VkShaderModule VS, VkShaderModule FS, VkPipelineLayout layout, bool rtxEnabled)
{

    VkGraphicsPipelineCreateInfo createInfo = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};

    // shader stages
    VkPipelineShaderStageCreateInfo stages[2] = {};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = rtxEnabled ? VK_SHADER_STAGE_MESH_BIT_NV : VK_SHADER_STAGE_VERTEX_BIT;
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
    rasterizationStateCreateInfo.lineWidth = 1.0f;
    rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
    // rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
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
