#include <stdio.h>

#include "common.h"
#include "shaders.h"

#include <spirv/unified1/spirv.h>

struct Id{
    enum Kind {Unknown = 0, Variable, TypePointer, TypeStruct, TypeImage, TypeSampler, TypeSampledImage};

    Kind kind = Unknown;
    uint32_t typeId;
    uint32_t storageClass;
    uint32_t binding;
    uint32_t set;
};

static VkShaderStageFlagBits getShaderStage(SpvExecutionModel executionModel){

    switch (executionModel)
    {
        case SpvExecutionModelVertex:
            return VK_SHADER_STAGE_VERTEX_BIT;
        case SpvExecutionModelFragment:
            return VK_SHADER_STAGE_FRAGMENT_BIT;
        case SpvExecutionModelGLCompute:
            return VK_SHADER_STAGE_COMPUTE_BIT;
        case SpvExecutionModelMeshNV:
            return VK_SHADER_STAGE_MESH_BIT_NV;
        case SpvExecutionModelTaskNV:
            return VK_SHADER_STAGE_TASK_BIT_NV;
        default:
            assert(!"Unsupported Execution Model");
            return VkShaderStageFlagBits(0);
    }
}

static void parseShader(Shader& shader, const uint32_t* code, uint32_t codeSize) {

    assert(code[0] == SpvMagicNumber);

    uint32_t idBound = code[3];

    std::vector<Id> ids(idBound);

    const uint32_t* insn = code + 5;

    while (insn != code + codeSize){
        uint16_t opCode = uint16_t(insn[0]);
        uint16_t wordCount = uint16_t(insn[0] >> 16);

        switch (opCode)
        {
        case SpvOpEntryPoint:
        {
            uint32_t executionModel = insn[0];
            shader.stage = getShaderStage(SpvExecutionModel(insn[1]));
        }break;
        case SpvOpExecutionMode:
        {
            assert(wordCount >= 3);
            uint32_t mode = insn[2];

            switch (mode){
                case SpvExecutionModeLocalSize:
                    assert(wordCount == 6);
                    shader.localSizeX = insn[3];
                    shader.localSizeY = insn[4];
                    shader.localSizeZ = insn[5];
                    break;
            }
        }break;
        case SpvOpDecorate:
        {
            assert(wordCount >= 3);
            uint32_t id = insn[1];
            assert(id < idBound);

            switch (insn[2])
            {
            case SpvDecorationDescriptorSet:
                assert(wordCount >= 4);
                ids[id].set = insn[3];
                break;
            case SpvDecorationBinding:
                assert(wordCount >= 4);
                ids[id].binding = insn[3];
                break;
            }
        }break;
        case SpvOpTypeStruct:
        {
            assert(wordCount >= 2);

            uint32_t id = insn[1];
            assert(id < idBound);

            assert(ids[id].kind == Id::Unknown );
            ids[id].kind = Id::TypeStruct;
        }break;
        case SpvOpTypeImage:
        {
            assert(wordCount >= 2);

            uint32_t id = insn[1];
            assert(id < idBound);

            assert(ids[id].kind == Id::Unknown );
            ids[id].kind = Id::TypeImage;
        }break;
        case SpvOpTypeSampler:
        {
            assert(wordCount >= 2);

            uint32_t id = insn[1];
            assert(id < idBound);

            assert(ids[id].kind == Id::Unknown );
            ids[id].kind = Id::TypeSampler;
        }break;
        case SpvOpTypeSampledImage:
        {
            assert(wordCount >= 2);

            uint32_t id = insn[1];
            assert(id < idBound);

            assert(ids[id].kind == Id::Unknown );
            ids[id].kind = Id::TypeSampledImage;
        }break;
        case SpvOpTypePointer:
        {
            assert(wordCount == 4);

            uint32_t id = insn[1];
            assert(id < idBound);

            assert(ids[id].kind == Id::Unknown );
            ids[id].kind = Id::TypePointer;
            ids[id].typeId = insn[3];
            ids[id].storageClass = insn[2];
        }break;
        case SpvOpVariable:
        {
            assert(wordCount >= 4);

            uint32_t id = insn[2];
            assert(id < idBound);

            assert(ids[id].kind == Id::Unknown );
            ids[id].kind = Id::Variable;
            ids[id].typeId = insn[1];
            ids[id].storageClass = insn[3];
        }break;
        default:
            break;
        }

        assert(insn + wordCount <= code + codeSize);
        insn += wordCount;
    }

    for (auto& id : ids){
        if (id.kind == Id::Variable && (id.storageClass == SpvStorageClassUniform || id.storageClass == SpvStorageClassStorageBuffer || id.storageClass == SpvStorageClassUniformConstant)){
            // assume that id.type refers to a pointer to the storageBuffer
            assert(id.set == 0);
            assert(id.binding < 32);
            assert(ids[id.typeId].kind == Id::TypePointer);
            // assert((shader.storageBufferMask & (1 << id.binding)) == 0);

            Id::Kind typeKind = ids[ids[id.typeId].typeId].kind;

            switch(typeKind){
                case Id::TypeStruct:
                    shader.resourceTypes[id.binding] = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                    shader.resourceMask |= 1 << id.binding;
                    break;
                case Id::TypeImage:
                    shader.resourceTypes[id.binding] = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                    shader.resourceMask |= 1 << id.binding;
                    break;
                case Id::TypeSampler:
                    shader.resourceTypes[id.binding] = VK_DESCRIPTOR_TYPE_SAMPLER;
                    shader.resourceMask |= 1 << id.binding;
                    break;
                case Id::TypeSampledImage:
                    shader.resourceTypes[id.binding] = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                    shader.resourceMask |= 1 << id.binding;
                    break;
                default:
                    assert(!"Unknown resource type!");
            }
        }

        if (id.kind == Id::Variable && id.storageClass == SpvStorageClassPushConstant){
            shader.usesPushConstant = true;
        }
    }
}

