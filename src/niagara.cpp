
// #include <vulkan/vulkan.h>
#define VK_USE_PLATFORM_XLIB_KHR
// #define VOLK_IMPLEMENTATION
#define VOLK_IMPLEMENTATION
// #include "../include/volk/volk.h"
#include "common.h"


#include "device.h"
#include "resources.h"
#include "shaders.h"
#include "swapchain.h"


#include <math.h>
#include <string.h>

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>

#include <X11/Xlib.h>

#include "../include/fast_obj/fast_obj.h"

#include "../meshoptimizer/src/meshoptimizer.h"

// math includes
#include <glm/vec4.hpp>
#include <glm/matrix.hpp>
#include <glm/ext/quaternion_float.hpp>
#include <glm/ext/quaternion_transform.hpp>

#define _DEBUG 0

#define RTX 0



bool meshShadingEnabled = true;
FILE *VkLogFile = stderr;


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

/* Day 2 */
VkRenderPass createRenderPass(VkDevice device, VkFormat colorFormat, VkFormat depthFormat)
{

    VkAttachmentDescription attachments[2] = {};
    attachments[0].format = colorFormat;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    attachments[1].format = depthFormat;
    attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachment = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference depthAttachment = {1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subPass = {};
    subPass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subPass.colorAttachmentCount = 1;
    subPass.pColorAttachments = &colorAttachment;
    subPass.pDepthStencilAttachment = &depthAttachment;

    VkRenderPassCreateInfo createInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    createInfo.attachmentCount = sizeof(attachments) / sizeof(attachments[0]);
    createInfo.pAttachments = attachments;
    createInfo.subpassCount = 1;
    createInfo.pSubpasses = &subPass;

    VkRenderPass renderPass = 0;
    VK_CHECK(vkCreateRenderPass(device, &createInfo, 0, &renderPass));

    return renderPass;
}

VkFramebuffer createFramebuffer(VkDevice device, VkRenderPass renderPass, VkImageView colorView, VkImageView depthView, uint32_t width, uint32_t height){

    VkImageView attachments[] = {colorView, depthView};
    VkFramebufferCreateInfo createInfo = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    createInfo.renderPass = renderPass;
    createInfo.attachmentCount = ARRAYSIZE(attachments);
    createInfo.pAttachments = attachments;
    createInfo.width = width;
    createInfo.height = height;
    createInfo.layers = 1;

    VkFramebuffer framebuffer = 0;
    vkCreateFramebuffer(device, &createInfo, 0, &framebuffer);

    return framebuffer;
}





VkQueryPool createQueryPool(VkDevice device, uint32_t queryCount){

    VkQueryPoolCreateInfo createInfo = {VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
    createInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
    createInfo.queryCount = queryCount;

    VkQueryPool queryPool = 0;
    VK_CHECK(vkCreateQueryPool(device, &createInfo, 0, &queryPool));

    return queryPool;
}

// Day 4 -> Mesh loading

// For mesh shader
struct alignas(16) Meshlet{
    glm::vec3 center;
    float radius;
    // glm::vec3 cone_apex;
    // float padding;
    int8_t cone_axis[3];
    int8_t cone_cutoff;

    uint32_t dataOffset;    // dataOffset... dataOffset+vertexCount-1 stores vertex indices, we store indices packed in 4b units after that
    uint8_t vertexCount;
    uint8_t triangleCount;

};

struct alignas(16) Globals{
    glm::mat4x4 projection;
};

struct alignas(16) MeshDraw{
    glm::vec3 position;
    float scale;
    glm::quat orientation;


    union{
        uint32_t commandData[7];

        struct{
            VkDrawIndexedIndirectCommand commandIndirect;
            VkDrawMeshTasksIndirectCommandNV commandIndirectMS;
        };
    };
};

struct Vertex{
    float vx, vy, vz;
    uint8_t nx, ny, nz, nw;
    uint16_t tu, tv;
};


struct Mesh{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<Meshlet> meshlets;
    std::vector<uint32_t> meshletdata;
};

bool loadMesh(Mesh& result, const char* path){
    fastObjMesh* objMesh = fast_obj_read(path);;
	if (!objMesh){
        printf("Can not found the specified OBJ file\n");
		return false;
	}

	// size_t index_count = file.f_size / 3;
	
	std::vector<Vertex> vertices;
    vertices.reserve(objMesh->index_count);
	// std::vector<uint32_t> indices(objMesh->index_count);

    uint32_t indexOffset = 0;   // running offset into mesh->indices[]
	
    // Loop over all faces
    for (unsigned int f = 0; f < objMesh->face_count; f++) {
        // Get the number of vertices in this face (usually 3 for triangles)
        unsigned int vertex_count = objMesh->face_vertices[f];
        
        // Get the starting index for this face in the indices array
        // unsigned int index_offset = objMesh->face_offsets[f];

        // Loop over vertices in the face
        for (unsigned int v = 1; v + 1 < vertex_count; ++v) {

            // The three corners of this triangle within the face
            uint32_t corners[3] = { 0, v, v + 1 };

            for (uint32_t c : corners){
                fastObjIndex idx = objMesh->indices[indexOffset + c];

                Vertex vtx{};

                // ── Position ──────────────────────────────────────────────
                // fast-obj stores positions starting at slot 1 (slot 0 is a
                // dummy zero-initialised entry).  Each position is 3 floats.
                if (idx.p > 0) {
                    vtx.vx = (objMesh->positions[3 * idx.p + 0]);
                    vtx.vy = (objMesh->positions[3 * idx.p + 1]);
                    vtx.vz = (objMesh->positions[3 * idx.p + 2]);
                }

                // ── Normal ────────────────────────────────────────────────
                if (idx.n > 0) {
                    float nx = objMesh->normals[3 * idx.n + 0];
                    float ny = objMesh->normals[3 * idx.n + 1];
                    float nz = objMesh->normals[3 * idx.n + 2];
                    // TODO : fix rounding
                    vtx.nx = uint8_t(nx * 127.f + 127.f);
                    vtx.ny = uint8_t(ny * 127.f + 127.f);
                    vtx.nz = uint8_t(nz * 127.f + 127.f);
                }

                // ── Texture coordinate ────────────────────────────────────
                // fast-obj stores UVs as 2 floats per entry.
                // OBJ convention: V=0 is bottom; Vulkan: V=0 is top.
                // Flip V so textures appear correctly.
                if (idx.t > 0) {
                    vtx.tu =       meshopt_quantizeHalf(objMesh->texcoords[2 * idx.t + 0]);
                    vtx.tv = meshopt_quantizeHalf(1.0f - objMesh->texcoords[2 * idx.t + 1]); // flip V
                }

                // printf("x = %f, y = %f, z = %f\n", vtx.nx, vtx.ny, vtx.nz);

                vertices.push_back(vtx);
            }
            
            // Use px, py, pz, nx, ny, nz, u, v_coord to build your vertex buffer
        }
        indexOffset += vertex_count;   // advance past all corners of this face
    }
	/*for (size_t i=0; i < index_count; ++i) {
		Vertex& v = vertices[i];

		int vi = file.f[i * 3 + 0];
		int vti = file.f[i * 3 + 1];
		int vni = file.f[i * 3 + 2];

		v.vx = file.v[vi * 3 + 0];
		v.vy = file.v[vi * 3 + 1];
		v.vz = file.v[vi * 3 + 2];
		v.nx = vni < 0 ? 0.f : file.vn[vni * 3 + 0];
		v.ny = vni < 0 ? 0.f : file.vn[vni * 3 + 1];
		v.nz = vni < 0 ? 1.f : file.vn[vni * 3 + 2];
		v.tu = vti < 0 ? 0.f : file.vt[vti * 3 + 0];
		v.tv = vti < 0 ? 0.f : file.vt[vti * 3 + 1];
	}*/

		std::vector<uint32_t> remap(objMesh->index_count);
		size_t vertex_count = meshopt_generateVertexRemap(remap.data(), 0, objMesh->index_count, vertices.data(), objMesh->index_count, sizeof(Vertex));

		result.vertices.resize(vertex_count);
		result.indices.resize(objMesh->index_count);

		meshopt_remapVertexBuffer(result.vertices.data(), vertices.data(), objMesh->index_count, sizeof(Vertex), remap.data());
		meshopt_remapIndexBuffer(result.indices.data(), 0, objMesh->index_count, remap.data());

    if (1){
        meshopt_optimizeVertexCache(result.indices.data(), result.indices.data(), objMesh->index_count, vertex_count);
        meshopt_optimizeVertexFetch(result.vertices.data(), result.indices.data(), objMesh->index_count, result.vertices.data(), vertex_count, sizeof(Vertex));
    }


    fast_obj_destroy(objMesh);

    return true;
}

float halfToFloat(uint16_t v){
    uint16_t sign = v >> 15;
    uint16_t exp = (v >> 10) & 31;
    uint16_t mant = v & 1023;

    assert(exp != 31);

    if (exp == 0){
        assert(mant == 0);
        return 0.f;
    } else {
        return (sign ? -1.f : 1.f) * ldexpf(float(mant + 1024) / 1024.f, exp -15);
    }
}

/*
void buildMeshlets(Mesh& mesh){

    // size_t max_vertices = 64;
    // size_t max_triangles = 126;
    // std::vector<meshopt_Meshlet> meshlets(meshopt_buildMeshletsBound(mesh.indices.size(), max_vertices, max_triangles));
    // meshlets.resize(meshopt_buildMeshlets(meshlets.data(), mesh.indices.data(), mesh.indices.size(), mesh.vertices.size(), max_vertices, max_triangles));

    // meshlets.resize(meshopt_buildMeshlets(meshlets.data(), mesh.vertices.data(), meshlets[0]. mesh.indices.data(), mesh.indices.size(), mesh.vertices.size(), max_vertices, max_triangles));

    Meshlet meshlet = {};

    std::vector<uint8_t> meshletVertices(mesh.vertices.size(), 0xff);

    for (size_t i=0; i < mesh.indices.size(); i += 3){
        unsigned int a = mesh.indices[i + 0];
        unsigned int b = mesh.indices[i + 1];
        unsigned int c = mesh.indices[i + 2];

        uint8_t& av = meshletVertices[a];
        uint8_t& bv = meshletVertices[b];
        uint8_t& cv = meshletVertices[c];

        if (meshlet.vertexCount + (av == 0xff) + (bv == 0xff) + (cv == 0xff) > 64 || meshlet.triangleCount >= 126){
            mesh.meshlets.push_back(meshlet);
            meshlet = {};
            memset(meshletVertices.data(), 0xff, mesh.vertices.size());
        }

        if (av == 0xff){
            av = meshlet.vertexCount;
            meshlet.vertices[meshlet.vertexCount++] = a;
        }

        if (bv == 0xff){
            bv = meshlet.vertexCount;
            meshlet.vertices[meshlet.vertexCount++] = b;
        }
        if (cv == 0xff){
            cv = meshlet.vertexCount;
            meshlet.vertices[meshlet.vertexCount++] = c;
        }

        meshlet.indices[meshlet.triangleCount * 3] [0] = av;
        meshlet.indices[meshlet.triangleCount * 3] [1] = bv;
        meshlet.indices[meshlet.triangleCount * 3] [2] = cv;
        meshlet.triangleCount++;
    }

    if (meshlet.triangleCount)
        mesh.meshlets.push_back(meshlet);

    // TODO: we don't really need this but it makes sure we can assume that we need all 32 meshlets in task shader
    while(mesh.meshlets.size() % 32){
        mesh.meshlets.push_back(Meshlet());
    }
// }


// void buildMeshletCones(Mesh& mesh){

    size_t zeroarea = 0;
    for (Meshlet& meshlet : mesh.meshlets){
        
        float normals[126][6] = {};
        for (unsigned int i = 0; i < meshlet.triangleCount; ++i){
            unsigned int a = meshlet.indices[i * 3] [0];
            unsigned int b = meshlet.indices[i * 3] [1];
            unsigned int c = meshlet.indices[i * 3] [2];

            const Vertex& va = mesh.vertices[meshlet.vertices[a]];
            const Vertex& vb = mesh.vertices[meshlet.vertices[b]];
            const Vertex& vc = mesh.vertices[meshlet.vertices[c]];

            float p0[3] = {(va.vx), (va.vy), (va.vz)};
            float p1[3] = {(vb.vx), (vb.vy), (vb.vz)};
            float p2[3] = {(vc.vx), (vc.vy), (vc.vz)};

            float p10[3] = {p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2]};
            float p20[3] = {p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2]};

            float normalx = p10[1] * p20[2] - p10[2] * p20[1];
            float normaly = p10[2] * p20[0] - p10[0] * p20[2];
            float normalz = p10[0] * p20[1] - p10[1] * p20[0];

            float area = sqrtf(normalx * normalx + normaly * normaly + normalz * normalz);
            float invarea = area == 0.f ? 0.f : 1 / area;
            zeroarea += area == 0.f;

            normals[i][0] = normalx * invarea;
            normals[i][1] = normaly * invarea;
            normals[i][2] = normalz * invarea;
        }

        float avgnormal[3] = {};

        for (unsigned int i = 0; i < meshlet.triangleCount; ++i){
            avgnormal[0] += normals[i][0];
            avgnormal[1] += normals[i][1];
            avgnormal[2] += normals[i][2];
        }

        float avglength = sqrtf(avgnormal[0] * avgnormal[0] + avgnormal[1] * avgnormal[1] + avgnormal[2] * avgnormal[2]);
        if (avglength == 0.f) {
            avgnormal[0] = 1.f;
            avgnormal[1] = 0.f;
            avgnormal[2] = 0.f;
        } else {
            avgnormal[0] /= avglength;
            avgnormal[1] /= avglength;
            avgnormal[2] /= avglength;
        }

        float mindp = 1.f;

        for (unsigned int i = 0; i < meshlet.triangleCount; ++i){
            float dp = normals[i][0] * avgnormal[0] + normals[i][1] * avgnormal[1] + normals[i][2] * avgnormal[2];

            mindp = std::min(mindp, dp);
        }

        float conew = mindp <= 0.f ? 1 : sqrtf(1 - mindp * mindp);

        meshlet.cone[0] = avgnormal[0];
        meshlet.cone[1] = avgnormal[1];
        meshlet.cone[2] = avgnormal[2];
        meshlet.cone[3] = conew;
    }
}
*/

// TODO : writing this function here like this for now, but needs to correctly map to latest meshoptimizer API's
// if it is already to the latest API, merge it to above buildMeshlets() function 
void buildMeshletsOpt(Mesh& mesh){
    size_t max_vertices = 64;
    size_t max_triangles = 124;
    const float cone_weight = 0.0f;   // balance cluster size vs cone culling quality

    size_t maxMeshlets = meshopt_buildMeshletsBound(mesh.indices.size(), max_vertices, max_triangles);

    std::vector<meshopt_Meshlet> meshlets(maxMeshlets);
    // meshlets.resize(meshopt_buildMeshlets())
    std::vector<uint32_t>        mo_vertices(maxMeshlets * max_vertices);
    std::vector<uint8_t>         mo_triangles(maxMeshlets * max_triangles * 3);

    // build meshlets
    size_t meshletCount = meshopt_buildMeshlets(
        meshlets.data(),
        mo_vertices.data(),
        mo_triangles.data(),
        mesh.indices.data(),
        mesh.indices.size(),
        &mesh.vertices[0].vx,   // float* to first position component
        mesh.vertices.size(),
        sizeof(Vertex),          // stride in bytes
        max_vertices,
        max_triangles,
        cone_weight);

    meshlets.resize(meshletCount);
    mesh.meshlets.clear();
    mesh.meshlets.reserve(meshletCount);

    // while (meshlets.size() % 32)
    //     meshlets.push_back(meshopt_Meshlet{});
    
    for (size_t i = 0; i < meshletCount; ++i){
       
        const meshopt_Meshlet& src = meshlets[i];
        size_t dataOffset = mesh.meshletdata.size();

        // Copy vertices
        for (unsigned int j = 0; j < src.vertex_count; ++j){
            // dst.vertices[j] = mo_vertices[src.vertex_offset + j];
            mesh.meshletdata.push_back(mo_vertices[src.vertex_offset + j]);
        }

        const uint8_t* triangleData = mo_triangles.data() + src.triangle_offset;
        unsigned int indexGroupCount = (src.triangle_count * 3 + 3) / 4;

        for (unsigned int j = 0; j < indexGroupCount; ++j){
            uint32_t packed = 0;
            unsigned int count = std::min<unsigned int>(4, src.triangle_count * 3 - j * 4);
            memcpy(&packed, triangleData + j * 4, count);
            mesh.meshletdata.push_back(packed);
        }

        meshopt_Bounds bounds = meshopt_computeMeshletBounds(
            &mo_vertices[src.vertex_offset],
            &mo_triangles[src.triangle_offset],
            src.triangle_count,
            &mesh.vertices[0].vx,
            mesh.vertices.size(),
            sizeof(Vertex));

        // Meshlet& dst = mesh.meshlets[i];
        Meshlet dst = {};

        dst.dataOffset = uint32_t(dataOffset);
        dst.triangleCount = src.triangle_count;
        dst.vertexCount = src.vertex_count;
        
        dst.center = glm::vec3(bounds.center[0], bounds.center[1], bounds.center[2]);
        dst.radius = bounds.radius;
        // dst.cone_apex = glm::vec3(bounds.cone_apex[0], bounds.cone_apex[1], bounds.cone_apex[2]);
        // dst.padding = 0;
        dst.cone_axis[0] = bounds.cone_axis_s8[0];
        dst.cone_axis[1] = bounds.cone_axis_s8[1];
        dst.cone_axis[2] = bounds.cone_axis_s8[2];
        dst.cone_cutoff = bounds.cone_cutoff_s8;

        mesh.meshlets.push_back(dst);
    }

    // 5. Pad to a multiple of 32 so the task shader can always dispatch
    //    full 32-meshlet waves without bounds-checking.
    while (mesh.meshlets.size() % 32)
        mesh.meshlets.push_back(Meshlet{});

    // printf("Meshlets: %zu  zero-area: %zu\n", mesh.meshlets.size(), zeroarea);

}


// 



void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods){
    if (action == GLFW_PRESS){
        if (key == GLFW_KEY_R){
            meshShadingEnabled = !meshShadingEnabled;
        }
    }
}

glm::mat4 perspectiveProjection(float fovY, float aspectWbyH, float zNear)
{
    float f = 1.0f / tanf(fovY / 2.0f);
    return glm::mat4(
        f / aspectWbyH, 0.0f,  0.0f,  0.0f,
                  0.0f,    f,  0.0f,  0.0f,
                  0.0f, 0.0f,  0.0f, 1.0f,
                  0.0f, 0.0f, zNear,  0.0f);
}

int main(int argc, const char** argv)
{
	if (argc < 2){
		printf("Usage : %s [mesh]\n", argv[0]);
        argv[1] = "asset/kitten.obj";
		// return 1;
	}

    VkLogFile = fopen("validation.log", "wa");
    assert(VkLogFile);

    int ret = glfwInit();
    assert(ret);

    VK_CHECK(volkInitialize());

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    VkInstance instance = createInstance();
    assert(instance);

    volkLoadInstance(instance);

    // enable validation layer output
    createDebugCallback(instance);

    // create window
    GLFWwindow *window = glfwCreateWindow(1024, 768, "Following Niagara", 0, 0);
    assert(window);

    glfwSetKeyCallback(window, keyCallback);

    // physical device selection
    VkPhysicalDevice physicalDevices[16];
    uint32_t physicalDeviceCount = sizeof(physicalDevices) / sizeof(physicalDevices[0]);
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices));

    VkPhysicalDevice physicalDevice = pickPhysicalDevice(physicalDevices, physicalDeviceCount, window);
    assert(physicalDevice);

    uint32_t extensionCount = 0;
    VK_CHECK(vkEnumerateDeviceExtensionProperties(physicalDevice, 0, &extensionCount, 0));

    std::vector<VkExtensionProperties> extensions(extensionCount);
    VK_CHECK(vkEnumerateDeviceExtensionProperties(physicalDevice, 0, &extensionCount, extensions.data()));

    bool meshShadingSupported = false;
    for (auto& ext : extensions){
        if (strcmp(ext.extensionName, "VK_NV_mesh_shader") == 0){
            meshShadingSupported = true;
            break;
        }
    }

    meshShadingEnabled = meshShadingSupported;

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(physicalDevice, &props);
    assert(props.limits.timestampComputeAndGraphics);

    // create logical device
    uint32_t familyIndex = getGraphicsFamilyIndex(physicalDevice);
    assert(familyIndex != VK_QUEUE_FAMILY_IGNORED);
    VkDevice device = createDevice(instance, physicalDevice, familyIndex, meshShadingSupported);
    assert(device);

    // creating surface
    VkSurfaceKHR surface = createSurface(instance, window);
    assert(surface);

    int winWidth = 0, winHeight = 0;
    glfwGetWindowSize(window, &winWidth, &winHeight);

    // swapchain format
    VkFormat swapchainFormat = getSwapchainFormat(physicalDevice, surface);
    VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;

    VkSurfaceCapabilitiesKHR surfaceCaps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCaps);

    // create semaphore
    VkSemaphore acquireSemaphore = createSemaphore(device);
    assert(acquireSemaphore);

    VkSemaphore releaseSemaphore = createSemaphore(device);
    assert(releaseSemaphore);

    // get queue
    VkQueue queue = 0;
    vkGetDeviceQueue(device, familyIndex, 0, &queue);

    // create renderpass
    VkRenderPass renderPass = createRenderPass(device, swapchainFormat, depthFormat);
    assert(renderPass);

    bool rcs = false;
    // shader module
    Shader meshVS = {};
    rcs = loadShader(meshVS, device, "src/shaders/mesh.vert.spv");
    assert(rcs);

    Shader meshFS = {};
    rcs = loadShader(meshFS, device, "src/shaders/mesh.frag.spv");
    assert(rcs);

    Shader meshletMS = {};
    Shader meshletTS = {};
    if (meshShadingSupported){
        rcs = loadShader(meshletMS, device, "src/shaders/meshlet.mesh.spv");
        assert(rcs);

        rcs = loadShader(meshletTS, device, "src/shaders/meshlet.task.spv");
        assert(rcs);
    } 

    
    // graphics pipeline layout
    Program meshProgram = createProgram(device, VK_PIPELINE_BIND_POINT_GRAPHICS, {&meshVS, &meshFS}, sizeof(Globals));

    Program meshProgramMS = {};
    if (meshShadingSupported){
        meshProgramMS = createProgram(device, VK_PIPELINE_BIND_POINT_GRAPHICS, { &meshletTS, &meshletMS, &meshFS }, sizeof(Globals));
    }

    // create graphics pipeline
    VkPipelineCache pipelineCache = 0;
    VkPipeline meshPipeline = createGraphicsPipeline(device, pipelineCache, renderPass, {&meshVS, &meshFS}, meshProgram.layout);
    assert(meshPipeline);

    VkPipeline meshPipelineMS = 0;
    if (meshShadingSupported){
        meshPipelineMS = createGraphicsPipeline(device, pipelineCache, renderPass, { &meshletTS, &meshletMS, &meshFS}, meshProgramMS.layout);
        assert(meshPipelineMS);
    }

    // swapchain creation code
    Swapchain swapchain;
    createSwapchain(swapchain, physicalDevice, device, surface, familyIndex, swapchainFormat, winWidth, winHeight, renderPass);

    VkQueryPool queryPool = createQueryPool(device, 128);
    assert(queryPool);

    // commandpool
    VkCommandPool commandPool = createCommandPool(device, familyIndex);
    assert(commandPool);

    VkCommandBufferAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffers = 0;
    VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, &commandBuffers));

    VkPhysicalDeviceMemoryProperties memoryProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

	// Mesh loading code
	Mesh mesh;
	bool rcm = loadMesh(mesh, argv[1]);
    assert(rcm);

    if (meshShadingSupported){
        buildMeshletsOpt(mesh);
        // buildMeshletCones(mesh);

        // size_t culled = 0;
        // for (Meshlet& meshlet : mesh.meshlets){
        //     if (meshlet.cone[3] < 1.0f && meshlet.cone[2] > meshlet.cone[3]){
        //         culled++;
        //     }
        // }

        // printf("Culled meshlets : %d/%d\n", int(culled), int(mesh.meshlets.size()));
    }

    Buffer scratch = {};
    createBuffer(scratch, device, memoryProperties, 128 * 1024 * 1024, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    Buffer vb = {};
    createBuffer(vb, device, memoryProperties, 128 * 1024 * 1024, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    Buffer ib = {};
    createBuffer(ib, device, memoryProperties, 128 * 1024 * 1024, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    Buffer mb = {};
    Buffer mdb = {};
    if (meshShadingSupported){
        createBuffer(mb, device, memoryProperties, 128 * 1024 * 1024, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        createBuffer(mdb, device, memoryProperties, 128 * 1024 * 1024, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    }

    uploadBuffer(device, commandPool, commandBuffers, queue, vb, scratch, mesh.vertices.data(), mesh.vertices.size() * sizeof(Vertex));
    
    uploadBuffer(device, commandPool, commandBuffers, queue, ib, scratch, mesh.indices.data(), mesh.indices.size() * sizeof(uint32_t));

    if (meshShadingSupported) {
        uploadBuffer(device, commandPool, commandBuffers, queue, mb, scratch, mesh.meshlets.data(), mesh.meshlets.size() * sizeof(Meshlet));
        uploadBuffer(device, commandPool, commandBuffers, queue, mdb, scratch, mesh.meshletdata.data(), mesh.meshletdata.size() * sizeof(uint32_t));
    }

    fprintf(VkLogFile, "\n==================================================================================================================\n");
    fprintf(VkLogFile, "Begin Rendering");
    fprintf(VkLogFile, "\n==================================================================================================================\n");


    uint32_t drawCount = 30000;
    std::vector<MeshDraw> draws(drawCount);

    srand(42);

    for (uint32_t i=0; i<drawCount; ++i){
        draws[i].position[0] = float(rand()) / RAND_MAX * 40 - 20; 
        draws[i].position[1] = float(rand()) / RAND_MAX * 40 - 20; 
        draws[i].position[2] = float(rand()) / RAND_MAX * 40 - 20;
        draws[i].scale = float(rand()) / RAND_MAX + 0.3f;

        glm::vec3 axis = glm::vec3(float(rand()) / RAND_MAX * 2 - 1, float(rand()) / RAND_MAX * 2 - 1, float(rand()) / RAND_MAX * 2 - 1);
        float angle = glm::radians(float(rand()) / RAND_MAX * 90.f);

        draws[i].orientation = glm::rotate(glm::quat(1, 0, 0, 0), angle, axis);

        //
        memset(draws[i].commandData, 0, sizeof(draws[i].commandData));
        draws[i].commandIndirect.indexCount = uint32_t(mesh.indices.size());
        draws[i].commandIndirect.instanceCount = 1;
        draws[i].commandIndirectMS.taskCount = uint32_t(mesh.meshlets.size() / 32);

    }

    Buffer db = {};
    createBuffer(db, device, memoryProperties, 128 * 1024 * 1024, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT| VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    uploadBuffer(device, commandPool, commandBuffers, queue, db, scratch, draws.data(), draws.size() * sizeof(MeshDraw));

    Image colorTarget = {};
    Image depthTarget = {};
    VkFramebuffer targetFB = 0;

    while (!glfwWindowShouldClose(window))
    {
        /* code */
        double frameBegin = glfwGetTime() * 1000.0;

        glfwPollEvents();

        if (resizeSwapchainIfNecessary(swapchain, device, physicalDevice, surface, familyIndex, swapchainFormat, renderPass) || !targetFB){
            // recreate stuff
            if (colorTarget.image){
                destroyImage(colorTarget, device);
            }
            if (depthTarget.image){
                destroyImage(depthTarget, device);
            }
            if (targetFB){
                vkDestroyFramebuffer(device, targetFB, 0);
            }

            createImage(colorTarget, device, memoryProperties, swapchain.width, swapchain.height, swapchainFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
            createImage(depthTarget, device, memoryProperties, swapchain.width, swapchain.height, depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
            targetFB = createFramebuffer(device, renderPass, colorTarget.imageView, depthTarget.imageView, swapchain.width, swapchain.height);
        }

        uint32_t imageIndex = 0;
        VkResult acquireResult = vkAcquireNextImageKHR(device, swapchain.swapchain, ~0ull, acquireSemaphore, VK_NULL_HANDLE, &imageIndex);
        if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR)
        {
            // surface changed since last resizeSwapchain() check — skip this frame, rebuild next iteration
            continue;
        }
        assert(acquireResult == VK_SUCCESS || acquireResult == VK_SUBOPTIMAL_KHR);

        VK_CHECK(vkResetCommandPool(device, commandPool, 0));

        VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        VK_CHECK(vkBeginCommandBuffer(commandBuffers, &beginInfo));

        vkCmdResetQueryPool(commandBuffers, queryPool, 0, 128);
        vkCmdWriteTimestamp(commandBuffers, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queryPool, 0);

        // VkImageMemoryBarrier renderBeginBarrier = imageBarrier(swapchain.images[imageIndex], 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        //                                                        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        // vkCmdPipelineBarrier(commandBuffers, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        //                      VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, &renderBeginBarrier);

        VkImageMemoryBarrier renderBeginBarrier[] = {
            imageBarrier(colorTarget.image, 0, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL),
            imageBarrier(depthTarget.image, 0, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT),
        };
        vkCmdPipelineBarrier(commandBuffers, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, ARRAYSIZE(renderBeginBarrier), renderBeginBarrier);

        
        VkClearValue clearColor[2] = {};
        clearColor[0].color = {48.0f / 255.0f, 10.0f / 255.0f, 36.0f / 255.0f, 1};
        clearColor[1].depthStencil = {0.f, 0};

        VkRenderPassBeginInfo passBeginInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        passBeginInfo.renderPass = renderPass;
        // passBeginInfo.framebuffer = swapchain.framebuffers[imageIndex];
        passBeginInfo.framebuffer = targetFB;
        passBeginInfo.renderArea.extent.width = swapchain.width;
        passBeginInfo.renderArea.extent.height = swapchain.height;
        passBeginInfo.clearValueCount = ARRAYSIZE(clearColor);
        passBeginInfo.pClearValues = clearColor;

        vkCmdBeginRenderPass(commandBuffers, &passBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport = {0, float(swapchain.height), float(swapchain.width), -float(swapchain.height), 0, 1};
        VkRect2D scissor = {{0, 0}, {uint32_t(swapchain.width), uint32_t(swapchain.height)}};

        vkCmdSetViewport(commandBuffers, 0, 1, &viewport);
        vkCmdSetScissor(commandBuffers, 0, 1, &scissor);

        Globals globals = {};
        globals.projection = perspectiveProjection(glm::radians(70.f), float(swapchain.width) / float(swapchain.height), 0.01f);

        if (meshShadingSupported && meshShadingEnabled){
            vkCmdBindPipeline(commandBuffers, VK_PIPELINE_BIND_POINT_GRAPHICS, meshPipelineMS);

            DescriptorInfo descriptors[] = {db.buffer, mb.buffer, mdb.buffer, vb.buffer};
            vkCmdPushDescriptorSetWithTemplateKHR(commandBuffers, meshProgramMS.updateTemplate, meshProgramMS.layout, 0, descriptors);
            
            vkCmdPushConstants(commandBuffers, meshProgramMS.layout, meshProgramMS.pushConstantStages, 0, sizeof(globals), &globals);
            vkCmdDrawMeshTasksIndirectNV(commandBuffers, db.buffer, offsetof(MeshDraw, commandIndirectMS), uint32_t(draws.size()), sizeof(MeshDraw));
        }
        else {
            vkCmdBindPipeline(commandBuffers, VK_PIPELINE_BIND_POINT_GRAPHICS, meshPipeline);

            DescriptorInfo descriptors[] = {db.buffer, vb.buffer};
            vkCmdPushDescriptorSetWithTemplateKHR(commandBuffers, meshProgram.updateTemplate, meshProgram.layout, 0, descriptors);

            vkCmdBindIndexBuffer(commandBuffers, ib.buffer, 0, VK_INDEX_TYPE_UINT32);

            vkCmdPushConstants(commandBuffers, meshProgram.layout, meshProgram.pushConstantStages, 0, sizeof(globals), &globals);
            vkCmdDrawIndexedIndirect(commandBuffers, db.buffer, offsetof(MeshDraw, commandIndirect), uint32_t(draws.size()), sizeof(MeshDraw));
        }
        vkCmdEndRenderPass(commandBuffers);

        VkImageMemoryBarrier copyBarrier[] = {
            imageBarrier(colorTarget.image, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL),
            imageBarrier(swapchain.images[imageIndex], 0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        };
        vkCmdPipelineBarrier(commandBuffers, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, ARRAYSIZE(copyBarrier), copyBarrier);

        VkImageCopy copyRegion = {};
        copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.srcSubresource.layerCount = 1;
        copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.dstSubresource.layerCount = 1;
        copyRegion.extent = {swapchain.width, swapchain.height, 1};

        vkCmdCopyImage(commandBuffers, colorTarget.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, swapchain.images[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

        VkImageMemoryBarrier presentBarrier = imageBarrier(swapchain.images[imageIndex], VK_ACCESS_TRANSFER_WRITE_BIT, 0, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
        vkCmdPipelineBarrier(commandBuffers, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, &presentBarrier);

        // VkImageMemoryBarrier renderBeginBarrier = imageBarrier(swapchain.images[imageIndex], 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        // vkCmdPipelineBarrier(commandBuffers, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, &renderBeginBarrier);
        // ============

        // VkImageMemoryBarrier renderEndBarrier = imageBarrier(swapchain.images[imageIndex], VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
        // vkCmdPipelineBarrier(commandBuffers, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, &renderEndBarrier);

        vkCmdWriteTimestamp(commandBuffers, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queryPool, 1);

        VK_CHECK(vkEndCommandBuffer(commandBuffers));

        // VkPipelineStageFlags stageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; // older code
        VkPipelineStageFlags stageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;

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
        presentInfo.pSwapchains = &swapchain.swapchain;
        presentInfo.pImageIndices = &imageIndex;

        VkResult presentResult = vkQueuePresentKHR(queue, &presentInfo);
        if (presentResult != VK_ERROR_OUT_OF_DATE_KHR && presentResult != VK_SUBOPTIMAL_KHR)
        {
            VK_CHECK(presentResult);
        }

        VK_CHECK(vkDeviceWaitIdle(device));

        uint64_t queryResult[2];
        vkGetQueryPoolResults(device, queryPool, 0, ARRAYSIZE(queryResult), sizeof(queryResult), queryResult, sizeof(queryResult[0]), VK_QUERY_RESULT_64_BIT);

        double frameGpuBegin = double(queryResult[0]) * props.limits.timestampPeriod * 1e-6;
        double frameGpuEnd = double(queryResult[1]) * props.limits.timestampPeriod * 1e-6;

        double frameEnd = glfwGetTime() * 1000.0;

        double trianglesPerSec = double(drawCount) * double(mesh.indices.size() / 3) / double((frameGpuEnd - frameGpuBegin) * 1e-3);
        double kittensPerSec = double(drawCount) / double((frameGpuEnd - frameGpuBegin) * 1e-3);
        
        char title[256];
        sprintf(title, "cpu: %.3f ms; gpu: %.3f ms; triangles: %d; meshlets: %d; mesh shading: %s; %.2fB tri/sec, %.1fM kittens/sec", (frameEnd - frameBegin) , (frameGpuEnd - frameGpuBegin), int(mesh.indices.size() / 3), mesh.meshlets.size(), meshShadingEnabled ? "ON" : "OFF", trianglesPerSec * 1e-9, kittensPerSec * 1e-6);
        glfwSetWindowTitle(window, title);
    }

    fprintf(VkLogFile, "\n==================================================================================================================\n");
    fprintf(VkLogFile, "Begin Uninitializing");
    fprintf(VkLogFile, "\n==================================================================================================================\n");

    VK_CHECK(vkDeviceWaitIdle(device));

    if (colorTarget.image){
        destroyImage(colorTarget, device);
    }
    if (depthTarget.image){
        destroyImage(depthTarget, device);
    }
    if (targetFB){
        vkDestroyFramebuffer(device, targetFB, 0);
    }


    if (meshShadingSupported) {
        destroyBuffer(mb, device);
        destroyBuffer(mdb, device);
    }
    
    destroyBuffer(vb, device);
    destroyBuffer(ib, device);
    destroyBuffer(db, device);
    
    destroyBuffer(scratch, device);

    glfwDestroyWindow(window);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffers);
    vkDestroyCommandPool(device, commandPool, 0);

    vkDestroyQueryPool(device, queryPool, 0);

    vkQueueWaitIdle(queue);

    destroySwapchain(device, swapchain);

    vkDestroyPipeline(device, meshPipeline, 0);
    destroyProgram(device, meshProgram);
    
    if (meshShadingSupported) {
        vkDestroyPipeline(device, meshPipelineMS, 0);
        destroyProgram(device, meshProgramMS);
    }

    destroyShaderModule(meshFS, device);
    destroyShaderModule(meshVS, device);

    if (meshShadingSupported) {
        destroyShaderModule(meshletTS, device);
        destroyShaderModule(meshletMS, device);
    }

    vkDestroyRenderPass(device, renderPass, 0);
    vkDestroySemaphore(device, acquireSemaphore, 0);
    vkDestroySemaphore(device, releaseSemaphore, 0);

    vkDestroySurfaceKHR(instance, surface, 0);
    vkDestroyDevice(device, 0);

    destroyDebugUtilsMessenger(instance);

    vkDestroyInstance(instance, 0);

    if (VkLogFile)
    {
        printf("\nLogFile closed \n");
        fclose(VkLogFile);
        VkLogFile = NULL;
    }
    return (0);
}