static uint32_t gatherResourceS(Shaders shaders, VkDescriptorType(&resourceTypes)[32]){
    uint32_t resourceMask = 0;

    for (const Shader* shader : shaders){
        for (uint32_t i = 0; i < 32; ++i){
            if (shader->resourceMask & (1 << i)){
                if (resourceMask & (1 << i)){
                    assert(resourceTypes[i] == shader->resourceTypes[i]);
                } else {
                    resourceTypes[i] = shader->resourceTypes[i];
                    resourceMask |= 1 << i;
                }
            }
        }
    }

    return resourceMask;
}


static VkDescriptorSetLayout createSetLayout(VkDevice device, Shaders shaders) {

    std::vector<VkDescriptorSetLayoutBinding> setBindings = {};

    VkDescriptorType resourceTypes[32] = {};
    uint32_t resourceMask = gatherResourceS(shaders, resourceTypes);
    for (const Shader* shader : shaders){
        resourceMask |= shader->resourceMask;
    }

    for (uint32_t i = 0; i < 32; ++i){
        if (resourceMask & (1 << i)){
            VkDescriptorSetLayoutBinding bindings = {};
            bindings.binding = i;
            bindings.descriptorType = resourceTypes[i];
            bindings.descriptorCount = 1;

            // bindings.stageFlags = 0;
            for (const Shader* shader : shaders){
                if (resourceMask & (1 << i)){
                    bindings.stageFlags |= shader->stage;
                }
            }
            setBindings.push_back(bindings);
        }
    }

    VkDescriptorSetLayoutCreateInfo setCreateInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    setCreateInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
    setCreateInfo.bindingCount = uint32_t(setBindings.size());
    setCreateInfo.pBindings = setBindings.data();

    VkDescriptorSetLayout setLayout = 0;
    VK_CHECK(vkCreateDescriptorSetLayout(device, &setCreateInfo, 0, &setLayout));

    return setLayout;
}

static VkDescriptorUpdateTemplate createUpdateTemplate(VkDevice device, VkPipelineBindPoint bindPoint, VkPipelineLayout layout,  Shaders shaders){

    // VkDescriptorSetLayout setLayout = createSetLayout(device, VS, FS);

    std::vector<VkDescriptorUpdateTemplateEntry> entries;

    VkDescriptorType resourceTypes[32] = {};
    uint32_t resourceMask = gatherResourceS(shaders, resourceTypes);

    for (uint32_t i = 0; i < 32; ++i){
        if (resourceMask & (1 << i)){
            VkDescriptorUpdateTemplateEntry entry = {};
            entry.dstBinding = i;
            entry.dstArrayElement = 0;
            entry.descriptorCount = 1;
            entry.descriptorType = resourceTypes[i];
            entry.offset = sizeof(DescriptorInfo) * i;
            entry.stride = sizeof(DescriptorInfo);

            entries.push_back(entry);
        }
    }

    VkDescriptorUpdateTemplateCreateInfo createInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO};
    createInfo.descriptorUpdateEntryCount = uint32_t(entries.size());
    createInfo.pDescriptorUpdateEntries = entries.data();
    createInfo.templateType = VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_PUSH_DESCRIPTORS_KHR;
    // createInfo.descriptorSetLayout = setLayout;
    createInfo.pipelineBindPoint = bindPoint;
    createInfo.pipelineLayout = layout;

    VkDescriptorUpdateTemplate updateTemplate = 0;
    vkCreateDescriptorUpdateTemplate(device, &createInfo, 0, &updateTemplate);

    // TODO : Is this safe? or needs to add at uninitialization // Doing it in uninitialize()
    // vkDestroyDescriptorSetLayout(device, setLayout, 0);

    return updateTemplate;
}

static VkPipelineLayout createPipelineLayout(VkDevice device, Shaders shaders, VkShaderStageFlags pushConstantStages, size_t pushConstantSize)
{
    VkDescriptorSetLayout setLayout = createSetLayout(device, shaders);

    VkPipelineLayoutCreateInfo createInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    createInfo.setLayoutCount = 1;
    createInfo.pSetLayouts = &setLayout;

    VkPushConstantRange pushConstRange = {};
    if (pushConstantSize){
        pushConstRange.stageFlags = pushConstantStages;
        pushConstRange.size = uint32_t(pushConstantSize);

        createInfo.pushConstantRangeCount = 1;
        createInfo.pPushConstantRanges = &pushConstRange;
    }

    VkPipelineLayout layout = 0;
    VK_CHECK(vkCreatePipelineLayout(device, &createInfo, 0, &layout));


    // TODO : Is this safe? or needs to add at uninitialization // Doing it in uninitialize()
    // vkDestroyDescriptorSetLayout(device, setLayout, 0);

    return layout;
}


VkPipeline createGraphicsPipeline(VkDevice device, VkPipelineCache pipelineCache, VkRenderPass renderPass, Shaders shaders, VkPipelineLayout layout)
{
    VkGraphicsPipelineCreateInfo createInfo = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};

    // shader stages
    std::vector<VkPipelineShaderStageCreateInfo> stages;
    for (const Shader* shader : shaders){
        VkPipelineShaderStageCreateInfo stage ={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        stage.stage = shader->stage;
        stage.module = shader->module;
        stage.pName = "main";

        stages.push_back(stage);
    }
    createInfo.stageCount = uint32_t(stages.size());
    createInfo.pStages = stages.data();

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
    // rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_FRONT_BIT;
    createInfo.pRasterizationState = &rasterizationStateCreateInfo;

    VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    createInfo.pMultisampleState = &multisampleStateCreateInfo;

    VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = {VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    depthStencilStateCreateInfo.depthTestEnable = VK_TRUE;
    depthStencilStateCreateInfo.depthWriteEnable = VK_TRUE;
    depthStencilStateCreateInfo.depthCompareOp = VK_COMPARE_OP_GREATER; // AS WE ARE DOING INVERSE DEPTH TESTING
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

VkPipeline createComputePipeline(VkDevice device, VkPipelineCache pipelineCache, const Shader& shader, VkPipelineLayout layout){

    assert(shader.stage == VK_SHADER_STAGE_COMPUTE_BIT);

    VkComputePipelineCreateInfo createInfo = {VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};

    VkPipelineShaderStageCreateInfo stage ={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    stage.stage = shader.stage;
    stage.module = shader.module;
    stage.pName = "main";

    createInfo.stage = stage;
    createInfo.layout = layout;
    
    
    VkPipeline pipeline = 0;
    VK_CHECK(vkCreateComputePipelines(device, pipelineCache, 1, &createInfo, 0, &pipeline));
    return pipeline;
}

bool loadShader(Shader& result, VkDevice device, const char *path)
{

    FILE *file = fopen(path, "rb");
    if (!file)
        return false;

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

    assert(length % 4 == 0);
    parseShader(result, reinterpret_cast<const uint32_t*>(buffer), length / 4);

    delete buffer;
    result.module = shaderModule;
    // result.stage = ???;

    return true;
}

void destroyShaderModule(Shader& shader, VkDevice device){
    vkDestroyShaderModule(device, shader.module, 0);
}


Program createProgram(VkDevice device, VkPipelineBindPoint bindPoint, Shaders shaders, size_t pushConstantSize){
    
    VkShaderStageFlags pushConstantStages = 0;
    for (const Shader* shader : shaders){
        if (shader->usesPushConstant){
            pushConstantStages |= shader->stage;
        }
    }
    
    Program program = {};
    program.layout = createPipelineLayout(device, shaders, pushConstantStages, pushConstantSize);
    assert(program.layout);

    program.updateTemplate = createUpdateTemplate(device, bindPoint, program.layout, shaders);
    assert(program.updateTemplate);

    program.pushConstantStages = pushConstantStages;

    return program;
}

void destroyProgram(VkDevice device, const Program& program){
    vkDestroyDescriptorUpdateTemplate(device, program.updateTemplate, 0);
    vkDestroyPipelineLayout(device, program.layout, 0);
}